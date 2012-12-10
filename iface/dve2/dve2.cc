// -*- coding: utf-8 -*-
// Copyright (C) 2011, 2012 Laboratoire de Recherche et Développement
// de l'Epita (LRDE)
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Spot; see the file COPYING.  If not, write to the Free
// Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.

#include <ltdl.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

// MinGW does not define this.
#ifndef WEXITSTATUS
# define WEXITSTATUS(x) ((x) & 0xff)
#endif

#include "dve2.hh"
#include "misc/hashfunc.hh"
#include "misc/fixpool.hh"
#include "misc/mspool.hh"
#include "misc/intvcomp.hh"
#include "misc/intvcmp2.hh"


namespace spot
{
  namespace
  {

    ////////////////////////////////////////////////////////////////////////
    // DVE2 --ltsmin interface

    typedef struct transition_info {
      int* labels; // edge labels, NULL, or pointer to the edge label(s)
      int  group;  // holds transition group or -1 if unknown
    } transition_info_t;

    typedef void (*TransitionCB)(void *ctx,
				 transition_info_t *transition_info,
				 int *dst);

    struct dve2_interface
    {
      lt_dlhandle handle;	// handle to the dynamic library
      void (*get_initial_state)(void *to);
      int (*have_property)();
      int (*get_successors)(void* m, int *in, TransitionCB, void *arg);

      int (*get_state_variable_count)();
      const char* (*get_state_variable_name)(int var);
      int (*get_state_variable_type)(int var);
      int (*get_state_variable_type_count)();
      const char* (*get_state_variable_type_name)(int type);
      int (*get_state_variable_type_value_count)(int type);
      const char* (*get_state_variable_type_value)(int type, int value);
      int (*get_transition_count)();
      const int* (*get_transition_read_dependencies)(int t);
    };

    ////////////////////////////////////////////////////////////////////////
    // STATE

    struct dve2_state: public state
    {
      dve2_state(int s, fixed_size_pool* p)
	: pool(p), hash_value(0), size(s), count(1)
      {
      }

      void compute_hash()
      {
	hash_value = 0;
	for (int i = 0; i < size; ++i)
	  hash_value = wang32_hash(hash_value ^ vars[i]);
      }

      dve2_state* clone() const
      {
	++count;
	return const_cast<dve2_state*>(this);
      }

      void destroy() const
      {
	if (--count)
	  return;
	pool->deallocate(this);
      }

      size_t hash() const
      {
	return hash_value;
      }

      int compare(const state* other) const
      {
	if (this == other)
	  return 0;
	const dve2_state* o = down_cast<const dve2_state*>(other);
	assert(o);
	if (hash_value < o->hash_value)
	  return -1;
	if (hash_value > o->hash_value)
	  return 1;
	return memcmp(vars, o->vars, size * sizeof(*vars));
      }

    private:

      ~dve2_state()
      {
      }

    public:
      fixed_size_pool* pool;
      size_t hash_value: 32;
      int size: 16;
      mutable unsigned count: 16;
      int vars[0];
    };

    struct dve2_compressed_state: public state
    {
      dve2_compressed_state(int s, multiple_size_pool* p)
	: pool(p), size(s), count(1)
      {
      }

      void compute_hash()
      {
	hash_value = 0;
	for (int i = 0; i < size; ++i)
	  hash_value = wang32_hash(hash_value ^ vars[i]);
      }

      dve2_compressed_state* clone() const
      {
	++count;
	return const_cast<dve2_compressed_state*>(this);
      }

      void destroy() const
      {
	if (--count)
	  return;
	pool->deallocate(this, sizeof(*this) + size * sizeof(*vars));
      }

      size_t hash() const
      {
	return hash_value;
      }

      int compare(const state* other) const
      {
	if (this == other)
	  return 0;
	const dve2_compressed_state* o =
	  down_cast<const dve2_compressed_state*>(other);
	assert(o);
	if (hash_value < o->hash_value)
	  return -1;
	if (hash_value > o->hash_value)
	  return 1;

	if (size < o->size)
	  return -1;
	if (size > o->size)
	  return 1;

	return memcmp(vars, o->vars, size * sizeof(*vars));
      }

    private:

      ~dve2_compressed_state()
      {
      }

    public:
      multiple_size_pool* pool;
      size_t hash_value: 32;
      int size: 16;
      mutable unsigned count: 16;
      int vars[0];
    };

    ////////////////////////////////////////////////////////////////////////
    // CALLBACK FUNCTION for transitions.

    struct callback_context
    {
      typedef std::list<state*> transitions_t;
      transitions_t transitions;
      int state_size;
      void* pool;
      int* compressed;
      void (*compress)(const int*, size_t, int*, size_t&);
      transition_info_t* trans_info;


      ~callback_context()
      {
	callback_context::transitions_t::const_iterator it;
	for (it = transitions.begin(); it != transitions.end(); ++it)
	  (*it)->destroy();
      }
    };

