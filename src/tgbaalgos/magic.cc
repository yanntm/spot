// Copyright (C) 2011 Laboratoire de recherche et développement de
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
#include "magic.hh"
#include "ndfs_result.hxx"
#include "ltlast/constant.hh"

namespace spot
{
  namespace
  {
    enum color {WHITE, BLUE, RED};

    /// \brief Emptiness checker on spot::tgba automata having at most one
    /// acceptance condition (i.e. a TBA).
    template <typename heap>
    class magic_search_ : public emptiness_check, public ec_statistics
    {
    public:
      /// \brief Initialize the Magic Search algorithm on the automaton \a a
      ///
      /// \pre The automaton \a a must have at most one acceptance
      /// condition (i.e. it is a TBA).
      magic_search_(const tgba *a, size_t size, option_map o = option_map(),
		    bool dyn = false, bool stat = false)
        : emptiness_check(a, o),
          h(size),
          all_cond(a->all_acceptance_conditions()),
	  is_dynamic(dyn),
	  is_static (stat)
      {
        assert(a->number_of_acceptance_conditions() <= 1);
	assert((is_static && !is_dynamic) ||
	       (!is_static && is_dynamic) ||
	       (!is_static && !is_dynamic));
      }

      virtual ~magic_search_()
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


      void
      stats_formula (const ltl::formula *formula)
      {
	if (formula->is_syntactic_guarantee())
	  inc_reachability();
	else if (formula->is_syntactic_persistence())
	  inc_dfs ();
	else
	  inc_ndfs ();
      }

      void
      stats_commut (const ltl::formula *formula)
      {
	if (formula->is_syntactic_guarantee())
	  commut_algo (REACHABILITY);
	else if (formula->is_syntactic_persistence())
	  commut_algo (DFS);
	else
	  commut_algo(NDFS);
      }

      /// \brief Perform a Magic Search.
      ///
      /// \return non null pointer iff the algorithm has found a
      /// new accepting path.
      ///
      /// check() can be called several times (until it returns a null
      /// pointer) to enumerate all the visited accepting paths. The method
      /// visits only a finite set of accepting paths.
      virtual emptiness_check_result* check()
      {
        if (st_red.empty())
          {
            assert(st_blue.empty());
            const state* s0 = a_->get_init_state();
            inc_states();

	    // It's the static case : that means that the algorithm 
	    // is chosen regarding only the first state of the automata
	    if (is_static)
	      {
		// Trap only guarantee properties 
		const ltl::formula * formula =  es_->formula_from_state(s0);
		if (formula->is_syntactic_guarantee())
		  {
		    inc_reachability();
		    commut_algo(REACHABILITY);
		    h.add_new_state(s0, BLUE);
		    push(st_blue, s0, bddfalse, bddfalse);
		    if (static_guarantee ())
		      return new magic_search_result(*this,
						     options(),
						     is_dynamic);
		    else
		      return 0;
		  }
		// Trap only persistence properties 
		else if  (formula->is_syntactic_persistence())
		  {
		    inc_dfs();
		    commut_algo(DFS);
		    h.add_new_state(s0, BLUE);
		    push(st_blue, s0, bddfalse, bddfalse);
		    if (static_persistence ())
		      return new magic_search_result(*this,
						     options(),
						     is_dynamic);
		    else
		      return 0;
		  }
		// We are in the general case apply default algo.
		else
		  is_static = false;
	      }

	    // If it's dynamic update the good stats.
	    if (is_dynamic)
	      {
		const ltl::formula * formula =  es_->formula_from_state(s0);
		stats_commut (formula);
		stats_formula (formula);
	      }
	    else
	      {
		inc_ndfs();
		commut_algo(NDFS);
	      }

            h.add_new_state(s0, BLUE);
            push(st_blue, s0, bddfalse, bddfalse);
            if (dfs_blue())
              return new magic_search_result(*this, options(), is_dynamic);
          }
        else
          {
            h.pop_notify(st_red.front().s);
            pop(st_red);
            if (!st_red.empty() && dfs_red())
              return new magic_search_result(*this, options(), is_dynamic);
            else
              if (dfs_blue())
                return new magic_search_result(*this, options(), is_dynamic);
          }
        return 0;
      }

