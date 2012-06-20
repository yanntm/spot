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

/// FIXME: Add
/// - a bit-state hashing version.

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
#include "tau03.hh"
#include "ndfs_result.hxx"

namespace spot
{
  namespace
  {
    enum color {WHITE, BLUE};

    /// \brief Emptiness checker on spot::tgba automata having at most one
    /// acceptance condition (i.e. a TBA).
    template <typename heap>
    class tau03_search : public emptiness_check, public ec_statistics
    {
    public:
      /// \brief Initialize the search algorithm on the automaton \a a
      tau03_search(const tgba *a, size_t size, option_map o,
		   bool is_dyn = false, bool is_stat = false)
        : emptiness_check(a, o),
          h(size),
          all_cond(a->all_acceptance_conditions()),
	  is_dynamic(is_dyn),
	  is_static(is_stat)
      {
        assert(a->number_of_acceptance_conditions() > 0);
	assert((is_static && !is_dynamic) ||
	       (!is_static && is_dynamic) ||
	       (!is_static && !is_dynamic));

	// FIXME : Here we consider that for multiple acceptance 
	// conditions (in case of a weak SCC) all transitions are
	// labelled by ALL acceptance conditions 
      }

      virtual ~tau03_search()
      {
        // Release all iterators on the stacks.
        while (!st_blue.empty())
          {
            h.pop_notify(st_blue.front().s);
            delete st_blue.front().it;
            st_blue.pop_front();
          }
        while (!st_red.empty())
          {
            h.pop_notify(st_red.front().s);
            delete st_red.front().it;
            st_red.pop_front();
          }
      }

      /// \brief Perform a Magic Search.
      ///
      /// \return non null pointer iff the algorithm has found an
      /// accepting path.
      virtual emptiness_check_result* check()
      {
        if (!st_blue.empty())
            return 0;

	const state* s0 = a_->get_init_state();
	inc_states();

	// It's the static case : that means that the algorithm 
	// is chosen regarding only the first state of the automata
	if (is_static)
	  {
	    // Trap only guarantee properties 
	    if (es_->is_guarantee(s0))
	      {
		inc_reachability();
		commut_algo(REACHABILITY);
		h.add_new_state(s0, BLUE);
		push(st_blue, s0, bddfalse, bddfalse);
		if (static_guarantee ())
		  return
		    new ndfs_result<tau03_search<heap>, heap>(*this,
							      is_dynamic);
		else
		  return 0;
	      }
	    // Trap only persistence properties 
	    else if  (es_->is_persistence(s0))
	      {
		inc_dfs();
		commut_algo(DFS);
		h.add_new_state(s0, BLUE);
		push(st_blue, s0, bddfalse, bddfalse);
		if (static_persistence ())
		  return
		    new ndfs_result<tau03_search<heap>, heap>(*this,
							      is_dynamic);
		else
		  return 0;
	      }
	    // We are in the general case apply default algo.
	    else
	      is_static = false;
	  }

	if (is_dynamic)
	  {
	    stats_commut (s0);
	    stats_formula (s0);
	  }
	else
	  {
	    inc_ndfs();
	    commut_algo(NDFS);
	  }

	h.add_new_state(s0, BLUE);
	push(st_blue, s0, bddfalse, bddfalse);
	if (dfs_blue())
	  return new ndfs_result<tau03_search<heap>, heap>(*this, is_dynamic);
	return 0;
      }

      virtual std::ostream& print_stats(std::ostream &os) const
      {
        os << states() << " distinct nodes visited" << std::endl;
        os << transitions() << " transitions explored" << std::endl;
        os << max_depth() << " nodes for the maximal stack depth" << std::endl;
        return os;
      }

      const heap& get_heap() const
        {
          return h;
        }

      const stack_type& get_st_blue() const
        {
          return st_blue;
        }

      const stack_type& get_st_red() const
        {
          return st_red;
        }

    private:
      void push(stack_type& st, const state* s,
                        const bdd& label, const bdd& acc)
      {
        inc_depth();
        tgba_succ_iterator* i = a_->succ_iter(s);
        i->first();
        st.push_front(stack_item(s, i, label, acc));
      }