    void transition_callback(void* arg, transition_info_t* i, int *dst)
    {
      callback_context* ctx = static_cast<callback_context*>(arg);
      fixed_size_pool* p = static_cast<fixed_size_pool*>(ctx->pool);
      dve2_state* out =
	new(p->allocate()) dve2_state(ctx->state_size, p);
      memcpy(out->vars, dst, ctx->state_size * sizeof(int));
      out->compute_hash();
      ctx->transitions.push_back(out);

      ctx->trans_info = i;
    }

    void transition_callback_compress(void* arg, transition_info_t*, int *dst)
    {
      callback_context* ctx = static_cast<callback_context*>(arg);
      multiple_size_pool* p = static_cast<multiple_size_pool*>(ctx->pool);

      size_t csize = ctx->state_size * 2;
      ctx->compress(dst, ctx->state_size, ctx->compressed, csize);

      void* mem = p->allocate(sizeof(dve2_compressed_state)
			      + sizeof(int) * csize);
      dve2_compressed_state* out = new(mem) dve2_compressed_state(csize, p);
      memcpy(out->vars, ctx->compressed, csize * sizeof(int));
      out->compute_hash();
      ctx->transitions.push_back(out);
    }

    ////////////////////////////////////////////////////////////////////////
    // SUCC_ITERATOR

    class dve2_succ_iterator: public kripke_succ_iterator
    {
    public:

      dve2_succ_iterator(const callback_context* cc,
			 bdd cond)
	: kripke_succ_iterator(cond), cc_(cc)
      {
      }

      ~dve2_succ_iterator()
      {
	delete cc_;
      }

      virtual
      void first()
      {
	it_ = cc_->transitions.begin();
      }

      virtual
      void next()
      {
	++it_;
      }

      virtual
      bool done() const
      {
	return it_ == cc_->transitions.end();
      }

      virtual
      state* current_state() const
      {
	return (*it_)->clone();
      }

    private:
      const callback_context* cc_;
      callback_context::transitions_t::const_iterator it_;
    };

    ////////////////////////////////////////////////////////////////////////
    // POR iterator

    struct trans
    {
      trans(int i, int* d):
	id(i),
	dst(d)
      {
      }

      int id;
      int* dst;
    };

    struct por_callback
    {
      por_callback(unsigned ss):
	tr(),
	state_size(ss)
      {
      }

      void clear()
      {
	for (std::list<trans>::iterator it = tr.begin();
	     it != tr.end();)
	{
	  int* st = it->dst;
	  ++it;
	  delete[] st;
	}

	tr.clear();
      }

      std::list<trans> tr;
      unsigned state_size;
    };

    void fill_trans_callback(void* arg, transition_info_t* info, int* dst)
    {
      assert(arg);

      por_callback* pc = static_cast<por_callback*>(arg);

      int* tmp = new int[4 * sizeof(int)];
      memcpy(tmp, dst, 4 * sizeof(int));

      pc->tr.push_back(trans (info->group, tmp));
    }

    unsigned my_hash(const int* in, unsigned size)
    {
      unsigned res = 0;
      for (unsigned i = 0; i < size; ++i)
	res = wang32_hash(res ^ in[i]);

      return res;
    }

    class one_state_iterator: public kripke_succ_iterator
    {
    public:
      one_state_iterator(const dve2_state* state, bdd cond):
	kripke_succ_iterator(cond),
	state_(state),
	done_(false)
      {
      }

      virtual
      void first()
      {
	//nothing to do here ;)
      }

      virtual
      void next()
      {
	done_ = true;
      }

      virtual
      bool done() const
      {
	return done_;
      }

      virtual
      state* current_state() const
      {
	return state_->clone();
      }

    protected:
      const dve2_state* state_;
      bool done_;
    };

    ////////////////////////////////////////////////////////////////////////
    // PREDICATE EVALUATION

    typedef enum { OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE } relop;

    struct one_prop
    {
      int var_num;
      relop op;
      int val;
      int bddvar;  // if "var_num op val" is true, output bddvar,
		   // else its negation
    };
    typedef std::vector<one_prop> prop_set;


    struct var_info
    {
      int num;
      int type;
    };


