// -*- coding: utf-8 -*-
// Copyright (C) 2011, 2012 Laboratoire de Recherche et Développement
// de l'Epita (LRDE)
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <ltdl.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

// MinGW does not define this.
#ifndef WEXITSTATUS
# define WEXITSTATUS(x) ((x) & 0xff)
#endif


#include "ltlast/atomic_prop.hh"
#include "ltlenv/defaultenv.hh"

#include "dve2.hh"
#include "fasttgba/fasttgba_state.hh"
#include "fasttgba/fasttgba_product.hh"
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
    };

    ////////////////////////////////////////////////////////////////////////
    // STATE

    struct dve2_state: public fasttgba_state
    {
      dve2_state(int s, fixed_size_pool* p)
    	: pool(p), size(s), count(1)
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

      int compare(const fasttgba_state* other) const
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

      void *
      external_information() const
      {
	assert(false);
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

    ////////////////////////////////////////////////////////////////////////
    // CALLBACK FUNCTION for transitions.

    struct callback_context
    {
      typedef std::list<fasttgba_state*> transitions_t;
      transitions_t transitions;
      int state_size;
      void* pool;
      int* compressed;
      void (*compress)(const int*, size_t, int*, size_t&);

      ~callback_context()
      {
    	callback_context::transitions_t::const_iterator it;
    	for (it = transitions.begin(); it != transitions.end(); ++it)
    	  (*it)->destroy();
      }
    };

    void transition_callback(void* arg, transition_info_t*, int *dst)
    {
      callback_context* ctx = static_cast<callback_context*>(arg);
      fixed_size_pool* p = static_cast<fixed_size_pool*>(ctx->pool);
      dve2_state* out =
    	new(p->allocate()) dve2_state(ctx->state_size, p);
      memcpy(out->vars, dst, ctx->state_size * sizeof(int));
      out->compute_hash();
      ctx->transitions.push_back(out);
    }

    ////////////////////////////////////////////////////////////////////////
    // SUCC_ITERATOR

    class dve2_succ_iterator: public fasttgba_succ_iterator
    {
    public:

      dve2_succ_iterator(const callback_context* cc,
    			 const cube cond):
	cond_(cond), cc_(cc)
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
      fasttgba_state* current_state() const
      {
    	return (*it_)->clone();
      }

      cube
      current_condition() const
      {
	return cond_;
      }

      markset
      current_acceptance_marks() const
      {
	// Can do this because because the product
	// must be instanciate according the fact that
	// this class is a kripke.
	assert(false);
      }


    private:
      const cube cond_;
      const callback_context* cc_;
      callback_context::transitions_t::const_iterator it_;
    };

    ////////////////////////////////////////////////////////////////////////
    // PREDICATE EVALUATION

    typedef enum { OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE } relop;

    // Structure for complex atomic proposition
    struct one_prop
    {
      int var_num;
      relop op;
      int val;
    };

    // Data structure to store complex atomic propositions
    typedef std::vector<one_prop> prop_set;

    //
    // Register all atomic propositions that are inside the kripke
    //
    int
    convert_aps(spot::ap_dict& aps,
		const dve2_interface* d,
		fasttgba* kripke,
		bool verbose)
    {
      int state_size = d->get_state_variable_count();

      if (verbose)
	std::cout << "Atomic Propositions in the Kripke :" << std::endl;

      for (int i = 0; i < state_size; ++i)
    	{
	  // Register for automaton
    	  const char* name = d->get_state_variable_name(i);
	  const ltl::atomic_prop* ap =
	    ltl::atomic_prop::instance(name, spot::ltl::default_environment::instance());
	  //ap->clone();
	  aps.register_ap_for_aut (ap, kripke);
	  ap->destroy();

	  if (verbose)
	    std::cout << "     " << name << std::endl;
    	}
      return 0;
    }

    ////////////////////////////////////////////////////////////////////////
    // KRIPKE

    class dve2_kripke: public fasttgba
    {
    public:

      dve2_kripke(const dve2_interface* d,
		  spot::ap_dict& aps,
		  spot::acc_dict& acc)
	: d_(d),
	  state_size_(d_->get_state_variable_count()),
	  ps_(new prop_set()),
    	  statepool_((sizeof(dve2_state) + state_size_ * sizeof(int))),
	  aps_(aps),
	  acc_(acc)
      {
      }

      ~dve2_kripke()
      {
	lt_dlclose(d_->handle);
	delete d_;
	delete ps_;
     	lt_dlexit();
      }

      virtual
      fasttgba_state* get_init_state() const
      {
	fixed_size_pool* p = const_cast<fixed_size_pool*>(&statepool_);
	dve2_state* res = new(p->allocate()) dve2_state(state_size_, p);
	d_->get_initial_state(res->vars);
	res->compute_hash();
	res->clone();
	return res;
      }


      cube
      compute_state_condition(const int* vars) const
      {
    	cube res(aps_);
	int state_size = d_->get_state_variable_count();
	for (int i = 0; i < state_size; ++i)
	  {
    	    if (vars[i])
    	      res.set_true_var(i);
    	    else
    	      res.set_false_var(i);
     	  }

 	int cpt = state_size;
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
    	      res.set_true_var(cpt);
    	    else
    	      res.set_false_var(cpt);
	    ++cpt;
     	  }
	assert(res.is_valid());
     	return res;
      }

      callback_context* build_cc(const int* vars, int& t) const
      {
    	callback_context* cc = new callback_context();
    	cc->state_size = state_size_;
	cc->pool =
    	   const_cast<void*>(static_cast<const void*>(&statepool_));
    	t = d_->get_successors(0, const_cast<int*>(vars),
    			       transition_callback,
    			       cc);
	assert((unsigned)t == cc->transitions.size());
    	return cc;
      }


      const int*
      get_vars(const fasttgba_state* st) const
      {
	const int* vars;
	const dve2_state* s = down_cast<const  dve2_state*>(st);
	assert(s);
	vars = s->vars;
    	return vars;
      }

      virtual
      dve2_succ_iterator*
      succ_iter(const fasttgba_state* state) const
      {
     	callback_context* cc;
	int t;
	const int *vars = get_vars(state);
	cc = build_cc(vars, t);
	cube scond = compute_state_condition(vars);

	if (!t)
	  {
	    cc->transitions.push_back(state->clone());
	  }

	//assert(t); //??? No deadlock in the model
	// // Add a self-loop to dead-states if we care about these.
	// if (t == 0 && scond != bddfalse)
	//   cc->transitions.push_back(local_state->clone());

     	return new dve2_succ_iterator(cc, scond);
	assert(false);
      }

      virtual
      std::string format_state(const fasttgba_state *st) const
      {
    	const int* vars = get_vars(st);

    	std::stringstream res;

    	if (state_size_ == 0)
    	  return "empty state";

    	int i = 0;
    	for (;;)
    	  {
    	    res << d_->get_state_variable_name(i) << "=" << vars[i];
    	    ++i;
    	    if (i == state_size_)
    	      break;
    	    res << ", ";
    	  }
    	return res.str();
      }

      ap_dict&
      get_dict() const
      {
	return aps_;
      }

      acc_dict&
      get_acc() const
      {
	return acc_;
      }

      markset
      all_acceptance_marks() const
      {
	assert(false);
      }

      unsigned int
      number_of_acceptance_marks() const
      {
	return 0;
      }

      std::string
      transition_annotation(const spot::fasttgba_succ_iterator* succ) const
      {
	return ("#" + succ->current_condition().dump());
      }

      fasttgba_state*
      project_state(const fasttgba_state* ,
		    const fasttgba*) const
      {
	assert(false);
      }

      // --
      // This part is for bein manipulated by the product
      // --

      void add_ap (one_prop p) const
      {
	ps_->push_back(p);
      }


      const dve2_interface*
      get_interface() const
      {
	return d_;
      }

    private:
      const dve2_interface* d_;
      int state_size_;
      const char** vname_;
      prop_set* ps_;
      fixed_size_pool statepool_;
      multiple_size_pool compstatepool_;
      spot::ap_dict& aps_;
      spot::acc_dict& acc_;

    };
  }

  ////////////////////////////////////////////////////////////////////////
  // Product

  void
  fasttgba_kripke_product::match_formula_ap()
  {
    const dve2_kripke* l = down_cast<const dve2_kripke*>(left_);
    assert(l);
    const dve2_interface* d_ = l->get_interface();

    //
    // If the number of Atomic Propositions is the same this
    // means that no other variables have been added: the product
    // only consider variables as True/False value.
    //
    if ((int) get_dict().size() == d_->get_state_variable_count())
      return;

    //
    // Grab the name of states that are used by processes
    //
    int type_count = d_->get_state_variable_type_count();
    typedef std::map<std::string, int> enum_map_t;
    std::vector<enum_map_t> enum_map(type_count);
    for (int i = 0; i < type_count; ++i)
      {
	int enum_count = d_->get_state_variable_type_value_count(i);
	for (int j = 0; j < enum_count; ++j)
	  {
	    enum_map[i]
	      .insert(std::make_pair(d_->get_state_variable_type_value(i, j),
				   j));
	  }
      }

    //
    // Otherwise there are other AP like "P_0.j==2" etc.
    // We have to check if these APs are correct and the
    // kripke must update for every successor these APs.
    //
    for (int i = d_->get_state_variable_count();
    	 i < (int) get_dict().size(); ++i)
      {
	int var_num = 0;

    	const ltl::atomic_prop* ap = get_dict().get(i);
	std::string str = ap->name();
	const char* s = str.c_str();

	// Skip any leading blank.
	while (*s && (*s == ' ' || *s == '\t'))
	  ++s;
	if (!*s)
	  {
	    std::cerr << "Proposition `" << str
		      << "' cannot be parsed." << std::endl;
	    exit(1);
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
	      exit(1);
      	    }

	  // Lookup the name
	  int j;
	  for (j = 0; j < (int) d_->get_state_variable_count(); ++j)
	    {
	      const ltl::atomic_prop* a = get_dict().get(j);
	      std::string sap = a->name();
	      if (sap.compare(name) == 0)
		break;
	    }
	  var_num = j;

	  if (j >= (int) d_->get_state_variable_count())
	    {
	      //
	      // It's a State name P_0.S
	      //
	      if (lastdot)
		{
		  std::string proc_name(name, lastdot);
		  //std::cout << proc_name << std::endl;

		  int ii;
		  for (ii = 0; ii < (int) d_->get_state_variable_count(); ++ii)
		    {
		      const ltl::atomic_prop* a = get_dict().get(ii);
		      std::string sap = a->name();
		      if (sap.compare(proc_name) == 0)
			break;
		    }
		  var_num = ii;

		  int type_num = d_->get_state_variable_type(ii);
		  enum_map_t::const_iterator ei = enum_map[type_num].find(lastdot+1);
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
		      exit(1);
		    }

		  one_prop p = {var_num, OP_EQ, ei->second };
		  l->add_ap(p);
		  continue;
		}
	      //
	      // It's an unknow variable.
	      //
	      else
		{
		  std::cerr << "Proposition `" << name
			    << "' doesn't match any proposition "
			    << "in the kripke"
			    << std::endl;
		  exit (1);
		}
	    }

	  //
	  // It's P_0.var == 2
	  //
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
      	      free(name);
	      exit(1);
      	    }

	  // leading blank
      	  while (*s && (*s == ' ' || *s == '\t'))
      	    ++s;

      	  int val = 0; // Initialize to kill a warning from old compilers.
      	  int type_num = d_->get_state_variable_type(i);//ni->second.type;
      	  if (type_num == 0 || (*s >= '0' && *s <= '9') || *s == '-')
      	    {
      	      char* s_end;
      	      val = strtol(s, &s_end, 10);
      	      if (s == s_end)
      		{
      		  std::cerr << "Failed to parse `" << s
      			    << "' as an integer." << std::endl;
      		  free(name);
		  exit(1);
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

	      //std::cout << name <<" " << st << std::endl;

	      int ii;
	      for (ii = 0; ii < (int) d_->get_state_variable_count(); ++ii)
		{
		  const ltl::atomic_prop* a = get_dict().get(ii);
		  std::string sap = a->name();
		  if (sap.compare(name) == 0)
		    break;
		}

	      int type_num = d_->get_state_variable_type(ii);
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
		  exit(1);
		}
	      val = ei->second;
	      while (*s && (*s != ' ' || *s != '\t'))
		++s;
      	    }

      	  free(name);

      	  while (*s && (*s == ' ' || *s == '\t'))
      	    ++s;
      	  if (*s)
      	    {
      	      std::cerr << "Unexpected `" << s
      			<< "' while parsing atomic proposition `" << str
      			<< "'." << std::endl;
	      exit(1);
      	    }
      	  one_prop p = { var_num , op, val};
	  l->add_ap(p);
      }
  }

  ////////////////////////////////////////////////////////////////////////////
  // LOADER


  // Call divine to compile "foo.dve" as "foo.dve2C" if the latter
  // does not exist already or is older.
  bool
  compile_dve2fast(std::string& filename, bool verbose)
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


  const spot::fasttgba* load_dve2fast(const std::string& file_arg,
				      spot::ap_dict& aps,
				      spot::acc_dict& accs,
				      bool verbose)
  {
    if (!aps.empty())
      {
	std::cerr << "Fail to create the Kripke from a non empty "
		  << "atomic proposition set" << std::endl;
	return 0;
      }

    std::string file;
    if (file_arg.find_first_of("/\\") != std::string::npos)
      file = file_arg;
    else
      file = "./" + file_arg;

    std::string ext = file.substr(file.find_last_of("."));
    if (ext == ".dve")
      {
    	if (compile_dve2fast(file, verbose))
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
    	  && d->get_transition_count))
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


    fasttgba *kripke = new dve2_kripke(d, aps, accs);
    int errors = convert_aps(aps, d, kripke, false);
    if (errors)
      {
    	delete d;
    	lt_dlexit();
    	return 0;
      }

    return kripke;
  }
}
