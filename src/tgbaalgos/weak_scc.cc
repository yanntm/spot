// Copyright (C) 2011 Laboratoire de Recherche et Developpement de
// l'Epita (LRDE).
// Copyright (C) 2004, 2005  Laboratoire d'Informatique de Paris 6 (LIP6),
// département Systèmes Répartis Coopératifs (SRC), Université Pierre
// et Marie Curie.
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

//#define TRACE

#include <iostream>
#ifdef TRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif

#include <cassert>
#include <list>
#include "misc/hash.hh"
#include "tgba/tgba.hh"
#include "emptiness.hh"
#include "emptiness_stats.hh"
#include "weak_scc.hh"
#include "ndfs_result.hxx"
#include "misc/accconv.hh"
#include "tgba/bddprint.hh"
#include "tgbaalgos/scc.hh"

namespace spot
{
  namespace
  {
    enum color {WHITE, CYAN, BLUE, RED};

    /// \brief Emptiness checker on spot::tgba automata having at most one
    /// acceptance condition (i.e. a TBA).
    template <typename heap>
    class weak_scc_search :  public weak_scc
    {
    private :
      const spot::tgba* a_;

      scc_map sm_;

    public:
      weak_scc_search(const tgba *a, size_t size, scc_map sc)
        : a_(a),
	  sm_(sc),
          h(size)
      {
      }

      virtual bool
      check_weak_accepting(const state * s)
      {
	trace << "check_weak_accepting : "
		  << a_->format_state(s)
		  << std::endl;

	bdd all = a_->all_acceptance_conditions();

	// Walk all Acceptance conditions
	while (all != bddfalse)
	  {
	    h.clear();
	    to_process.clear();
	    next.clear();

	    // Grab one of the conditions
	    bdd cond = bdd_satone(all);
	    trace << "Processing condition : "
		      << bdd_format_accset(a_->get_dict(), cond)
		      << std::endl;
	    if (!cycle_wo_acc (s , cond))
	      {
		return false;
	      }
	    // Remove this condition
	    all -= bdd_satone(all);
	  }
	return true;
      }

      bool
      cycle_wo_acc (const state *start, bdd current)
      {
	push (next, start);
	//while (next.size())
	while (!next.empty())
	  {
	    stack_item &item = next.front();
	    const state *s = item.s;
	    //item.it->current_state();//start;//next.pop();
	    typename heap::color_ref c = h.get_color_ref(s);
	    pop(next);
	    if (c.is_white())
	      {
		if (dfs(s, current))
 		  return false;
	      }
 	  }
	return true;
      }


      bool
      dfs(const state *s, bdd current)
      {

	h.add_new_state(s, CYAN);
	push (to_process, s);

	while (!to_process.empty())
          {
            stack_item& f = to_process.front();
	    typename heap::color_ref c = h.get_color_ref(s);
	    unsigned actual_scc = sm_.scc_of_state (s);

	    if (c.is_white())
	      {
		c.set_color(CYAN);
	      }

            if (!f.it->done())
              {
                const state *s_prime = f.it->current_state();
                bdd acc = f.it->current_acceptance_conditions();

 		f.it->next();
		unsigned scc = sm_.scc_of_state (s_prime);

		trace << "Working on edge ("
			  << a_-> format_state(f.s)
			  << " , "
			  << a_-> format_state(s_prime)
			  << " )" << std::endl;

		if (scc != actual_scc)
 		  {
		    trace << "     Skiping"
			      << std::endl;
		    continue;
		  }

		// The successor doesn't belong to the same scc 
		// or the acceptance condition is not the good one 
 		if ((current & acc) == current)
 		  {
		    trace << "     Skiping"
			      << std::endl;
		    push (next , s);
		    continue;
		  }
		else
		  {
		    typename heap::color_ref cc = h.get_color_ref(s_prime);
		    if (cc.is_white())
		      {
			h.add_new_state(s_prime, CYAN);
			push(to_process, s_prime);
		      }
		    else if (cc.get_color() == CYAN)
		      {
			return true;
		      }
		  }
	      }
	    else
	      {
		c.set_color(BLUE);
		h.pop_notify(f.s);
		pop (to_process);
	      }
	  }

	return false;
      }

      // 
      // 
      const spot::tgba* automaton() const
      {
	return a_;
      }