    int
    convert_aps(const ltl::atomic_prop_set* aps,
		const dve2_interface* d,
		bdd_dict* dict,
		const ltl::formula* dead,
		prop_set& out)
    {
      int errors = 0;

      int state_size = d->get_state_variable_count();
      typedef std::map<std::string, var_info> val_map_t;
      val_map_t val_map;

      for (int i = 0; i < state_size; ++i)
	{
	  const char* name = d->get_state_variable_name(i);
	  int type = d->get_state_variable_type(i);
	  var_info v = { i , type };
	  val_map[name] = v;
	}

      int type_count = d->get_state_variable_type_count();
      typedef std::map<std::string, int> enum_map_t;
      std::vector<enum_map_t> enum_map(type_count);
      for (int i = 0; i < type_count; ++i)
	{
	  int enum_count = d->get_state_variable_type_value_count(i);
	  for (int j = 0; j < enum_count; ++j)
	    enum_map[i]
	      .insert(std::make_pair(d->get_state_variable_type_value(i, j),
				     j));
	}

      for (ltl::atomic_prop_set::const_iterator ap = aps->begin();
	   ap != aps->end(); ++ap)
	{
	  if (*ap == dead)
	    continue;

	  std::string str = (*ap)->name();
	  const char* s = str.c_str();

	  // Skip any leading blank.
	  while (*s && (*s == ' ' || *s == '\t'))
	    ++s;
	  if (!*s)
	    {
	      std::cerr << "Proposition `" << str
			<< "' cannot be parsed." << std::endl;
	      ++errors;
	      continue;
	    }


	  char* name = (char*) malloc(str.size() + 1);
	  char* name_p = name;
	  char* lastdot = 0;
	  while (*s && (*s != '=') && *s != '<' && *s != '!'  && *s != '>')
	    {

	      if (*s == ' ' || *s == '\t')
		++s;
	      else
		{
		  if (*s == '.')
		    lastdot = name_p;
		  *name_p++ = *s++;
		}
	    }
	  *name_p = 0;

	  if (name == name_p)
	    {
	      std::cerr << "Proposition `" << str
			<< "' cannot be parsed." << std::endl;
	      free(name);
	      ++errors;
	      continue;
	    }

	  // Lookup the name
	  val_map_t::const_iterator ni = val_map.find(name);
	  if (ni == val_map.end())
	    {
	      // We may have a name such as X.Y.Z
	      // If it is not a known variable, it might mean
	      // an enumerated variable X.Y with value Z.
	      if (lastdot)
		{
		  *lastdot++ = 0;
		  ni = val_map.find(name);
		}

	      if (ni == val_map.end())
		{
		  std::cerr << "No variable `" << name
			    << "' found in model (for proposition `"
			    << str << "')." << std::endl;
		  free(name);
		  ++errors;
		  continue;
		}

	      // We have found the enumerated variable, and lastdot is
	      // pointing to its expected value.
	      int type_num = ni->second.type;
	      enum_map_t::const_iterator ei = enum_map[type_num].find(lastdot);
	      if (ei == enum_map[type_num].end())
		{
		  std::cerr << "No state `" << lastdot
			    << "' known for variable `"
			    << name << "'." << std::endl;
		  std::cerr << "Possible states are:";
		  for (ei = enum_map[type_num].begin();
		       ei != enum_map[type_num].end(); ++ei)
		    std::cerr << " " << ei->first;
		  std::cerr << std::endl;

		  free(name);
		  ++errors;
		  continue;
		}

	      // At this point, *s should be 0.
	      if (*s)
		{
		  std::cerr << "Trailing garbage `" << s
			    << "' at end of proposition `"
			    << str << "'." << std::endl;
		  free(name);
		  ++errors;
		  continue;
		}

	      // Record that X.Y must be equal to Z.
	      int v = dict->register_proposition(*ap, d);
	      one_prop p = { ni->second.num, OP_EQ, ei->second, v };
	      out.push_back(p);
	      free(name);
	      continue;
	    }

	  int var_num = ni->second.num;

	  if (!*s)		// No operator?  Assume "!= 0".
	    {
	      int v = dict->register_proposition(*ap, d);
	      one_prop p = { var_num, OP_NE, 0, v };
	      out.push_back(p);
	      free(name);
	      continue;
	    }

	  relop op;

	  switch (*s)
	    {
	    case '!':
	      if (s[1] != '=')
		goto report_error;
	      op = OP_NE;
	      s += 2;
	      break;
	    case '=':
	      if (s[1] != '=')
		goto report_error;
	      op = OP_EQ;
	      s += 2;
	      break;
	    case '<':
	      if (s[1] == '=')
		{
		  op = OP_LE;
		  s += 2;
		}
	      else
		{
		  op = OP_LT;
		  ++s;
		}
	      break;
	    case '>':
	      if (s[1] == '=')
		{
		  op = OP_GE;
		  s += 2;
		}
	      else
		{
		  op = OP_GT;
		  ++s;
		}
	      break;
	    default:
	    report_error:
	      std::cerr << "Unexpected `" << s
			<< "' while parsing atomic proposition `" << str
			<< "'." << std::endl;
	      ++errors;
	      free(name);
	      continue;
	    }

	  while (*s && (*s == ' ' || *s == '\t'))
	    ++s;

	  int val = 0; // Initialize to kill a warning from old compilers.
	  int type_num = ni->second.type;
	  if (type_num == 0 || (*s >= '0' && *s <= '9') || *s == '-')
	    {
	      char* s_end;
	      val = strtol(s, &s_end, 10);
	      if (s == s_end)
		{
		  std::cerr << "Failed to parse `" << s
			    << "' as an integer." << std::endl;
		  ++errors;
		  free(name);
		  continue;
		}
	      s = s_end;
	    }
	  else
	    {
	      // We are in a case such as P_0 == S, trying to convert
	      // the string S into an integer.
	      const char* end = s;
	      while (*end && *end != ' ' && *end != '\t')
		++end;
	      std::string st(s, end);

	      // Lookup the string.
	      enum_map_t::const_iterator ei = enum_map[type_num].find(st);
	      if (ei == enum_map[type_num].end())
		{
		  std::cerr << "No state `" << st
			    << "' known for variable `"
			    << name << "'." << std::endl;
		  std::cerr << "Possible states are:";
		  for (ei = enum_map[type_num].begin();
		       ei != enum_map[type_num].end(); ++ei)
		    std::cerr << " " << ei->first;
		  std::cerr << std::endl;

		  free(name);
		  ++errors;
		  continue;
		}
	      s = end;
	      val = ei->second;
	    }

	  free(name);

	  while (*s && (*s == ' ' || *s == '\t'))
	    ++s;
	  if (*s)
	    {
	      std::cerr << "Unexpected `" << s
			<< "' while parsing atomic proposition `" << str
			<< "'." << std::endl;
		  ++errors;
		  continue;
	    }


	  int v = dict->register_proposition(*ap, d);
	  one_prop p = { var_num, op, val, v };
	  out.push_back(p);
	}

      return errors;
    }


