// -*- coding: utf-8 -*-
// Copyright (C) 2011, 2012, 2014, 2015, 2016 Laboratoire de Recherche et
// Développement de l'Epita (LRDE)
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

// MinGW does not define this.
#ifndef WEXITSTATUS
# define WEXITSTATUS(x) ((x) & 0xff)
#endif

#include <spot/ltsmin/ltsmin.hh>
#include <spot/misc/hashfunc.hh>
#include <spot/misc/fixpool.hh>
#include <spot/misc/mspool.hh>
#include <spot/misc/intvcomp.hh>
#include <spot/misc/intvcmp2.hh>

#include <spot/twaalgos/reachiter.hh>
#include <string.h>
#include <spot/twacube/cube.hh>
#include <spot/mc/utils.hh>
#include <spot/mc/ec.hh>

#include <bricks/brick-hashset>
#include <bricks/brick-hash>

#include <spot/twaalgos/dot.hh>
#include <spot/twa/twaproduct.hh>
#include <spot/twaalgos/emptiness.hh>

namespace spot
{
  namespace
  {
    ////////////////////////////////////////////////////////////////////////
    // spins interface

    typedef struct transition_info {
      int* labels; // edge labels, NULL, or pointer to the edge label(s)
      int  group;  // holds transition group or -1 if unknown
    } transition_info_t;

    typedef void (*TransitionCB)(void *ctx,
                                 transition_info_t *transition_info,
                                 int *dst);
  }

  struct spins_interface
  {
    lt_dlhandle handle;        // handle to the dynamic library
    void (*get_initial_state)(void *to);
    int (*have_property)();
    int (*get_successors)(void* m, int *in, TransitionCB, void *arg);
    int (*get_state_size)();
    const char* (*get_state_variable_name)(int var);
    int (*get_state_variable_type)(int var);
    int (*get_type_count)();
    const char* (*get_type_name)(int type);
    int (*get_type_value_count)(int type);
    const char* (*get_type_value_name)(int type, int value);

    ~spins_interface()
    {
      if (handle)
        lt_dlclose(handle);
      lt_dlexit();
    }
  };

  namespace
  {
    typedef std::shared_ptr<const spins_interface> spins_interface_ptr;

    ////////////////////////////////////////////////////////////////////////
    // STATE

    struct spins_state final: public state
    {
      spins_state(int s, fixed_size_pool* p)
        : pool(p), size(s), count(1)
      {
      }

      void compute_hash()
      {
        hash_value = 0;
        for (int i = 0; i < size; ++i)
          hash_value = wang32_hash(hash_value ^ vars[i]);
      }

      spins_state* clone() const override
      {
        ++count;
        return const_cast<spins_state*>(this);
      }

      void destroy() const override
      {
        if (--count)
          return;
        pool->deallocate(this);
      }

      size_t hash() const override
      {
        return hash_value;
      }

      int compare(const state* other) const override
      {
        if (this == other)
          return 0;
        const spins_state* o = down_cast<const spins_state*>(other);
        assert(o);
        if (hash_value < o->hash_value)
          return -1;
        if (hash_value > o->hash_value)
          return 1;
        return memcmp(vars, o->vars, size * sizeof(*vars));
      }

    private:

      ~spins_state()
      {
      }

    public:
      fixed_size_pool* pool;
      size_t hash_value: 32;
      int size: 16;
      mutable unsigned count: 16;
      int vars[0];
    };

    struct spins_compressed_state final: public state
    {
      spins_compressed_state(int s, multiple_size_pool* p)
        : pool(p), size(s), count(1)
      {
      }

      void compute_hash()
      {
        hash_value = 0;
        for (int i = 0; i < size; ++i)
          hash_value = wang32_hash(hash_value ^ vars[i]);
      }

      spins_compressed_state* clone() const override
      {
        ++count;
        return const_cast<spins_compressed_state*>(this);
      }

      void destroy() const override
      {
        if (--count)
          return;
        pool->deallocate(this, sizeof(*this) + size * sizeof(*vars));
      }

      size_t hash() const override
      {
        return hash_value;
      }

      int compare(const state* other) const override
      {
        if (this == other)
          return 0;
        const spins_compressed_state* o =
          down_cast<const spins_compressed_state*>(other);
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

      ~spins_compressed_state()
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

      ~callback_context()
      {
        for (auto t: transitions)
          t->destroy();
      }
    };

    void transition_callback(void* arg, transition_info_t*, int *dst)
    {
      callback_context* ctx = static_cast<callback_context*>(arg);
      fixed_size_pool* p = static_cast<fixed_size_pool*>(ctx->pool);
      spins_state* out =
        new(p->allocate()) spins_state(ctx->state_size, p);
      memcpy(out->vars, dst, ctx->state_size * sizeof(int));
      out->compute_hash();
      ctx->transitions.emplace_back(out);
    }

    void transition_callback_compress(void* arg, transition_info_t*, int *dst)
    {
      callback_context* ctx = static_cast<callback_context*>(arg);
      multiple_size_pool* p = static_cast<multiple_size_pool*>(ctx->pool);

      size_t csize = ctx->state_size * 2;
      ctx->compress(dst, ctx->state_size, ctx->compressed, csize);

      void* mem = p->allocate(sizeof(spins_compressed_state)
                              + sizeof(int) * csize);
      spins_compressed_state* out = new(mem) spins_compressed_state(csize, p);
      memcpy(out->vars, ctx->compressed, csize * sizeof(int));
      out->compute_hash();
      ctx->transitions.emplace_back(out);
    }

    ////////////////////////////////////////////////////////////////////////
    // SUCC_ITERATOR

    class spins_succ_iterator final: public kripke_succ_iterator
    {
    public:

      spins_succ_iterator(const callback_context* cc,
                         bdd cond)
        : kripke_succ_iterator(cond), cc_(cc)
      {
      }

      void recycle(const callback_context* cc, bdd cond)
      {
        delete cc_;
        cc_ = cc;
        kripke_succ_iterator::recycle(cond);
      }

      ~spins_succ_iterator()
      {
        delete cc_;
      }

      virtual bool first() override
      {
        it_ = cc_->transitions.begin();
        return it_ != cc_->transitions.end();
      }

      virtual bool next() override
      {
        ++it_;
        return it_ != cc_->transitions.end();
      }

      virtual bool done() const override
      {
        return it_ == cc_->transitions.end();
      }