      virtual ~weak_scc_search()
      {
        // Release all iterators on the stacks.
        while (!to_process.empty())
          {
            h.pop_notify(to_process.front().s);
            delete to_process.front().it;
            to_process.pop_front();
          }
      }

    private:

      void push(stack_type& st, const state* s)
      {
        tgba_succ_iterator* i = a_->succ_iter(s);
        i->first();
        st.push_front(stack_item(s, i, bddfalse, bddfalse));
      }

      void pop(stack_type& st)
      {
        delete st.front().it;
        st.pop_front();
      }

      /// 
      stack_type next;

      stack_type to_process;


      /// \brief Map where each visited state is colored
      /// by the last dfs visiting it.
      heap h;
    };

    class explicit_weak_scc_search_heap
    {
      typedef Sgi::hash_set<const state*,
                state_ptr_hash, state_ptr_equal> hcyan_type;
      typedef Sgi::hash_map<const state*, color,
                state_ptr_hash, state_ptr_equal> hash_type;
    public:
      enum { Safe = 1 };

      class color_ref
      {
      public:
        color_ref(hash_type* h, hcyan_type* hc, const state* s)
          : is_cyan(true), ph(h), phc(hc), ps(s), pc(0)
          {
          }
        color_ref(color* c)
          : is_cyan(false), ph(0), phc(0), ps(0), pc(c)
          {
          }
        color get_color() const
          {
            if (is_cyan)
              return CYAN;
            return *pc;
          }
        void set_color(color c)
          {
            assert(!is_white());
            if (is_cyan)
              {
                assert(c != CYAN);
                int i = phc->erase(ps);
                assert(i == 1);
                (void)i;
                ph->insert(std::make_pair(ps, c));
              }
            else
              {
                *pc=c;
              }
          }
        bool is_white() const
          {
            return !is_cyan && pc == 0;
          }

	void clear()
	{
	  phc->clear();
	  ph->clear();
	}
      private:
        bool is_cyan;
        hash_type* ph; //point to the main hash table
        hcyan_type* phc; // point to the hash table hcyan
        const state* ps; // point to the state in hcyan
        color *pc; // point to the color of a state stored in main hash table
      };

      explicit_weak_scc_search_heap(size_t)
        {
        }

      ~explicit_weak_scc_search_heap()
        {
          hcyan_type::const_iterator sc = hc.begin();
          while (sc != hc.end())
            {
              const state* ptr = *sc;
              ++sc;
              ptr->destroy();
            }
          hash_type::const_iterator s = h.begin();
          while (s != h.end())
            {
              const state* ptr = s->first;
              ++s;
              ptr->destroy();
            }
        }

      color_ref get_color_ref(const state*& s)
        {
          hcyan_type::iterator ic = hc.find(s);
          if (ic == hc.end())
            {
              hash_type::iterator it = h.find(s);
              if (it == h.end())
                return color_ref(0); // white state
              if (s != it->first)
                {
                  s->destroy();
                  s = it->first;
                }
              return color_ref(&it->second); // blue or red state
            }
          if (s != *ic)
            {
              s->destroy();
              s = *ic;
            }
          return color_ref(&h, &hc, *ic); // cyan state
        }

      void add_new_state(const state* s, color c)
        {
          assert(hc.find(s) == hc.end() && h.find(s) == h.end());
          if (c == CYAN)
            hc.insert(s);
          else
            h.insert(std::make_pair(s, c));
        }

      void pop_notify(const state*) const
        {
        }

      bool has_been_visited(const state* s) const
        {
          hcyan_type::const_iterator ic = hc.find(s);
          if (ic == hc.end())
            {
              hash_type::const_iterator it = h.find(s);
              return (it != h.end());
            }
          return true;
        }

      void clear()
      {
	h.clear();
	hc.clear();
      }

      enum { Has_Size = 1 };
      int size() const
        {
          return h.size() + hc.size();
        }

    private:

      hash_type h; // associate to each blue and red state its color
      hcyan_type hc; // associate to each cyan state its weight
    };



  } // anonymous

  weak_scc* weak_scc_computer(const tgba *a, scc_map sm)
  {
    return new weak_scc_search<explicit_weak_scc_search_heap>(a, 0, sm);
  }
}