      void pop(stack_type& st)
      {
        dec_depth();
        delete st.front().it;
        st.pop_front();
      }

      /// \brief Stack of the blue dfs.
      stack_type st_blue;

      /// \brief Stack of the red dfs.
      stack_type st_red;

      /// \brief Map where each visited state is colored
      /// by the last dfs visiting it.
      heap h;

      /// The unique acceptance condition of the automaton \a a.
      bdd all_cond;

      // This allows to know if a dynamic algorithm should be perform
      bool is_dynamic;

      // This allows to know if a static algorithm should be perform
      bool is_static;

      void
      stats_formula (const spot::state *s)
      {
	if (es_->is_guarantee(s))
	  inc_reachability();
	else if (es_->is_persistence(s))
	  inc_dfs();
	else
	  inc_ndfs();
      }

      void
      stats_commut (const spot::state *s)
      {
	if (es_->is_guarantee(s))
	  commut_algo (REACHABILITY);
	else if (es_->is_persistence(s))
	  commut_algo (DFS);
	else
	  commut_algo(NDFS);
      }

      /// THis function perform the search on a a tgba which is the 
      /// result of a Kripke structure and a a formula : this formula 
      /// is necessary a guarantee formula
      ///
      /// Assume that the Kripke structure is such that every state 
      /// has at least one outgoing edge
      bool
      static_guarantee ()
      {
        while (!st_blue.empty())
          {
            stack_item& f = st_blue.front();
	    trace << "static guarantee treats: " << a_->format_state(f.s) << std::endl;
            if (!f.it->done())
              {
                const state *s_prime = f.it->current_state();
		trace  << "  Visit the successor: "
		       << a_->format_state(s_prime) << std::endl;
                bdd label = f.it->current_condition();
                bdd acc = f.it->current_acceptance_conditions();
                // Go down the edge (f.s, <label, acc>, s_prime)
		// Here it's the previous algorithm 
                typename heap::color_ref c_prime = h.get_color_ref(s_prime);

		// For the sake of dynamism
		if (is_dynamic && !es_->is_guarantee(f.s))
		  {
		    //s_prime->destroy();
		    return false;
		  }

		f.it->next();
		inc_transitions();

		if (es_->is_terminal_accepting_scc (f.s))
		  {
		    if (c_prime.is_white ())
		      {
			inc_states();
			inc_reachability();
			h.add_new_state(s_prime, BLUE);
		      }
		    push(st_blue, s_prime, label, acc);
		    is_dynamic = true;
		    return true;
		  }
		else
                if (c_prime.is_white())
                  {
                    trace << "  It is white, go down" << std::endl;
                    inc_states();
		    inc_reachability();
                    h.add_new_state(s_prime, BLUE);
                    push(st_blue, s_prime, label, acc);
                  }
		else
                  {
		    trace << " no propagation is needed, pop it."
			  << std::endl;
		    h.pop_notify(s_prime);
		  }
              }
            else
            // Backtrack the edge
            //        (predecessor of f.s in st_blue, <f.label, f.acc>, f.s)
              {
                typename heap::color_ref c = h.get_color_ref(f.s);
                assert(!c.is_white());
		//c.set_color(BLUE);
		pop(st_blue);

              }
          }
        return false;
      }