      virtual state* dst() const override
      {
        return (*it_)->clone();
      }

    private:
      const callback_context* cc_;
      callback_context::transitions_t::const_iterator it_;
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


    void
    convert_aps(const atomic_prop_set* aps,
                spins_interface_ptr d,
                bdd_dict_ptr dict,
                formula dead,
                prop_set& out)
    {
      int errors = 0;
      std::ostringstream err;

      int state_size = d->get_state_size();
      typedef std::map<std::string, var_info> val_map_t;
      val_map_t val_map;

      for (int i = 0; i < state_size; ++i)
        {
          const char* name = d->get_state_variable_name(i);
          int type = d->get_state_variable_type(i);
          var_info v = { i , type };
          val_map[name] = v;
        }

      int type_count = d->get_type_count();
      typedef std::map<std::string, int> enum_map_t;
      std::vector<enum_map_t> enum_map(type_count);
      for (int i = 0; i < type_count; ++i)
        {
          int enum_count = d->get_type_value_count(i);
          for (int j = 0; j < enum_count; ++j)
            enum_map[i].emplace(d->get_type_value_name(i, j), j);
        }

      for (atomic_prop_set::const_iterator ap = aps->begin();
           ap != aps->end(); ++ap)
        {
          if (*ap == dead)
            continue;

          const std::string& str = ap->ap_name();
          const char* s = str.c_str();

          // Skip any leading blank.
          while (*s && (*s == ' ' || *s == '\t'))
            ++s;
          if (!*s)
            {
              err << "Proposition `" << str << "' cannot be parsed.\n";
              ++errors;
              continue;
            }


          char* name = (char*) malloc(str.size() + 1);
          char* name_p = name;
          char* lastdot = nullptr;
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
              err << "Proposition `" << str << "' cannot be parsed.\n";
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
                  err << "No variable `" << name
                      << "' found in model (for proposition `"
                      << str << "').\n";
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
                  err << "No state `" << lastdot << "' known for variable `"
                      << name << "'.\n";
                  err << "Possible states are:";
                  for (auto& ej: enum_map[type_num])
                    err << ' ' << ej.first;
                  err << '\n';

                  free(name);
                  ++errors;
                  continue;
                }

              // At this point, *s should be 0.
              if (*s)
                {
                  err << "Trailing garbage `" << s
                      << "' at end of proposition `"
                      << str << "'.\n";
                  free(name);
                  ++errors;
                  continue;
                }

              // Record that X.Y must be equal to Z.
              int v = dict->register_proposition(*ap, d.get());
              one_prop p = { ni->second.num, OP_EQ, ei->second, v };
              out.emplace_back(p);
              free(name);
              continue;
            }

          int var_num = ni->second.num;

          if (!*s)                // No operator?  Assume "!= 0".
            {
              int v = dict->register_proposition(*ap, d);
              one_prop p = { var_num, OP_NE, 0, v };
              out.emplace_back(p);
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
              err << "Unexpected `" << s
                  << "' while parsing atomic proposition `" << str
                  << "'.\n";
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
                  err << "Failed to parse `" << s << "' as an integer.\n";
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
                  err << "No state `" << st << "' known for variable `"
                      << name << "'.\n";
                  err << "Possible states are:";
                  for (ei = enum_map[type_num].begin();
                       ei != enum_map[type_num].end(); ++ei)
                    err << ' ' << ei->first;
                  err << '\n';

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
              err << "Unexpected `" << s
                  << "' while parsing atomic proposition `" << str
                  << "'.\n";
              ++errors;
              continue;
            }


          int v = dict->register_proposition(*ap, d);
          one_prop p = { var_num, op, val, v };
          out.emplace_back(p);
        }

      if (errors)
        throw std::runtime_error(err.str());
    }

    ////////////////////////////////////////////////////////////////////////
    // KRIPKE

    class spins_kripke final: public kripke
    {
    public:

      spins_kripke(spins_interface_ptr d, const bdd_dict_ptr& dict,
                   const spot::prop_set* ps, formula dead,
                   int compress)
        : kripke(dict),
          d_(d),
          state_size_(d_->get_state_size()),
          ps_(ps),
          compress_(compress == 0 ? nullptr
                    : compress == 1 ? int_array_array_compress
                    : int_array_array_compress2),
          decompress_(compress == 0 ? nullptr
                      : compress == 1 ? int_array_array_decompress
                      : int_array_array_decompress2),
          uncompressed_(compress ? new int[state_size_ + 30] : nullptr),
          compressed_(compress ? new int[state_size_ * 2] : nullptr),
          statepool_(compress ? sizeof(spins_compressed_state) :
                     (sizeof(spins_state) + state_size_ * sizeof(int))),
          state_condition_last_state_(nullptr),
          state_condition_last_cc_(nullptr)
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
              (d->get_type_value_count(type) != 1);
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
        if (dead.is_ff())
          {
            alive_prop = bddtrue;
            dead_prop = bddfalse;
          }
        else if (dead.is_tt())
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
      }

      ~spins_kripke()
      {
        if (iter_cache_)
          {
            delete iter_cache_;
            iter_cache_ = nullptr;
          }
        delete[] format_filter_;
        delete[] vname_;
        if (compress_)
          {
            delete[] uncompressed_;
            delete[] compressed_;
          }
        dict_->unregister_all_my_variables(d_.get());

        delete ps_;

        if (state_condition_last_state_)
          state_condition_last_state_->destroy();
        delete state_condition_last_cc_; // Might be 0 already.
      }

      virtual state* get_init_state() const override
      {
        if (compress_)
          {
            d_->get_initial_state(uncompressed_);
            size_t csize = state_size_ * 2;
            compress_(uncompressed_, state_size_, compressed_, csize);

            multiple_size_pool* p =
              const_cast<multiple_size_pool*>(&compstatepool_);
            void* mem = p->allocate(sizeof(spins_compressed_state)
                                    + sizeof(int) * csize);
            spins_compressed_state* res = new(mem)
              spins_compressed_state(csize, p);
            memcpy(res->vars, compressed_, csize * sizeof(int));
            res->compute_hash();
            return res;
          }
        else
          {
            fixed_size_pool* p = const_cast<fixed_size_pool*>(&statepool_);
            spins_state* res = new(p->allocate()) spins_state(state_size_, p);
            d_->get_initial_state(res->vars);
            res->compute_hash();
            return res;
          }
      }