    ////////////////////////////////////////////////////////////////////////
    // AMPLE SET

    bool invisible(const int* start, const int* to,
		   unsigned state_size, const prop_set* ps)
    {
      assert (to);
      for (unsigned i = 0; i < state_size; ++i)
	{
	  //labels modified by the transition
	  if (start[i] != to[i])
	    {
	      for (prop_set::const_iterator it = ps->begin();
		   it != ps->end(); ++it)
		{
		  if (it->var_num == (int) i)
		    return false;
		}
	    }
	}

      return true;
    }


    std::list<dve2_state*> my_copy (const std::list<trans>& in,
				    fixed_size_pool* pool,
				    unsigned state_size)
      {
	std::list<dve2_state*> res;
	 
	for (std::list<trans>::const_iterator it = in.begin ();
	     it != in.end (); ++it)
	{
	  dve2_state* nstate =
	    new(pool->allocate ()) dve2_state(state_size, pool);
	  memcpy (nstate->vars, it->dst, state_size * sizeof(int));
	  res.push_back (nstate);
	}

	return res;
      }

    bool check_c1 (const dve2_interface* d,
		   unsigned p,
		   const std::vector<int>& processes,
		   const std::vector<std::list<trans> >& procT)		     
    {
      for (int i = 0; i < d->get_state_variable_count (); ++i)
	{
	  if (i == processes[p])
	    continue;
	  
	  for (std::list<trans>::const_iterator it = procT[p].begin ();
	       it != procT[p].end (); ++it)
	    {
	      //std::cerr << "id = " << it->id << std::endl;
	      const int* dep =
		d->get_transition_read_dependencies (it->id);
	      if (dep[i])
		return false;
	    }
	}

      return true;
    }

    bool check_c2 (const int* s, std::list<trans>& t,
		   unsigned state_size, const prop_set* ps)
    {
      for (std::list<trans>::const_iterator it = t.begin ();
	   it != t.end (); ++it)
	{
	  if (!invisible (s, it->dst, state_size, ps))
	    return false;
	}
      
	return true;
      }
      
      bool check_c3 (std::list<trans>& t,
		     const std::set<unsigned>& visited,
		     unsigned state_size)
      {
	for (std::list<trans>::const_iterator it = t.begin ();
	     it != t.end (); ++it)
	  if (visited.find (my_hash(it->dst, state_size)) != visited.end ())
	    return false;

	return true;
      }

    class ample_iterator: public kripke_succ_iterator
    {
    public:
      ample_iterator(const int* state,
		     bdd cond,
		     const dve2_interface* d,
		     const por_callback& pc,
		     const std::set<unsigned>& visited,
		     const std::vector<int>& processes,
		     const prop_set* ps,
		     fixed_size_pool* pool)
	: kripke_succ_iterator(cond)
	  , state_size_ (pc.state_size)
	{
	  std::vector<std::list<trans> >
	    procT (processes.size (), std::list<trans> ());

	  for (std::list<trans>::const_iterator it = pc.tr.begin ();
	       it != pc.tr.end (); ++it)
	  {
	    for (unsigned p = 0; p < processes.size (); ++p)
	      if (state[processes[p]] != it->dst[processes[p]])
		procT[p].push_back (*it);
	  }
	  
	  for (unsigned p = 0;
	       p < processes.size () && !procT[p].empty (); ++p)
	  {
	    bool c1 = check_c1 (d, p, processes, procT);
	    bool c2 = check_c2 (state, procT[p], pc.state_size, ps);
	    bool c3 = check_c3 (procT[p], visited, pc.state_size);

	    //std::cerr << "C1 = " << c1 << " C2 = " << c2 << " C3 = " << c3 << std::endl;
	    if (c1 && c2 && c3)
	      {
		next_ = my_copy(procT[p], pool, pc.state_size);
		break;
	      }
	  }

	  if (next_.empty ())
	  {
	    std::cerr << "no ample set for this state" << std::endl;
	    next_ = my_copy(pc.tr, pool, pc.state_size);
	  }
	}