      /// THis function perform the search on a a tgba which is the 
      /// result of a Kripke structure and a a formula : this formula 
      /// may be a guarantee formula or a persistence formula and is 
      /// represented by a weak or a terminal automata
      /// 
      /// If no assumption is done about the Kripke this algorithm is the
      /// one we should use over guarantee formula
      bool
      static_persistence ()
      {
	assert (is_static || is_dynamic);
	assert (es_ != 0);
        while (!st_blue.empty())
          {
            stack_item& f = st_blue.front();
	    trace << "PERSISTENCE treats: "
		  << a_->format_state(f.s) << std::endl;
            if (!f.it->done())
              {
                const state *s_prime = f.it->current_state();
		trace << "  Visit the successor: "
                      << a_->format_state(s_prime) << std::endl;
                bdd label = f.it->current_condition();
                bdd acc = f.it->current_acceptance_conditions();
                // Go down the edge (f.s, <label, acc>, s_prime)

		// Trap all states that represents guarantee formulas
		// for dynamism 
		// 
		// In the case of static algorithms this is not performed
		bool inc_me = true;
		if (is_dynamic && es_->is_guarantee(f.s))
		  {
		    if (static_guarantee ())
		      {
			s_prime->destroy();
			return true;
		      }
		    else
		      {
			if (st_blue.empty())
			  {
			    s_prime->destroy();
			    return false;
			  }
			inc_me = false;
			continue;
		      }
		  }

		// For the sake of dynamism
		if (is_dynamic && !es_->is_persistence(f.s))
		  {
		    trace << "Backtrack from persistence\n";
		    s_prime->destroy();
		    return false;
		  }

		if (inc_me)
		  {
		    f.it->next();
		    inc_transitions();
		  }


		// We reach the most important part of the algorithm 
		// for persistence : we check if the current state and 
		// its successors are in the same SCC in the formula automaton
		bool has_been_visited = false;
		if (es_->same_weak_acc (f.s, s_prime) && acc == all_cond)
		  {
		    // If Yes we check wether this state is already in the
		    // blue stack
		    stack_type::const_reverse_iterator i;
		    i = st_blue.rbegin();

		    trace << "This 2 states are in the same SCC : "
			  << a_->format_state(f.s) << "     "
			  << a_->format_state(s_prime) << std::endl;
		    while (i != st_blue.rend())
		      {
			if (i->s->compare(s_prime) == 0)
			  {
			    has_been_visited = true;
			    break;
			  }
			++i;
		      }
		    if (has_been_visited)
		      {
			s_prime->destroy();
			is_dynamic = true;
			return true;
		      }
		  }

                typename heap::color_ref c = h.get_color_ref(s_prime);
		if (c.is_white())
		  {
		    inc_states();
		    inc_dfs();
		    h.add_new_state(s_prime, BLUE);
		    push(st_blue, s_prime, label, acc);
		    continue;
		  }
		else
		  {
		    h.pop_notify(s_prime);
		  }
		continue;
	      }
            else
              {
                typename heap::color_ref c = h.get_color_ref(f.s);
                assert(!c.is_white());
                tgba_succ_iterator* i = a_->succ_iter(f.s);
                for (i->first(); !i->done(); i->next())
                  {
                    inc_transitions();
                   const state *s_prime = i->current_state();
                   trace << "DFS_BLUE rescanning the arc from "
                         << a_->format_state(f.s) << "  to "
                         << a_->format_state(s_prime) << std::endl;
                    bdd label = i->current_condition();
                    bdd acc = i->current_acceptance_conditions();
                    typename heap::color_ref c_prime =
		      h.get_color_ref(s_prime);

		    // In this case we have already visit this sub system we 
		    // don't need to go further
		    if (is_dynamic && c_prime.is_white())
		      {
			break;
		      }

                    assert(!c_prime.is_white());
                    bdd acu = acc | c.get_acc();
                    if ((c_prime.get_acc() & acu) != acu)
                      {
                        trace << "  a propagation is needed, go down"
                              << std::endl;
                        c_prime.cumulate_acc(acu);
                        push(st_red, s_prime, label, acc);
                        dfs_red(acu);
                     }
                  }
                delete i;
                if (c.get_acc() == all_cond)
                  {
                    trace << "DFS_BLUE propagation is successful, report a"
                          << " cycle" << std::endl;
		    is_dynamic = false;
                    return true;
                  }
                else
                  {
                    trace << "DFS_BLUE propagation is unsuccessful, pop it"
                          << std::endl;
                    h.pop_notify(f.s);
                    pop(st_blue);
                 }

		//		pop(st_blue);
	      }
	  }
        return false;
      }


      /// Override previous declaration in emptiness.h 
      /// This is used to detect dynamic application of the algorithm
      /// Nota Bene : static emptiness are considered as dynamic emptiness
      /// in the sense that they could leads to prefix computation only
      virtual bool
      is_dynamic_emptiness ()
      {
	return is_dynamic || is_static;
      }