      virtual std::ostream& print_stats(std::ostream &os) const
      {
        os << states() << " distinct nodes visited" << std::endl;
        os << transitions() << " transitions explored" << std::endl;
        os << max_depth() << " nodes for the maximal stack depth" << std::endl;
        if (!st_red.empty())
          {
            assert(!st_blue.empty());
            os << st_blue.size() + st_red.size() - 1
               << " nodes for the counter example" << std::endl;
          }
        return os;
      }

      virtual bool safe() const
      {
	return heap::Safe;
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

      /// State targeted by the red dfs.
      const state* target;

      /// The unique acceptance condition of the automaton \a a.
      bdd all_cond;

      /// True if the algorithm used the property temporal hierarchy
      bool is_dynamic;

      /// True if the algorigtm used property temporal hierarchy in a 
      /// static way 
      bool is_static;

      /// Override previous declaration in emptiness.h 
      /// This is used to detect dynamic application of the algorithm
      /// Nota Bene : static emptiness are considered as dynamic emptiness
      /// in the sense that they could leads to prefix computation only
      virtual bool
      is_dynamic_emptiness ()
      {
	return is_dynamic || is_static;
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
	assert (is_static || is_dynamic);
	assert (es_ != 0);
        while (!st_blue.empty())
          {
            stack_item& f = st_blue.front();
	    trace << "Guarantee treats: " << a_->format_state(f.s)
		  << std::endl;
            if (!f.it->done())
              {
                const state *s_prime = f.it->current_state();
                trace << "  Visit the successor: "
                      << a_->format_state(s_prime) << std::endl;
                bdd label = f.it->current_condition();
                bdd acc = f.it->current_acceptance_conditions();
                // Go down the edge (f.s, <label, acc>, s_prime)
                typename heap::color_ref c = h.get_color_ref(s_prime);
 		const ltl::formula * formula = 0;
 		formula =  es_->formula_from_state(f.s);

		// For the sake of dynamism
		if (is_dynamic && !formula->is_syntactic_guarantee())
		  {
		    s_prime->destroy();
		    return false;
		  }

                f.it->next();
                inc_transitions();

		// Condition working over kripke having at least one successor
 		if ((ltl::constant::true_instance() == formula))
		  {
		    if (c.is_white ())
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
		  if (c.is_white())
		  {
		    inc_states();
		    inc_reachability();
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
		const ltl::formula * formula = 0;
		formula =  es_->formula_from_state(f.s);

		// Trap all states that represents guarantee formulas
		// for dynamism 
		// 
		// In the case of static algorithms this is not performed
		bool inc_me = true;
		if (is_dynamic && formula->is_syntactic_guarantee())
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
		if (is_dynamic && !formula->is_syntactic_persistence())
		  {
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
		if (es_->same_weak_acc (f.s, s_prime)&& acc == all_cond)
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
                trace << "  All the successors have been visited" << std::endl;
                stack_item f_dest(f);
		pop(st_blue);
		if (is_static)
		  continue;

                typename heap::color_ref c = h.get_color_ref(f_dest.s);
                assert(!c.is_white());
                if (!st_blue.empty() &&
                           f_dest.acc == all_cond && c.get_color() != RED)
                  {
                    // the test 'c.get_color() != RED' is added to limit
                    // the number of runs reported by successive
                    // calls to the check method. Without this
                    // functionnality, the test can be ommited.
                    trace << "  It is blue and the arc from "
                          << a_->format_state(st_blue.front().s)
                          << " to it is accepting, start a red dfs"
                          << std::endl;
                    target = st_blue.front().s;
                    c.set_color(RED);
                    push(st_red, f_dest.s, f_dest.label, f_dest.acc);

		    const ltl::formula * formula = 0;
		    if (is_dynamic)
		      formula =  es_->formula_from_state(f_dest.s);
		    if (is_dynamic &&
			formula->is_syntactic_persistence() &&
			!es_->same_weak_acc (target, f_dest.s))
		      {
			trace << "DFS RED avoid by dynamism\n";
			pop(st_red);
		      }
		    else
		      if (dfs_red())
			{
			  is_dynamic = false;
			  return true;
			}
                  }
                else
                  {
                    trace << "  Pop it" << std::endl;
                    h.pop_notify(f_dest.s);
                  }

		//		pop(st_blue);
	      }
	  }
        return false;
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
		const ltl::formula * formula = 0;
		bool inc_me = true;

		// There it's the inclusion of dynamism : this use 
		// the same function that static one
		if (is_dynamic)
		   {
 		    formula =  es_->formula_from_state(f.s);
		    stats_commut (formula);

		    // Trap all states that represents guarantee formulas
		    if (formula->is_syntactic_guarantee())
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
		    else if (formula->is_syntactic_persistence())
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

		// There it s the classic algorithm for magic with some
		// adds to have statistics about dynamism 
                typename heap::color_ref c = h.get_color_ref(s_prime);
                if (c.is_white())
                  {
                    trace << "  It is white, go down" << std::endl;
                    inc_states();
		    if (is_dynamic)
		      {
			formula =  es_->formula_from_state(s_prime);
			stats_formula (formula);
		      }
		    else
		      inc_ndfs();

                    h.add_new_state(s_prime, BLUE);
                    push(st_blue, s_prime, label, acc);
                  }
                else
                  {
                    if (acc == all_cond && c.get_color() != RED)
                      {
                        // the test 'c.get_color() != RED' is added to limit
                        // the number of runs reported by successive
                        // calls to the check method. Without this
                        // functionnality, the test can be ommited.
                        trace << "  It is blue and the arc is "
                              << "accepting, start a red dfs" << std::endl;

			target = f.s;
			c.set_color(RED);
			push(st_red, s_prime, label, acc);

			if (is_dynamic &&
			    formula->is_syntactic_persistence() &&
			    !es_->same_weak_acc (target, s_prime))
			  {
			    trace << "DFS RED avoid by dynamism\n";
			    pop(st_red);
			  }
			else
			  if (dfs_red())
			    {
			      is_dynamic = false;
			      return true;
			    }
		      }
                    else
                      {
                        trace << "  It is blue or red, pop it" << std::endl;
                        h.pop_notify(s_prime);
                      }
                  }
              }
            else
            // Backtrack the edge
            //        (predecessor of f.s in st_blue, <f.label, f.acc>, f.s)
              {
                trace << "  All the successors have been visited" << std::endl;
                stack_item f_dest(f);
		pop(st_blue);
		if (is_static)
		  continue;

                typename heap::color_ref c = h.get_color_ref(f_dest.s);
                assert(!c.is_white());
                if (!st_blue.empty() &&
                           f_dest.acc == all_cond && c.get_color() != RED)
                  {
                    // the test 'c.get_color() != RED' is added to limit
                    // the number of runs reported by successive
                    // calls to the check method. Without this
                    // functionnality, the test can be ommited.
                    trace << "  It is blue and the arc from "
                          << a_->format_state(st_blue.front().s)
                          << " to it is accepting, start a red dfs"
                          << std::endl;
                    target = st_blue.front().s;
                    c.set_color(RED);
                    push(st_red, f_dest.s, f_dest.label, f_dest.acc);

		    const ltl::formula * formula = 0;
		    if (is_dynamic)
		      formula =  es_->formula_from_state(f_dest.s);
		    if (is_dynamic &&
			formula->is_syntactic_persistence() &&
			!es_->same_weak_acc (target, f_dest.s))
		      {
			trace << "DFS RED avoid by dynamism\n";
			pop(st_red);
		      }
		    else
		      if (dfs_red())
			{
			  is_dynamic = false;
			  return true;
			}
                  }
                else
                  {
                    trace << "  Pop it" << std::endl;
                    h.pop_notify(f_dest.s);
                  }
              }
          }
        return false;
      }

      bool dfs_red()
      {
        assert(!st_red.empty());
        if (target->compare(st_red.front().s) == 0)
          return true;

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
                typename heap::color_ref c = h.get_color_ref(s_prime);
                if (c.is_white())
                  {
                    // If the red dfs find a white here, it must have crossed
                    // the blue stack and the target must be reached soon.
                    // Notice that this property holds only for explicit 
		    // search.
                    // Collisions in bit-state hashing search can also lead
                    // to the visit of white state. Anyway, it is not necessary
                    // to visit white states either if a cycle can be missed
                    // with bit-state hashing search.
                    trace << "  It is white, pop it" << std::endl;
                    s_prime->destroy();
                  }
                else if (c.get_color() == BLUE)
                  {
                    trace << "  It is blue, go down" << std::endl;
                    c.set_color(RED);
                    push(st_red, s_prime, label, acc);
                    if (target->compare(s_prime) == 0)
                      return true;
                  }
                else
                  {
                    trace << "  It is red, pop it" << std::endl;
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
        return false;
      }

      class result_from_stack: public emptiness_check_result,
        public acss_statistics
      {
      public:
        result_from_stack(magic_search_& ms)
          : emptiness_check_result(ms.automaton()), ms_(ms)
        {
        }

        virtual tgba_run* accepting_run()
        {
          assert(!ms_.st_blue.empty());
          //assert(!ms_.st_red.empty());

          tgba_run* run = new tgba_run;

          typename stack_type::const_reverse_iterator i, j, end;
          tgba_run::steps* l;

          const state* target = ms_.st_red.front().s;
	  if (ms_.st_red.empty())
	    target  = 0;

          l = &run->prefix;

          i = ms_.st_blue.rbegin();
          end = ms_.st_blue.rend(); --end;
          j = i; ++j;
          for (; i != end; ++i, ++j)
            {
              if (!(target == 0) &&
		  l == &run->prefix && i->s->compare(target) == 0)
                l = &run->cycle;
              tgba_run::step s = { i->s->clone(), j->label, j->acc };
              l->push_back(s);
            }

	  if (ms_.st_red.empty())
	    {
	      // We are in a reachability and so we don't have cycle
	      return run;
	    }

          if (l == &run->prefix && i->s->compare(target) == 0)
            l = &run->cycle;
	  assert(l == &run->cycle);

          j = ms_.st_red.rbegin();
          tgba_run::step s = { i->s->clone(), j->label, j->acc };
          l->push_back(s);

          i = j; ++j;
          end = ms_.st_red.rend(); --end;
          for (; i != end; ++i, ++j)
            {
              tgba_run::step s = { i->s->clone(), j->label, j->acc };
              l->push_back(s);
            }
          return run;
        }


        unsigned acss_states() const
        {
          return 0;
        }
      private:
        magic_search_& ms_;
      };

#     define FROM_STACK "ar:from_stack"

      class magic_search_result: public emptiness_check_result
      {
      public:
        magic_search_result(magic_search_& m, option_map o = option_map(),
			    bool dyn = false)
          : emptiness_check_result(m.automaton(), o), ms(m), is_dynamic(dyn)
        {
          if (options()[FROM_STACK])
            computer = new result_from_stack(ms);
          else
            computer =
	      new ndfs_result<magic_search_<heap>, heap>(ms, is_dynamic);
        }

        virtual void options_updated(const option_map& old)
        {
          if (old[FROM_STACK] && !options()[FROM_STACK])
            {
              delete computer;
              computer =
		new ndfs_result<magic_search_<heap>, heap>(ms, is_dynamic);
            }
          else if (!old[FROM_STACK] && options()[FROM_STACK])
            {
              delete computer;
              computer = new result_from_stack(ms);
            }
        }

        virtual ~magic_search_result()
        {
          delete computer;
        }

        virtual tgba_run* accepting_run()
        {
          return computer->accepting_run();
        }

        virtual const unsigned_statistics* statistics() const
        {
          return computer->statistics();
        }

      private:
        emptiness_check_result* computer;
        magic_search_& ms;
	bool is_dynamic;
      };
    };

    class explicit_magic_search_heap
    {
    public:
      enum { Safe = 1 };

      class color_ref
      {
      public:
        color_ref(color* c) :p(c)
          {
          }
        color get_color() const
          {
            return *p;
          }
        void set_color(color c)
          {
            assert(!is_white());
            *p=c;
          }
        bool is_white() const
          {
            return p == 0;
          }
      private:
        color *p;
      };

      explicit_magic_search_heap(size_t)
        {
        }

      ~explicit_magic_search_heap()
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
            return color_ref(0);
          if (s != it->first)
            {
              s->destroy();
              s = it->first;
            }
          return color_ref(&it->second);
        }

      void add_new_state(const state* s, color c)
        {
          assert(h.find(s) == h.end());
          h.insert(std::make_pair(s, c));
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

      typedef Sgi::hash_map<const state*, color,
                state_ptr_hash, state_ptr_equal> hash_type;
      hash_type h;
    };

    class bsh_magic_search_heap
    {
    public:
      enum { Safe = 0 };

      class color_ref
      {
      public:
        color_ref(unsigned char *b, unsigned char o): base(b), offset(o*2)
          {
          }
        color get_color() const
          {
            return color(((*base) >> offset) & 3U);
          }
        void set_color(color c)
          {
            *base =  (*base & ~(3U << offset)) | (c << offset);
          }
        bool is_white() const
          {
            return get_color() == WHITE;
          }
      private:
        unsigned char *base;
        unsigned char offset;
      };

      bsh_magic_search_heap(size_t s)
        {
          size_ = s;
          h = new unsigned char[size_];
          memset(h, WHITE, size_);
        }

      ~bsh_magic_search_heap()
        {
          delete[] h;
        }

      color_ref get_color_ref(const state*& s)
        {
          size_t ha = s->hash();
          return color_ref(&(h[ha%size_]), ha%4);
        }

      void add_new_state(const state* s, color c)
        {
          color_ref cr(get_color_ref(s));
          assert(cr.is_white());
          cr.set_color(c);
        }

      void pop_notify(const state* s) const
        {
          s->destroy();
        }

      bool has_been_visited(const state* s) const
        {
          size_t ha = s->hash();
          return color((h[ha%size_] >> ((ha%4)*2)) & 3U) != WHITE;
        }

      enum { Has_Size = 0 };

    private:
      size_t size_;
      unsigned char* h;
    };

  } // anonymous

  emptiness_check* explicit_magic_search(const tgba *a, option_map o, bool dyn,
					 bool stat)
  {
    return new magic_search_<explicit_magic_search_heap>(a, 0, o, dyn, stat);
  }

  emptiness_check* bit_state_hashing_magic_search(const tgba *a, size_t size,
                                                  option_map o, bool dyn,
						  bool stat)
  {
    return new magic_search_<bsh_magic_search_heap>(a, size, o, dyn, stat);
  }

  emptiness_check*
  magic_search(const tgba *a, option_map o)
  {
    size_t size = o.get("bsh");
    if (size)
      return bit_state_hashing_magic_search(a, size, o);
    return explicit_magic_search(a, o);
  }


  emptiness_check*
  magic_stat_search(const tgba *a, option_map o)
  {
    size_t size = o.get("bsh");
    if (size)
      return bit_state_hashing_magic_search(a, size, o, false, true);
    return explicit_magic_search(a, o, false, true);
  }

  emptiness_check*
  magic_dyn_search(const tgba *a, option_map o)
  {
    size_t size = o.get("bsh");
    if (size)
      return bit_state_hashing_magic_search(a, size, o, true, false);
    return explicit_magic_search(a, o, true, false);
  }
}