      virtual
      void first()
      {
	cur_ = next_.begin ();
      }

      virtual
      void next()
      {
	if (cur_ != next_.end ())
	  ++cur_;
      }
      
      virtual
      bool done() const
      {
	return cur_ == next_.end ();
      }

      virtual
      state* current_state() const
      {
	return (*cur_)->clone ();
      }
      
    protected:
      unsigned state_size_;
      std::list<dve2_state*> next_;
      std::list<dve2_state*>::const_iterator cur_;
    };
  ////////////////////////////////////////////////////////////////////////
  // KRIPKE
    
    struct mycmp
    {
      bool operator() (const int* l, const int* r) const
	{
	  for (int i = 0; i < 4; ++i)
	    if (l[i] && !r[i])
	      return true;

	  return false;
	}
    };

    class dve2_kripke: public kripke
    {
    protected:
      bool internal(const trans& t, int p) const
      {
	const int* dep = d_->get_transition_read_dependencies(t.id);
	
	for (int i = 0; i < d_->get_state_variable_count(); ++i)
	  {
	    if (dep[i] && i != processes_.at(p))
	      return false;
	  }
	
	return true;
      }

      bool only_one_enabled(const int* s, int p,
			    const std::list<trans>& tr, trans& t,
			    const std::set<unsigned>& visited) const
      {
	int cpt = 0;

	for (std::list<trans>::const_iterator it = tr.begin();
	     it != tr.end(); ++it)
	  {
	    if (visited.find(my_hash(it->dst, state_size_)) == visited.end() &&
		s[processes_[p]] != it->dst[processes_[p]])
	      {
		++cpt;
		t = *it;
	      }

	    if (cpt > 1)
	      return false;
	  }

	return cpt == 1;
      }

      bool deterministic(const int* s, unsigned p, const std::list<trans>& tr,
			 trans& res, const std::set<unsigned>& visited) const
      {
	if (only_one_enabled (s, p, tr, res, visited))
	  {
	    if (invisible (s, res.dst, state_size_, ps_) && internal (res, p))
	      return true;
	  }
	
	return false;
      }
      
      const dve2_state*
      phase1(const int* in) const
      {
	const int* s = in;
	std::set<unsigned> visited;
	visited.insert(my_hash(in, state_size_));

	por_callback pc(state_size_);

	std::list<trans> tr;
	d_->get_successors (0, const_cast<int*> (s),
			    fill_trans_callback, &pc);

	trans t(-1, 0);
	for (unsigned p = 0; p < processes_.size(); ++p)
	  {
	    while (deterministic(s, p, pc.tr, t, visited))
	      {
		if (s != in)
		  delete[] s;

		s = new int[state_size_ * sizeof(int)];
		memcpy(const_cast<int*>(s),
		       t.dst, state_size_ * sizeof(int));

		pc.clear();

		if (visited.find(my_hash(s, state_size_)) != visited.end())
		  break;
		else
		  {
		    visited.insert(my_hash(s, state_size_));
		    d_->get_successors(0, const_cast<int*>(s),
				       fill_trans_callback, &pc);
		  }
	      }


	    if (!pc.tr.empty())
	      pc.clear();
	  }

	if (s == in)
	  return 0;

	fixed_size_pool* p = const_cast<fixed_size_pool*>(&statepool_);
	dve2_state* res = new(p->allocate()) dve2_state(state_size_, 0);
	memcpy(res->vars, s, state_size_ * sizeof(int));

	return res;
      }

    public:

      dve2_kripke(const dve2_interface* d, bdd_dict* dict, const prop_set* ps,
		  const ltl::formula* dead, int compress, bool por, bool ample)
	: d_(d),
	  state_size_(d_->get_state_variable_count()),
	  dict_(dict), ps_(ps),
	  compress_(compress == 0 ? 0
		    : compress == 1 ? int_array_array_compress
		    : int_array_array_compress2),
	  decompress_(compress == 0 ? 0
		      : compress == 1 ? int_array_array_decompress
		      : int_array_array_decompress2),
	  uncompressed_(compress ? new int[state_size_ + 30] : 0),
	  compressed_(compress ? new int[state_size_ * 2] : 0),
	  statepool_(compress ? sizeof(dve2_compressed_state) :
		     (sizeof(dve2_state) + state_size_ * sizeof(int))),
	  state_condition_last_state_(0), state_condition_last_cc_(0),
	  por_(por),
	  even_(true),
	  ample_(ample),
	  visited_ (),
	  cur_process_(0)
	{
	  vname_ = new const char*[state_size_];
	  format_filter_ = new bool[state_size_];
	  for (int i = 0; i < state_size_; ++i)
	    {

	      vname_[i] = d_->get_state_variable_name(i);
	      // We don't want to print variables that can take a single
	      // value (e.g. process with a single state) to shorten the
	      // output.
	      int type = d->get_state_variable_type(i);
	      format_filter_[i] =
		(d->get_state_variable_type_value_count(type) != 1);
	    }

	  // Register the "dead" proposition.  There are three cases to
	  // consider:
	  //  * If DEAD is "false", it means we are not interested in finite
	  //    sequences of the system.
	  //  * If DEAD is "true", we want to check finite sequences as well
	  //    as infinite sequences, but do not need to distinguish them.
	  //  * If DEAD is any other string, this is the name a property
	  //    that should be true when looping on a dead state, and false
	  //    otherwise.
	  // We handle these three cases by setting ALIVE_PROP and DEAD_PROP
	  // appropriately.  ALIVE_PROP is the bdd that should be ANDed
	  // to all transitions leaving a live state, while DEAD_PROP should
	  // be ANDed to all transitions leaving a dead state.
	  if (dead == ltl::constant::false_instance())
	    {
	      alive_prop = bddtrue;
	      dead_prop = bddfalse;
	    }
	  else if (dead == ltl::constant::true_instance())
	    {
	      alive_prop = bddtrue;
	      dead_prop = bddtrue;
	    }
	  else
	    {
	      int var = dict->register_proposition(dead, d_);
	      dead_prop = bdd_ithvar(var);
	      alive_prop = bdd_nithvar(var);
	    }


	  //Find the different processes for the partial order reduction
	  for (int i = 0; i < d->get_state_variable_type_count (); ++i)
	    {
	      std::string tmp (d->get_state_variable_type_name (i));

	      //ugly, find a better way to find the number of processes.
	      if (tmp != "byte" && tmp != "integer")
		processes_.push_back (i);
	    }
	}

      ~dve2_kripke()
      {
	delete[] format_filter_;
	delete[] vname_;
	if (compress_)
	  {
	    delete[] uncompressed_;
	    delete[] compressed_;
	  }
	lt_dlclose(d_->handle);

	dict_->unregister_all_my_variables(d_);

	delete d_;
	delete ps_;
	lt_dlexit();

	if (state_condition_last_state_)
	  state_condition_last_state_->destroy();
	delete state_condition_last_cc_; // Might be 0 already.
      }

      virtual
      state* get_init_state() const
      {
	if (compress_)
	  {
	    d_->get_initial_state(uncompressed_);
	    size_t csize = state_size_ * 2;
	    compress_(uncompressed_, state_size_, compressed_, csize);

	    multiple_size_pool* p =
	      const_cast<multiple_size_pool*>(&compstatepool_);
	    void* mem = p->allocate(sizeof(dve2_compressed_state)
				    + sizeof(int) * csize);
	    dve2_compressed_state* res = new(mem)
	      dve2_compressed_state(csize, p);
	    memcpy(res->vars, compressed_, csize * sizeof(int));
	    res->compute_hash();
	    return res;
	  }
	else
	  {
	    fixed_size_pool* p = const_cast<fixed_size_pool*>(&statepool_);
	    dve2_state* res = new(p->allocate()) dve2_state(state_size_, p);
	    d_->get_initial_state(res->vars);
	    res->compute_hash();
	    return res;
	  }
      }

      bdd
      compute_state_condition_aux(const int* vars) const
      {
	bdd res = bddtrue;
	for (prop_set::const_iterator i = ps_->begin();
	     i != ps_->end(); ++i)
	  {
	    int l = vars[i->var_num];
	    int r = i->val;

	    bool cond = false;
	    switch (i->op)
	      {
	      case OP_EQ:
		cond = (l == r);
		break;
	      case OP_NE:
		cond = (l != r);
		break;
	      case OP_LT:
		cond = (l < r);
		break;
	      case OP_GT:
		cond = (l > r);
		break;
	      case OP_LE:
		cond = (l <= r);
		break;
	      case OP_GE:
		cond = (l >= r);
		break;
	      }

	    if (cond)
	      res &= bdd_ithvar(i->bddvar);
	    else
	      res &= bdd_nithvar(i->bddvar);
	  }
	return res;
      }

      callback_context* build_cc(const int* vars, int& t) const
      {
	callback_context* cc = new callback_context;
	cc->state_size = state_size_;
	cc->pool =
	  const_cast<void*>(compress_
			    ? static_cast<const void*>(&compstatepool_)
			    : static_cast<const void*>(&statepool_));
	cc->compress = compress_;
	cc->compressed = compressed_;
	t = d_->get_successors(0, const_cast<int*>(vars),
			       compress_
			       ? transition_callback_compress
			       : transition_callback,
			       cc);
	assert((unsigned)t == cc->transitions.size());
	return cc;
      }