      bool dfs_blue()
      {
	if (is_dynamic)
	  assert (es_ != 0);
        while (!st_blue.empty())
          {
            stack_item& f = st_blue.front();
            trace << "DFS_BLUE treats: " << a_->format_state(f.s) << std::endl;
            if (!f.it->done())
              {
                const state *s_prime = f.it->current_state();
                trace << "  Visit the successor: "
                      << a_->format_state(s_prime) << std::endl;
                bdd label = f.it->current_condition();
                bdd acc = f.it->current_acceptance_conditions();
                // Go down the edge (f.s, <label, acc>, s_prime)

		bool inc_me = true;

		// There it's the inclusion of dynamism : this use 
		// the same function that static one
		if (is_dynamic)
		  {
		    stats_commut (f.s);

		    // Trap all states that represents guarantee formulas
		    if (es_->is_guarantee(f.s))
		      {
			if (static_guarantee ())
			  {
			    s_prime->destroy();
			    return true;
			  }
			else
			  {
			    if (st_blue.empty())
			      {
				s_prime->destroy();
				return false;
			      }
			    inc_me = false;
			    continue;
			  }
		      }

		    // Trap all states that represents persistence formula
		    // Persistence formula becoming guarantee formula are 
		    // trapped by the static_persistence algorithm in case 
		    // of dynamism
		    else if (es_->is_persistence(f.s))
		      {
			if (static_persistence ())
			  {
			    s_prime->destroy();
			    return true;
			  }
			else
			  {
			    if (st_blue.empty())
			      {
				s_prime->destroy();
				return false;
			      }
			    inc_me = false;
			    continue;
			  }
		      }
		  }
		else
		  commut_algo(NDFS);

		if (inc_me)
		  {
		    f.it->next();
		    inc_transitions();
		  }

		// Here it's the previous algorithm 
                typename heap::color_ref c_prime = h.get_color_ref(s_prime);
                if (c_prime.is_white())
                  {
                    trace << "  It is white, go down" << std::endl;
                    inc_states();

		    if (is_dynamic)
		      {
			stats_formula (s_prime);
		      }
		    else
		      inc_ndfs();

                    h.add_new_state(s_prime, BLUE);
                    push(st_blue, s_prime, label, acc);
                  }
                else
                  {
                    trace << "  It is blue, pop it" << std::endl;
                    h.pop_notify(s_prime);
                  }
              }
            else
            // Backtrack the edge
            //        (predecessor of f.s in st_blue, <f.label, f.acc>, f.s)
              {
                trace << "  All the successors have been visited"
                      << ", rescan this successors"
                      << std::endl;
                typename heap::color_ref c = h.get_color_ref(f.s);
                assert(!c.is_white());
                tgba_succ_iterator* i = a_->succ_iter(f.s);
                for (i->first(); !i->done(); i->next())
                  {
                    inc_transitions();
                   const state *s_prime = i->current_state();
                   trace << "DFS_BLUE rescanning the arc from "
                         << a_->format_state(f.s) << "  to "
                         << a_->format_state(s_prime) << std::endl;
                    bdd label = i->current_condition();
                    bdd acc = i->current_acceptance_conditions();
                    typename heap::color_ref c_prime =
		      h.get_color_ref(s_prime);

		    // In this case we have already visit this sub system we 
		    // don't need to go further
		    if (is_dynamic && c_prime.is_white())
		      {
			break;
		      }

                    assert(!c_prime.is_white());
                    bdd acu = acc | c.get_acc();
                    if ((c_prime.get_acc() & acu) != acu)
                      {
                        trace << "  a propagation is needed, go down"
                              << std::endl;
                        c_prime.cumulate_acc(acu);
                        push(st_red, s_prime, label, acc);
                        dfs_red(acu);
                     }
                  }
                delete i;
                if (c.get_acc() == all_cond)
                  {
                    trace << "DFS_BLUE propagation is successful, report a"
                          << " cycle" << std::endl;
		    is_dynamic = false;
                    return true;
                  }
                else
                  {
                    trace << "DFS_BLUE propagation is unsuccessful, pop it"
                          << std::endl;
                    h.pop_notify(f.s);
                    pop(st_blue);
                 }
              }
          }
        return false;
      }