      bdd
      compute_state_condition_aux(const int* vars) const
      {
        bdd res = bddtrue;
        for (auto& i: *ps_)
          {
            int l = vars[i.var_num];
            int r = i.val;

            bool cond = false;
            switch (i.op)
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
              res &= bdd_ithvar(i.bddvar);
            else
              res &= bdd_nithvar(i.bddvar);
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
        t = d_->get_successors(nullptr, const_cast<int*>(vars),
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
            state_condition_last_cc_ = nullptr;
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
              cc->transitions.emplace_back(st->clone());
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
            const spins_compressed_state* s =
              down_cast<const spins_compressed_state*>(st);
            assert(s);

            decompress_(s->vars, s->size, uncompressed_, state_size_);
            vars = uncompressed_;
          }
        else
          {
            const spins_state* s = down_cast<const spins_state*>(st);
            assert(s);
            vars = s->vars;
          }
        return vars;
      }


      virtual
      spins_succ_iterator* succ_iter(const state* st) const override
      {
        // This may also compute successors in state_condition_last_cc
        bdd scond = compute_state_condition(st);

        callback_context* cc;
        if (state_condition_last_cc_)
          {
            cc = state_condition_last_cc_;
            state_condition_last_cc_ = nullptr; // Now owned by the iterator.
          }
        else
          {
            int t;
            cc = build_cc(get_vars(st), t);

            // Add a self-loop to dead-states if we care about these.
            if (t == 0 && scond != bddfalse)
              cc->transitions.emplace_back(st->clone());
          }

        if (iter_cache_)
          {
            spins_succ_iterator* it =
              down_cast<spins_succ_iterator*>(iter_cache_);
            it->recycle(cc, scond);
            iter_cache_ = nullptr;
            return it;
          }
        return new spins_succ_iterator(cc, scond);
      }

      virtual
      bdd state_condition(const state* st) const override
      {
        return compute_state_condition(st);
      }

      virtual
      std::string format_state(const state *st) const override
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
                if (i == state_size_)
                  break;
                continue;
              }
            res << vname_[i] << '=' << vars[i];
            ++i;
            if (i == state_size_)
              break;
            res << ", ";
          }
        return res.str();
      }

    private:
      spins_interface_ptr d_;
      int state_size_;
      const char** vname_;
      bool* format_filter_;
      const spot::prop_set* ps_;
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
    };


    //////////////////////////////////////////////////////////////////////////
    // LOADER

    // Call spins to compile "foo.prom" as "foo.prom.spins" if the latter
    // does not exist already or is older.
    static void
    compile_model(std::string& filename, const std::string& ext)
    {
      std::string command;
      std::string compiled_ext;

      if (ext == ".prom" || ext == ".pm" || ext == ".pml")
        {
          command = "spins " + filename;
          compiled_ext = ".spins";
        }
      else if (ext == ".dve")
        {
          command = "divine compile --ltsmin " + filename;
          command += " 2> /dev/null"; // FIXME needed for Clang on MacOSX
          compiled_ext = "2C";
        }
      else
        {
          throw std::runtime_error(std::string("Unknown extension '")
                                   + ext + "'.  Use '.prom', '.pm', '.pml', "
                                   "'.dve', '.dve2C', or '.prom.spins'.");
        }

      struct stat s;
      if (stat(filename.c_str(), &s) != 0)
        throw std::runtime_error(std::string("Cannot open ") + filename);

      filename += compiled_ext;

      // Remove any directory, because the new file will
      // be compiled in the current directory.
      size_t pos = filename.find_last_of("/\\");
      if (pos != std::string::npos)
        filename = "./" + filename.substr(pos + 1);

      struct stat d;
      if (stat(filename.c_str(), &d) == 0)
        if (s.st_mtime < d.st_mtime)
          // The .spins or .dve2C is up-to-date, no need to recompile it.
          return;

      int res = system(command.c_str());
      if (res)
        throw std::runtime_error(std::string("Execution of '")
                                 + command.c_str() + "' returned exit code "
                                 + std::to_string(WEXITSTATUS(res)));
    }
  }

  ltsmin_model
  ltsmin_model::load(const std::string& file_arg)
  {
    std::string file;
    if (file_arg.find_first_of("/\\") != std::string::npos)
      file = file_arg;
    else
      file = "./" + file_arg;

    std::string ext = file.substr(file.find_last_of("."));
    if (ext != ".spins" && ext != ".dve2C")
      compile_model(file, ext);

    if (lt_dlinit())
      throw std::runtime_error("Failed to initialize libltldl.");

    lt_dlhandle h = lt_dlopen(file.c_str());
    if (!h)
      {
        lt_dlexit();
        throw std::runtime_error(std::string("Failed to load '")
                                 + file + "'.");
      }

    auto d = std::make_shared<spins_interface>();
    d->handle = h;

    // SpinS interface.
    if ((d->get_initial_state = (void (*)(void*))
        lt_dlsym(h, "spins_get_initial_state")))
      {
        d->have_property = nullptr;
        d->get_successors = (int (*)(void*, int*, TransitionCB, void*))
        lt_dlsym(h, "spins_get_successor_all");
        d->get_state_size = (int (*)())
        lt_dlsym(h, "spins_get_state_size");
        d->get_state_variable_name = (const char* (*)(int))
        lt_dlsym(h, "spins_get_state_variable_name");
        d->get_state_variable_type = (int (*)(int))
        lt_dlsym(h, "spins_get_state_variable_type");
        d->get_type_count = (int (*)())
        lt_dlsym(h, "spins_get_type_count");
        d->get_type_name = (const char* (*)(int))
        lt_dlsym(h, "spins_get_type_name");
        d->get_type_value_count = (int (*)(int))
        lt_dlsym(h, "spins_get_type_value_count");
        d->get_type_value_name = (const char* (*)(int, int))
        lt_dlsym(h, "spins_get_type_value_name");
      }
    // dve2 interface.
    else
      {
        d->get_initial_state = (void (*)(void*))
        lt_dlsym(h, "get_initial_state");
        d->have_property = (int (*)())
        lt_dlsym(h, "have_property");
        d->get_successors = (int (*)(void*, int*, TransitionCB, void*))
        lt_dlsym(h, "get_successors");
        d->get_state_size = (int (*)())
        lt_dlsym(h, "get_state_variable_count");
        d->get_state_variable_name = (const char* (*)(int))
        lt_dlsym(h, "get_state_variable_name");
        d->get_state_variable_type = (int (*)(int))
        lt_dlsym(h, "get_state_variable_type");
        d->get_type_count = (int (*)())
        lt_dlsym(h, "get_state_variable_type_count");
        d->get_type_name = (const char* (*)(int))
        lt_dlsym(h, "get_state_variable_type_name");
        d->get_type_value_count = (int (*)(int))
        lt_dlsym(h, "get_state_variable_type_value_count");
        d->get_type_value_name = (const char* (*)(int, int))
        lt_dlsym(h, "get_state_variable_type_value");
      }

    if (!(d->get_initial_state
          && d->get_successors
          && d->get_state_size
          && d->get_state_variable_name
          && d->get_state_variable_type
          && d->get_type_count
          && d->get_type_name
          && d->get_type_value_count
          && d->get_type_value_name))
      throw std::runtime_error(std::string("Failed resolve some symbol"
                                           "while loading '") + file + "'.");

    if (d->have_property && d->have_property())
      throw std::runtime_error("Models with embedded properties "
                               "are not supported.");

    return { d };
  }

  ////////////////////////////////////////////////////////////////////////
  // CSpins comparison functions & definitions


  struct cspins_state_equal
  {
    bool
    operator()(const cspins_state lhs,
               const cspins_state rhs) const
    {
      return 0 == memcmp(lhs, rhs, (2+rhs[1])* sizeof(int));
    }
  };

  struct cspins_state_hash
  {
    size_t
    operator()(const cspins_state that) const
    {
      return that[0];
    }
  };

  struct both
  {
    cspins_state first;
    int second;
  };

  struct cspins_state_hasher
  {
    cspins_state_hasher(cspins_state&)
    { }

    cspins_state_hasher() = default;

    brick::hash::hash128_t
    hash(cspins_state t) const
    {
      // FIXME we should compute a better hash value for this particular
      // case. Shall we use two differents hash functions?
      return std::make_pair(t[0], t[0]);
    }

    bool equal(cspins_state lhs, cspins_state rhs) const
    {
      return 0 == memcmp(lhs, rhs, (2+rhs[1])* sizeof(int));
    }
  };

  typedef brick::hashset::FastConcurrent<cspins_state,
                                         cspins_state_hasher>  cspins_state_map;

  ////////////////////////////////////////////////////////////////////////
  // A manager for Cspins states.

  class cspins_state_manager final
  {
  public:
    cspins_state_manager(unsigned int state_size,
                        int compress)
      : p_((state_size+2)*sizeof(int)), compress_(compress),
      /* reserve one integer for the hash value and size */
      state_size_(state_size),
      fn_compress_(compress == 0 ? nullptr
                   : compress == 1 ? int_array_array_compress
                   : int_array_array_compress2),
      fn_decompress_(compress == 0 ? nullptr
                     : compress == 1 ? int_array_array_decompress
                     : int_array_array_decompress2)
        { }

    int* unbox_state(cspins_state s) const
    {
      return s+2;
    }

    // cmp is the area we can use to compute the compressed rep of dst.
    cspins_state alloc_setup(int *dst, int* cmp, size_t cmpsize)
    {
      cspins_state out = nullptr;
      size_t size = state_size_;
      int* ref = dst;
      if (compress_)
        {
          size_t csize = cmpsize;
          fn_compress_(dst, state_size_, cmp, csize);
          ref = cmp;
          size = csize;
          out = (cspins_state) msp_.allocate((size+2)*sizeof(int));
        }
      else
        out = (cspins_state) p_.allocate();
      int hash_value = 0;
      memcpy(unbox_state(out), ref, size * sizeof(int));
      for (unsigned int i = 0; i < state_size_; ++i)
        hash_value = wang32_hash(hash_value ^ dst[i]);
      out[0] = hash_value;
      out[1] = size;
      return out;
    }

    void decompress(cspins_state s, int* uncompressed, unsigned size) const
    {
      fn_decompress_(s+2, s[1], uncompressed, size);
    }

    void  dealloc(cspins_state s)
    {
      if (compress_)
        msp_.deallocate(s, (s[1]+2)*sizeof(int));
      else
        p_.deallocate(s);
    }

    unsigned int size() const
    {
      return state_size_;
    }

  private:
    fixed_size_pool p_;
    multiple_size_pool msp_;
    bool compress_;
    const unsigned int state_size_;
    void (*fn_compress_)(const int*, size_t, int*, size_t&);
    void (*fn_decompress_)(const int*, size_t, int*, size_t);
  };


  ////////////////////////////////////////////////////////////////////////
  // Iterator over Cspins states

  // This structure is used as a parameter during callback when
  // generating states from the shared librarie produced by LTSmin
  struct inner_callback_parameters
  {
    cspins_state_manager* manager;   // The state manager
    std::vector<cspins_state>* succ; // The successors of a state
    cspins_state_map* map;
    int* compressed_;
    int* uncompressed_;
    int default_value_;
    bool compress;
    bool selfloopize;
  };

  // This class provides an iterator over the successors of a state.
  // All successors are computed once when an iterator is recycled or
  // created.
  class cspins_iterator final
  {
  public:
    cspins_iterator(cspins_state s,
                    const spot::spins_interface* d,
                    cspins_state_manager& manager,
                    cspins_state_map& map,
                    inner_callback_parameters& inner,
                    int defvalue,
                    cube cond,
                    bool compress,
                    bool selfloopize,
                    cubeset& cubeset,
                    int dead_idx, unsigned tid)
      :  current_(0), cond_(cond), tid_(tid)
    {
      successors_.reserve(10);
      inner.manager = &manager;
      inner.map = &map;
      inner.succ = &successors_;
      inner.default_value_ = defvalue;
      inner.compress = compress;
      inner.selfloopize = selfloopize;
      int* ref = s;

      if (compress)
        // already filled by compute_condition
        ref = inner.uncompressed_;

      int n = d->get_successors
        (nullptr, manager.unbox_state(ref),
         [](void* arg, transition_info_t*, int *dst){
          inner_callback_parameters* inner =
          static_cast<inner_callback_parameters*>(arg);
          cspins_state s =
          inner->manager->alloc_setup(dst, inner->compressed_,
                                      inner->manager->size() * 2);
          auto it = inner->map->insert({s});
          inner->succ->push_back(*it);
          if (!it.isnew())
            inner->manager->dealloc(s);
        },
         &inner);
      if (!n && selfloopize)
        {
          successors_.push_back(s);
          if (dead_idx != -1)
            cubeset.set_true_var(cond, dead_idx);
        }
    }

    void recycle(cspins_state s,
                 const spot::spins_interface* d,
                 cspins_state_manager& manager,
                 cspins_state_map& map,
                 inner_callback_parameters& inner,
                 int defvalue,
                 cube cond,
                 bool compress,
                 bool selfloopize,
                 cubeset& cubeset, int dead_idx, unsigned tid)
    {
      tid_ = tid;
      cond_ = cond;
      current_ = 0;
      // Constant time since int* is is_trivially_destructible
      successors_.clear();
      inner.manager = &manager;
      inner.succ = &successors_;
      inner.map = &map;
      inner.default_value_ = defvalue;
      inner.compress = compress;
      inner.selfloopize = selfloopize;
      int* ref  = s;

      if (compress)
        // Already filled by compute_condition
        ref = inner.uncompressed_;

      int n = d->get_successors
        (nullptr, manager.unbox_state(ref),
         [](void* arg, transition_info_t*, int *dst){
          inner_callback_parameters* inner =
          static_cast<inner_callback_parameters*>(arg);
          cspins_state s =
          inner->manager->alloc_setup(dst, inner->compressed_,
                                      inner->manager->size() * 2);
          auto it = inner->map->insert({s});
          inner->succ->push_back(*it);
          if (!it.isnew())
            inner->manager->dealloc(s);
        },
         &inner);
      if (!n && selfloopize)
        {
          successors_.push_back(s);
          if (dead_idx != -1)
            cubeset.set_true_var(cond, dead_idx);
        }
    }

    ~cspins_iterator()
      {
        // Do not release successors states, the manager
        // will do it on time.
      }

    void next()
    {
      ++current_;
    }

    bool done() const
    {
      return current_ >= successors_.size();
    }

    cspins_state state() const
    {
      if (SPOT_UNLIKELY(!tid_))
        return successors_[current_];
      return  successors_[(((current_+1)*primes[tid_])
                           % ((int)successors_.size()))];
    }

    cube condition() const
    {
      return cond_;
    }

  private:
    std::vector<cspins_state> successors_;
    unsigned int current_;
    cube cond_;
    unsigned tid_;
  };

  ////////////////////////////////////////////////////////////////////////
  // Concrete definition of the system

  // A specialisation of the template class kripke
  template<>
  class kripkecube<cspins_state, cspins_iterator> final
  {

    typedef enum {
      OP_EQ_VAR, OP_NE_VAR, OP_LT_VAR, OP_GT_VAR, OP_LE_VAR, OP_GE_VAR,
      VAR_OP_EQ, VAR_OP_NE, VAR_OP_LT, VAR_OP_GT, VAR_OP_LE, VAR_OP_GE,
      VAR_OP_EQ_VAR, VAR_OP_NE_VAR, VAR_OP_LT_VAR,
      VAR_OP_GT_VAR, VAR_OP_LE_VAR, VAR_OP_GE_VAR, VAR_DEAD
    } relop;

    // Structure for complex atomic proposition
    struct one_prop
    {
      int lval;
      relop op;
      int rval;
    };

    // Data structure to store complex atomic propositions
    typedef std::vector<one_prop> prop_set;
    prop_set pset_;

  public:
    kripkecube(spins_interface_ptr sip, bool compress,
               std::vector<std::string> visible_aps,
               bool selfloopize, std::string dead_prop,
               unsigned int nb_threads)
      : sip_(sip), d_(sip.get()),
        compress_(compress), cubeset_(visible_aps.size()),
        selfloopize_(selfloopize), aps_(visible_aps), nb_threads_(nb_threads)
    {
      map_.initialSize(2000000);
      manager_ = static_cast<cspins_state_manager*>
        (::operator new(sizeof(cspins_state_manager) * nb_threads));
      inner_ = new inner_callback_parameters[nb_threads_];
      for (unsigned i = 0; i < nb_threads_; ++i)
        {
          recycle_.push_back(std::vector<cspins_iterator*>());
          recycle_.back().reserve(2000000);
          new (&manager_[i])
            cspins_state_manager(d_->get_state_size(), compress);
          inner_[i].compressed_ = new int[d_->get_state_size() * 2];
          inner_[i].uncompressed_ = new int[d_->get_state_size()+30];
        }
      dead_idx_ = -1;
      match_aps(visible_aps, dead_prop);

    }

    ~kripkecube()
      {
        for (auto i: recycle_)
          {
            for (auto j: i)
              {
                cubeset_.release(j->condition());
                delete j;
              }
          }

        for (unsigned i = 0; i < nb_threads_; ++i)
          {
            manager_[i].~cspins_state_manager();
            delete inner_[i].compressed_;
            delete inner_[i].uncompressed_;
          }
        delete[] inner_;
      }

    cspins_state initial(unsigned tid)
    {
      d_->get_initial_state(inner_[tid].uncompressed_);
      return manager_[tid].alloc_setup(inner_[tid].uncompressed_,
                                  inner_[tid].compressed_,
                                  manager_[tid].size() * 2);
    }

    std::string to_string(const cspins_state s, unsigned tid = 0) const
    {
      std::string res = "";
      cspins_state out = manager_[tid].unbox_state(s);
      cspins_state ref = out;
      if (compress_)
        {
          manager_[tid].decompress(s, inner_[tid].uncompressed_,
                                   manager_[tid].size());
          ref = inner_[tid].uncompressed_;
        }
      for (int i = 0; i < d_->get_state_size(); ++i)
        res += (d_->get_state_variable_name(i)) +
          ("=" + std::to_string(ref[i])) + ",";
      res.pop_back();
      return res;
    }

    cspins_iterator* succ(const cspins_state s, unsigned tid)
    {
      if (SPOT_LIKELY(!recycle_[tid].empty()))
        {
          auto tmp  = recycle_[tid].back();
          recycle_[tid].pop_back();
          compute_condition(tmp->condition(), s, tid);
          tmp->recycle(s, d_, manager_[tid], map_, inner_[tid], -1,
                       tmp->condition(), compress_, selfloopize_,
                       cubeset_, dead_idx_, tid);
          return tmp;
        }
      cube cond = cubeset_.alloc();
      compute_condition(cond, s, tid);
      return new cspins_iterator(s, d_, manager_[tid], map_, inner_[tid],
                                 -1, cond, compress_, selfloopize_,
                                 cubeset_, dead_idx_, tid);
    }

    void recycle(cspins_iterator* it, unsigned tid)
    {
      recycle_[tid].push_back(it);
    }

    const std::vector<std::string> get_ap()
    {
      return aps_;
    }

    unsigned get_threads()
    {
      return nb_threads_;
    }

  private:
    // The two followings functions are too big  to be inlined in
    // this class. See below for more details

    /// Parse the set of atomic proposition to have a more
    /// efficient data strucure for computation
    void match_aps(std::vector<std::string>& aps, std::string dead_prop);

    /// Compute the cube associated to each state. The cube
    /// will then be given to all iterators.
    void compute_condition(cube c, cspins_state s, unsigned tid = 0);


    spins_interface_ptr sip_;
    const spot::spins_interface* d_; // To avoid numerous sip_.get()
    cspins_state_manager* manager_;
    bool compress_;
    std::vector<std::vector<cspins_iterator*>> recycle_;
    inner_callback_parameters* inner_;
    cubeset cubeset_;
    bool selfloopize_;
    int dead_idx_;
    std::vector<std::string> aps_;
    cspins_state_map map_;
    unsigned int nb_threads_;
  };

  void
  kripkecube<cspins_state, cspins_iterator>::compute_condition
  (cube c, cspins_state s, unsigned tid)
  {
    int i = -1;
    int *vars = manager_[tid].unbox_state(s);

    // State is compressed, uncompress it
    if (compress_)
      {
        manager_[tid].decompress(s, inner_[tid].uncompressed_+2,
                                 manager_[tid].size());
        vars = inner_[tid].uncompressed_ + 2;
      }

    for (auto& ap: pset_)
      {
        ++i;
        bool cond = false;
        switch (ap.op)
          {
          case OP_EQ_VAR:
            cond = (ap.lval == vars[ap.rval]);
            break;
          case OP_NE_VAR:
            cond = (ap.lval != vars[ap.rval]);
            break;
          case OP_LT_VAR:
            cond = (ap.lval < vars[ap.rval]);
            break;
          case OP_GT_VAR:
            cond = (ap.lval > vars[ap.rval]);
            break;
          case OP_LE_VAR:
            cond = (ap.lval <= vars[ap.rval]);
            break;
          case OP_GE_VAR:
            cond = (ap.lval >= vars[ap.rval]);
            break;
          case VAR_OP_EQ:
            cond = (vars[ap.lval] == ap.rval);
            break;
          case VAR_OP_NE:
            cond = (vars[ap.lval] != ap.rval);
            break;
          case VAR_OP_LT:
            cond = (vars[ap.lval] < ap.rval);
            break;
          case VAR_OP_GT:
            cond = (vars[ap.lval] > ap.rval);
            break;
          case VAR_OP_LE:
            cond = (vars[ap.lval] <= ap.rval);
            break;
          case VAR_OP_GE:
            cond = (vars[ap.lval] >= ap.rval);
            break;
          case VAR_OP_EQ_VAR:
            cond = (vars[ap.lval] == vars[ap.rval]);
            break;
          case VAR_OP_NE_VAR:
            cond = (vars[ap.lval] != vars[ap.rval]);
            break;
          case VAR_OP_LT_VAR:
            cond = (vars[ap.lval] < vars[ap.rval]);
            break;
          case VAR_OP_GT_VAR:
            cond = (vars[ap.lval] > vars[ap.rval]);
            break;
          case VAR_OP_LE_VAR:
            cond = (vars[ap.lval] <= vars[ap.rval]);
            break;
          case VAR_OP_GE_VAR:
            cond = (vars[ap.lval] >= vars[ap.rval]);
            break;
          case VAR_DEAD:
            break;
          default:
            assert(false);
          }

        if (cond)
          cubeset_.set_true_var(c, i);
        else
          cubeset_.set_false_var(c, i);
      }
  }

  // FIXME I think we only need visbles aps, i.e. if the system has
  // following variables, i.e. P_0.var1 and P_0.var2 but the property
  // automaton only mention P_0.var2, we do not need to capture (in
  // the resulting cube) any atomic proposition for P_0.var1
  void
  kripkecube<cspins_state,
             cspins_iterator>::match_aps(std::vector<std::string>& aps,
                                         std::string dead_prop)
  {
    // Keep trace of errors
    int errors = 0;
    std::ostringstream err;

    // First we capture state name of each processes.
    int type_count = d_->get_type_count();
    typedef std::map<std::string, int> enum_map_t;
    std::vector<enum_map_t> enum_map(type_count);
    std::unordered_map<std::string, int> matcher;
    for (int i = 0; i < type_count; ++i)
      {
        matcher[d_->get_type_name(i)] = i;
        int enum_count = d_->get_type_value_count(i);
        for (int j = 0; j < enum_count; ++j)
          enum_map[i].emplace(d_->get_type_value_name(i, j), j);
      }

    // Then we extract the basic atomics propositions from the Kripke
    std::vector<std::string> k_aps;
    int state_size = d_->get_state_size();
    for (int i = 0; i < state_size; ++i)
      k_aps.push_back(d_->get_state_variable_name(i));

    int i  = -1;
    for (auto ap: aps)
      {
        ++i;

        // Grab dead property
        if (ap.compare(dead_prop) == 0)
          {
            dead_idx_ = i;
            pset_.push_back({i , VAR_DEAD, 0});
            continue;
          }

        // Get ap name and remove all extra whitespace
        ap.erase(std::remove_if(ap.begin(), ap.end(),
                                [](char x){
                                  return std::isspace(x);
                                }),
                 ap.end());

        // Look if it is a well known atomic proposition
        auto it = std::find(k_aps.begin(), k_aps.end(), ap);
        if (it != k_aps.end())
          {
            // The aps is directly an AP of the system, we will just
            // have to detect if the variable is 0 or not.
            pset_.push_back({(int)std::distance(k_aps.begin(), it),
                  VAR_OP_NE, 0});
            continue;
          }

        // The ap is not known. We distinguish many cases:
        //     - It is a State name, i.e P_0.S or P_0 == S
        //     - It refers a specific variable value, i.e. P_0.var == 2,
        //       P_0.var < 2, P_0.var != 2, ...
        //     - It's an unknown variable
        // Note that we do not support P_0.state1 == 12 since we do not
        // know how to interpret such atomic proposition.

        // We split the formula according to operators
        std::size_t found_op_first = ap.find_first_of("=<>!");
        std::size_t found_op_last = ap.find_last_of("=<>!");
        std::string left;
        std::string right;
        std::string ap_error;
        std::string op;

        if (found_op_first == 0 || found_op_last == ap.size()-1)
          {
            err << "Invalid operator use in " << ap << '\n';
            ++errors;
            continue;
          }

        if (std::string::npos == found_op_first)
          {
            left = ap;
            right = "";
            op = "";
          }
        else
          {
            left = ap.substr(0, found_op_first);
            right = ap.substr(found_op_last+1, ap.size()-found_op_last);
            op  = ap.substr(found_op_first, found_op_last+1-found_op_first);
          }

        // Variables to store the left part of the atomic proposition
        bool left_is_digit = false;
        int lval;

        // Variables to store the right part of the atomic proposition
        bool right_is_digit = false;
        int rval;

        // And finally the operator
        relop oper;


        // Now, left and (possibly) right may refer atomic
        // propositions or specific state inside of a process.
        // First check if it is a known atomic proposition
        it = std::find(k_aps.begin(), k_aps.end(), left);
        if (it != k_aps.end())
          {
            // The ap is directly an AP of the system, we will just
            // have to detect if the variable is 0 or not.
            lval = std::distance(k_aps.begin(), it);
          }
        else
          {
            // Detect if it is a process state
            std::size_t found_dot = left.find_first_of('.');
            if (std::string::npos != found_dot)
              {
                std::string proc_name = left.substr(0, found_dot);
                std::string st_name = left.substr(found_dot+1,
                                                  left.size()-found_dot);

                auto ni = matcher.find(proc_name);
                if (ni == matcher.end())
                  {
                    ap_error = left;
                    goto error_ap_unknown;
                  }
                int type_num = ni->second;
                enum_map_t::const_iterator ei =
                  enum_map[type_num].find(st_name);
                if (ei == enum_map[type_num].end())
                  {
                    ap_error = left;
                    goto error_ap_unknown;
                  }

                if (right.compare("") != 0)
                  {
                    // We are in the case P.state1 == something.. We don't
                    // know how to interpret this.
                    ap_error = op + right;
                    err << "\nOperation " << op  << " in \"" << ap_error
                        << "\" is not available for process's state"
                        << " (i.e. " <<  left << ")\n";
                    ++errors;
                    continue;
                  }

                pset_.push_back({
                    (int) std::distance(k_aps.begin(),
                                        std::find(k_aps.begin(),
                                                  k_aps.end(), proc_name)),
                      VAR_OP_EQ,  ei->second});
                continue;
              }
            else
              {
                // Finally, it's a number...
                left_is_digit = true;
                for (auto c: left)
                  if (!isdigit(c))
                    left_is_digit = false;

                if (left_is_digit)
                  lval = std::strtol (left.c_str(), nullptr, 10);
                else
                  {
                    // ... or something like: State1 == P_0
                    // so it doesn't contains '.'
                    if (std::string::npos != right.find_first_of('.'))
                      {
                        err << "\nOperation \"" << right
                            << "\" does not refer a process"
                            << " (i.e. " << left << " is not valid)\n";
                        ++errors;
                        continue;
                      }

                    // or something like: P_0 == State1
                    auto ni = matcher.find(right);
                    if (ni == matcher.end())
                      {
                        ap_error = ap;
                        goto error_ap_unknown;
                      }
                    int type_num = ni->second;
                    enum_map_t::const_iterator ei =
                      enum_map[type_num].find(left);
                    if (ei == enum_map[type_num].end())
                      {
                        ap_error = left;
                        goto error_ap_unknown;
                      }
                    pset_.push_back({
                        (int) std::distance(k_aps.begin(),
                                            std::find(k_aps.begin(),
                                                      k_aps.end(), right)),
                          VAR_OP_EQ,  ei->second});
                    continue;
                  }
              }
          }

        // Here Left is known. Just detect cases where left is digit there is
        // no right part.
        if (left_is_digit && right.empty())
          {
            ap_error = ap;
            goto error_ap_unknown;
          }

        assert(!right.empty() && !op.empty());

        // Parse right part of the atomic proposition
        // Check if it is a known atomic proposition
        it = std::find(k_aps.begin(), k_aps.end(), right);
        if (it != k_aps.end())
          {
            // The aps is directly an AP of the system, we will just
            // have to detect if the variable is 0 or not.
            rval = std::distance(k_aps.begin(), it);
          }
        else
          {
            // We are is the right part, so  if it is a process state
            // we do not know how to interpret (xxx == P.state1). Abort
            std::size_t found_dot = right.find_first_of('.');
            if (std::string::npos != found_dot)
              {
                ap_error = left + op;
                err << "\nOperation " << op  << " in \"" << ap_error
                    << "\" is not available for process's state"
                    << " (i.e. " <<  right << ")\n";
                ++errors;
                continue;
              }
            else
              {
                // Finally, it's a number
                right_is_digit = true;
                for (auto c: right)
                  if (!isdigit(c))
                    right_is_digit = false;

                if (right_is_digit)
                  rval = std::strtol (right.c_str(), nullptr, 10);
                else
                  {
                    if (std::string::npos != left.find_first_of('.'))
                      {
                        err << "\nProposition \"" << ap
                            << "\" cannot be interpreted"
                            << " (i.e. " << op + right << " is not valid)\n";
                        ++errors;
                        continue;
                      }

                    // or something like: P_0 == State1
                    auto ni = matcher.find(left);
                    if (ni == matcher.end())
                      {

                        ap_error = left;
                        goto error_ap_unknown;
                      }
                    int type_num = ni->second;
                    enum_map_t::const_iterator ei =
                      enum_map[type_num].find(right);
                    if (ei == enum_map[type_num].end())
                      {
                        ap_error = right;
                        goto error_ap_unknown;
                      }
                    pset_.push_back({
                        (int) std::distance(k_aps.begin(),
                                            std::find(k_aps.begin(),
                                                      k_aps.end(), left)),
                          VAR_OP_EQ,  ei->second});
                    continue;
                  }
              }
          }

        if (left_is_digit && right_is_digit)
          {
            err << "\nOperation \"" << op
                << "\" between two numbers not available"
                << " (i.e. " << right << " and, "
                << left << ")\n";
            ++errors;
            continue;
          }

        // Left and Right are know, just analyse the operator.
        if (op.compare("==") == 0)
          oper = !left_is_digit && !right_is_digit? VAR_OP_EQ_VAR :
            (left_is_digit? OP_EQ_VAR : VAR_OP_EQ);
        else if (op.compare("!=") == 0)
          oper = !left_is_digit && !right_is_digit? VAR_OP_NE_VAR :
            (left_is_digit? OP_NE_VAR : VAR_OP_NE);
        else if (op.compare("<") == 0)
          oper = !left_is_digit && !right_is_digit? VAR_OP_LT_VAR :
            (left_is_digit? OP_LT_VAR : VAR_OP_LT);
        else if (op.compare(">") == 0)
          oper = !left_is_digit && !right_is_digit? VAR_OP_GT_VAR :
            (left_is_digit? OP_GT_VAR : VAR_OP_GT);
        else if (op.compare("<=") == 0)
          oper = !left_is_digit && !right_is_digit? VAR_OP_LE_VAR :
            (left_is_digit? OP_LE_VAR : VAR_OP_LE);
        else if (op.compare(">=") == 0)
          oper = !left_is_digit && !right_is_digit? VAR_OP_GE_VAR :
            (left_is_digit? OP_GE_VAR : VAR_OP_GE);
        else
          {
            err << "\nOperation \"" << op
                << "\" is unknown\n";
            ++errors;
            continue;
          }

        pset_.push_back({lval, oper, rval});
        continue;

      error_ap_unknown:
        err << "\nProposition \"" << ap_error << "\" does not exist\n";
        ++errors;
        continue;
      }

    if (errors)
      throw std::runtime_error(err.str());
  }

  ltsmin_kripkecube_ptr
  ltsmin_model::kripkecube(std::vector<std::string> to_observe,
                           const formula dead, int compress,
                           unsigned int nb_threads) const
  {
    // Register the "dead" proposition.  There are three cases to
    // consider:
    //  * If DEAD is "false", it means we are not interested in finite
    //    sequences of the system.
    //  * If DEAD is "true", we want to check finite sequences as well
    //    as infinite sequences, but do not need to distinguish them.
    //  * If DEAD is any other string, this is the name a property
    //    that should be true when looping on a dead state, and false
    //    otherwise.
    std::string dead_ap = "";
    bool selfloopize = true;
    if (dead == spot::formula::ff())
      selfloopize = false;
    else if (dead != spot::formula::tt())
      dead_ap = dead.ap_name();

    // Is dead proposition is already in to_observe?
    bool add_dead = true;
    for (auto it: to_observe)
      if (it.compare(dead_ap))
        add_dead = false;

    if (dead_ap.compare("") != 0 && add_dead)
      to_observe.push_back(dead_ap);

    // Finally build the system.
    return std::make_shared<spot::kripkecube<spot::cspins_state,
                                             spot::cspins_iterator>>
      (iface, compress, to_observe, selfloopize, dead_ap, nb_threads);
  }

  kripke_ptr
  ltsmin_model::kripke(const atomic_prop_set* to_observe,
                       bdd_dict_ptr dict,
                       const formula dead, int compress) const
  {
    spot::prop_set* ps = new spot::prop_set;
    try
      {
        convert_aps(to_observe, iface, dict, dead, *ps);
      }
    catch (std::runtime_error)
      {
        delete ps;
        dict->unregister_all_my_variables(iface.get());
        throw;
      }
    auto res = std::make_shared<spins_kripke>(iface, dict, ps, dead, compress);
    // All atomic propositions have been registered to the bdd_dict
    // for iface, but we also need to add them to the automaton so
    // twa::ap() works.
    for (auto ap: *to_observe)
      res->register_ap(ap);
    return res;
  }

  ltsmin_model::~ltsmin_model()
  {
  }

  std::tuple<bool, std::string, std::vector<istats>>
  ltsmin_model::modelcheck(ltsmin_kripkecube_ptr sys,
                           spot::twacube_ptr twa, bool compute_ctrx)
  {
    // Must ensure that the two automata are working on the same
    // set of atomic propositions.
    assert(sys->get_ap().size() == twa->get_ap().size());
    for (unsigned int i = 0; i < sys->get_ap().size(); ++i)
      assert(sys->get_ap()[i].compare(twa->get_ap()[i]) == 0);

    bool stop = false;
    std::vector<ec_renault13lpar<cspins_state, cspins_iterator,
                                 cspins_state_hash, cspins_state_equal>> ecs;
    for (unsigned i = 0; i < sys->get_threads(); ++i)
      ecs.push_back({*sys, twa, i, stop});

    std::vector<std::thread> threads;
    for (unsigned i = 0; i < sys->get_threads(); ++i)
      threads.push_back
        (std::thread(&ec_renault13lpar<cspins_state, cspins_iterator,
                     cspins_state_hash, cspins_state_equal>::run, &ecs[i]));

    for (unsigned i = 0; i < sys->get_threads(); ++i)
      threads[i].join();

    bool has_ctrx = false;
    std::string trace = "";
    std::vector<istats> stats;
    for (unsigned i = 0; i < sys->get_threads(); ++i)
      {
        has_ctrx |= ecs[i].counterexample_found();
        if (compute_ctrx && ecs[i].counterexample_found()
            && trace.compare("") == 0)
          trace = ecs[i].trace(); // Pick randomly one !
        stats.push_back(ecs[i].stats());
      }
    return std::make_tuple(has_ctrx, trace, stats);
  }

  int ltsmin_model::state_size() const
  {
    return iface->get_state_size();
  }

  const char* ltsmin_model::state_variable_name(int var) const
  {
    return iface->get_state_variable_name(var);
  }

  int ltsmin_model::state_variable_type(int var) const
  {
    return iface->get_state_variable_type(var);
  }

  int ltsmin_model::type_count() const
  {
    return iface->get_type_count();
  }

  const char* ltsmin_model::type_name(int type) const
  {
    return iface->get_type_name(type);
  }

  int ltsmin_model::type_value_count(int type)
  {
    return iface->get_type_value_count(type);
  }

  const char* ltsmin_model::type_value_name(int type, int val)
  {
    return iface->get_type_value_name(type, val);
  }
}