      bdd
      compute_state_condition(const state* st) const
      {
	// If we just computed it, don't do it twice.
	if (st == state_condition_last_state_)
	  return state_condition_last_cond_;

	if (state_condition_last_state_)
	  {
	    state_condition_last_state_->destroy();
	    delete state_condition_last_cc_; // Might be 0 already.
	    state_condition_last_cc_ = 0;
	  }

	const int* vars = get_vars(st);

	bdd res = compute_state_condition_aux(vars);
	int t;
	callback_context* cc = build_cc(vars, t);

	if (t)
	  {
	    res &= alive_prop;
	  }
	else
	  {
	    res &= dead_prop;

	    // Add a self-loop to dead-states if we care about these.
	    if (res != bddfalse)
	      cc->transitions.push_back(st->clone());
	  }

	state_condition_last_cc_ = cc;
	state_condition_last_cond_ = res;
	state_condition_last_state_ = st->clone();

	return res;
      }

      const int*
      get_vars(const state* st) const
      {
	const int* vars;
	if (compress_)
	  {
	    const dve2_compressed_state* s =
	      down_cast<const dve2_compressed_state*>(st);
	    assert(s);
	    
	    decompress_(s->vars, s->size, uncompressed_, state_size_);
	      vars = uncompressed_;
	  }
	else
	  {
	    const dve2_state* s = down_cast<const dve2_state*>(st);
	    assert(s);
	    vars = s->vars;
	  }
	return vars;
      }
      
      virtual
      kripke_succ_iterator*
      succ_iter(const state* local_state,
		const state*, const tgba*) const
      {
	even_ = !even_;
	if (por_ && even_)
	  {
	    const int* vstate = get_vars(local_state);
	    assert (vstate);
	    bdd scond = compute_state_condition_aux(vstate);
	    const dve2_state* s = phase1(vstate);
	    
	    if (s)
	      return new one_state_iterator(s, scond);
	  }
	else if (ample_)
	  {
	    const int* vstate = get_vars(local_state);
	    assert (vstate);
	    
	    visited_.insert (my_hash (vstate, state_size_));
	    bdd scond = compute_state_condition_aux(vstate);
	    
	    por_callback pc(state_size_);
	    
	    d_->get_successors (0, const_cast<int*> (vstate),
				fill_trans_callback, &pc);
	    
	    return new ample_iterator (vstate,
				       scond,
				       d_,
				       pc,
				       visited_,
				       processes_,
				       ps_,
				       const_cast<fixed_size_pool*> (&statepool_));
	    }
	// This may also compute successors in state_condition_last_cc
	bdd scond = compute_state_condition(local_state);
	
	callback_context* cc;
	if (state_condition_last_cc_)
	  {
	    cc = state_condition_last_cc_;
	    state_condition_last_cc_ = 0; // Now owned by the iterator.
	  }
	else
	  {
	    int t;
	    cc = build_cc(get_vars(local_state), t);
	    
	    // Add a self-loop to dead-states if we care about these.
	    if (t == 0 && scond != bddfalse)
	      cc->transitions.push_back(local_state->clone());
	  }
	
	return new dve2_succ_iterator(cc, scond);
      }
      
      virtual
      bdd
      state_condition(const state* st) const
      {
	return compute_state_condition(st);
      }

      virtual
      std::string format_state(const state *st) const
      {
	const int* vars = get_vars(st);

	std::stringstream res;

	if (state_size_ == 0)
	  return "empty state";

	int i = 0;
	for (;;)
	  {
	    if (!format_filter_[i])
	      {
		++i;
		continue;
	      }
	    res << vname_[i] << "=" << vars[i];
	    ++i;
	    if (i == state_size_)
	      break;
	    res << ", ";
	  }
	return res.str();
      }
      
      virtual
      spot::bdd_dict* get_dict() const
      {
	return dict_;
      }
      
    private:
      const dve2_interface* d_;
      int state_size_;
      bdd_dict* dict_;
      const char** vname_;
      bool* format_filter_;
      const prop_set* ps_;
      bdd alive_prop;
      bdd dead_prop;
      void (*compress_)(const int*, size_t, int*, size_t&);
      void (*decompress_)(const int*, size_t, int*, size_t);
      int* uncompressed_;
      int* compressed_;
      fixed_size_pool statepool_;
      multiple_size_pool compstatepool_;

      // This cache is used to speedup repeated calls to state_condition()
      // and get_succ().
      // If state_condition_last_state_ != 0, then state_condition_last_cond_
      // contain its (recently computed) condition.  If additionally
      // state_condition_last_cc_ != 0, then it contains the successors.
      mutable const state* state_condition_last_state_;
      mutable bdd state_condition_last_cond_;
      mutable callback_context* state_condition_last_cc_;
      bool por_;
      mutable bool even_;
      bool ample_;
      mutable std::set<unsigned> visited_;
      unsigned cur_process_;
      std::vector<int> processes_;
    };
  }


  ////////////////////////////////////////////////////////////////////////////
  // LOADER