      void dfs_red(const bdd& acu)
      {
        assert(!st_red.empty());

        while (!st_red.empty())
          {
            stack_item& f = st_red.front();
            trace << "DFS_RED treats: " << a_->format_state(f.s) << std::endl;
            if (!f.it->done())
              {
                const state *s_prime = f.it->current_state();
                trace << "  Visit the successor: "
                      << a_->format_state(s_prime) << std::endl;
                bdd label = f.it->current_condition();
                bdd acc = f.it->current_acceptance_conditions();
                // Go down the edge (f.s, <label, acc>, s_prime)
                f.it->next();
                inc_transitions();
                typename heap::color_ref c_prime = h.get_color_ref(s_prime);
                if (c_prime.is_white())
                  {
                    trace << "  It is white, pop it" << std::endl;
                    s_prime->destroy();
                  }
                 else if ((c_prime.get_acc() & acu) != acu)
                  {
                    trace << "  It is blue and propagation "
                          << "is needed, go down" << std::endl;
                    c_prime.cumulate_acc(acu);
                    push(st_red, s_prime, label, acc);
                  }
                else
                  {
                    trace << "  It is blue and no propagation "
                          << "is needed, pop it" << std::endl;
                    h.pop_notify(s_prime);
                  }
              }
            else // Backtrack
              {
                trace << "  All the successors have been visited, pop it"
                      << std::endl;
                h.pop_notify(f.s);
                pop(st_red);
              }
          }
      }

    };

    class explicit_tau03_search_heap
    {
    public:
      class color_ref
      {
      public:
        color_ref(color* c, bdd* a) :p(c), acc(a)
          {
          }
        color get_color() const
          {
            return *p;
          }
        void set_color(color c)
          {
            assert(!is_white());
            *p = c;
          }
        const bdd& get_acc() const
          {
            assert(!is_white());
            return *acc;
          }
        void cumulate_acc(const bdd& a)
          {
            assert(!is_white());
            *acc |= a;
          }
        bool is_white() const
          {
            return p == 0;
          }
      private:
        color *p;
        bdd* acc;
      };

      explicit_tau03_search_heap(size_t)
        {
        }

      ~explicit_tau03_search_heap()
        {
          hash_type::const_iterator s = h.begin();
          while (s != h.end())
            {
              // Advance the iterator before deleting the "key" pointer.
              const state* ptr = s->first;
              ++s;
              ptr->destroy();
            }
        }

      color_ref get_color_ref(const state*& s)
        {
          hash_type::iterator it = h.find(s);
          if (it == h.end())
            return color_ref(0, 0);
          if (s != it->first)
            {
              s->destroy();
              s = it->first;
            }
          return color_ref(&it->second.first, &it->second.second);
        }

      void add_new_state(const state* s, color c)
        {
          assert(h.find(s) == h.end());
          h.insert(std::make_pair(s, std::make_pair(c, bddfalse)));
        }

      void pop_notify(const state*) const
        {
        }

      bool has_been_visited(const state* s) const
        {
          hash_type::const_iterator it = h.find(s);
          return (it != h.end());
        }

      enum { Has_Size = 1 };
      int size() const
        {
          return h.size();
        }
    private:

      typedef Sgi::hash_map<const state*, std::pair<color, bdd>,
                state_ptr_hash, state_ptr_equal> hash_type;
      hash_type h;
    };

  } // anonymous

  emptiness_check* explicit_tau03_search(const tgba *a, option_map o)
  {
    return new tau03_search<explicit_tau03_search_heap>(a, 0, o);
  }

  emptiness_check* explicit_tau03_dyn_search(const tgba *a, option_map o)
  {
    return new tau03_search<explicit_tau03_search_heap>(a, 0, o, true, false);
  }

  emptiness_check* explicit_tau03_stat_search(const tgba *a, option_map o)
  {
    return new tau03_search<explicit_tau03_search_heap>(a, 0, o, false, true);
  }
}