  // Call divine to compile "foo.dve" as "foo.dve2C" if the latter
  // does not exist already or is older.
  bool
  compile_dve2(std::string& filename, bool verbose)
  {

    std::string command = "divine compile --ltsmin " + filename;

    struct stat s;
    if (stat(filename.c_str(), &s) != 0)
      {
	if (verbose)
	  {
	    std::cerr << "Cannot open " << filename << std::endl;
	    return true;
	  }
      }

    std::string old = filename;
    filename += "2C";

    // Remove any directory, because the new file will
    // be compiled in the current directory.
    size_t pos = filename.find_last_of("/\\");
    if (pos != std::string::npos)
      filename = "./" + filename.substr(pos + 1);

    struct stat d;
    if (stat(filename.c_str(), &d) == 0)
      if (s.st_mtime < d.st_mtime)
	// The dve2C is up-to-date, no need to recompile it.
	return false;

    int res = system(command.c_str());
    if (res)
      {
	if (verbose)
	  std::cerr << "Execution of `" << command.c_str()
		    << "' returned exit code " << WEXITSTATUS(res)
		    << "." << std::endl;
	return true;
      }
    return false;
  }

  kripke*
  load_dve2(const std::string& file_arg, bdd_dict* dict,
	    const ltl::atomic_prop_set* to_observe,
	    const ltl::formula* dead,
	    int compress,
	    bool verbose,
    	    bool por,
	    bool ample)
  {
    std::string file;
    if (file_arg.find_first_of("/\\") != std::string::npos)
      file = file_arg;
    else
      file = "./" + file_arg;

    std::string ext = file.substr(file.find_last_of("."));
    if (ext == ".dve")
      {
	if (compile_dve2(file, verbose))
	  {
	    if (verbose)
	      std::cerr << "Failed to compile `" << file_arg
			<< "'." << std::endl;
	    return 0;
	  }
      }
    else if (ext != ".dve2C")
      {
	if (verbose)
	  std::cerr << "Unknown extension `" << ext
		    << "'.  Use `.dve' or `.dve2C'." << std::endl;
	return 0;
      }

    if (lt_dlinit())
      {
	if (verbose)
	  std::cerr << "Failed to initialize libltdl." << std::endl;
	return 0;
      }

    lt_dlhandle h = lt_dlopen(file.c_str());
    if (!h)
      {
	if (verbose)
	  std::cerr << "Failed to load `" << file << "'." << std::endl;
	lt_dlexit();
	return 0;
      }

    dve2_interface* d = new dve2_interface;
    d->handle = h;

    d->get_initial_state = (void (*)(void*))
      lt_dlsym(h, "get_initial_state");
    d->have_property = (int (*)())
      lt_dlsym(h, "have_property");
    d->get_successors = (int (*)(void*, int*, TransitionCB, void*))
      lt_dlsym(h, "get_successors");
    d->get_state_variable_count = (int (*)())
      lt_dlsym(h, "get_state_variable_count");
    d->get_state_variable_name = (const char* (*)(int))
      lt_dlsym(h, "get_state_variable_name");
    d->get_state_variable_type = (int (*)(int))
      lt_dlsym(h, "get_state_variable_type");
    d->get_state_variable_type_count = (int (*)())
      lt_dlsym(h, "get_state_variable_type_count");
    d->get_state_variable_type_name = (const char* (*)(int))
      lt_dlsym(h, "get_state_variable_type_name");
    d->get_state_variable_type_value_count = (int (*)(int))
      lt_dlsym(h, "get_state_variable_type_value_count");
    d->get_state_variable_type_value = (const char* (*)(int, int))
      lt_dlsym(h, "get_state_variable_type_value");
    d->get_transition_count = (int (*)())
      lt_dlsym(h, "get_transition_count");
    d->get_transition_read_dependencies = (const int* (*) (int))
      lt_dlsym(h, "get_transition_read_dependencies");

    if (!(d->get_initial_state
	  && d->have_property
	  && d->get_successors
	  && d->get_state_variable_count
	  && d->get_state_variable_name
	  && d->get_state_variable_type
	  && d->get_state_variable_type_count
	  && d->get_state_variable_type_name
	  && d->get_state_variable_type_value_count
	  && d->get_state_variable_type_value
	  && d->get_transition_count
	  && d->get_transition_read_dependencies))
      {
	if (verbose)
	  std::cerr << "Failed to resolve some symbol while loading `"
		    << file << "'" << std::endl;
	delete d;
	lt_dlexit();
	return 0;
      }

    if (d->have_property())
      {
	if (verbose)
	  std::cerr << "Model with an embedded property are not supported."
		    << std::endl;
	delete d;
	lt_dlexit();
	return 0;
      }

    prop_set* ps = new prop_set;
    int errors = convert_aps(to_observe, d, dict, dead, *ps);
    if (errors)
      {
	delete ps;
	dict->unregister_all_my_variables(d);
	delete d;
	lt_dlexit();
	return 0;
      }

    return new dve2_kripke(d, dict, ps, dead, compress, por, ample);
  }
}
