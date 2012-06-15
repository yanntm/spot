// -*- coding: utf-8 -*-
// Copyright (C) 2012 Laboratoire de Recherche et
// Développement de l'Epita (LRDE).
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

#include <queue>
#include <map>
#include <utility>
#include "tgba/tgbaexplicit.hh"
#include "simulation.hh"
#include "misc/acccompl.hh"
#include "misc/minato.hh"
#include "tgba/bddprint.hh"
#include "tgbaalgos/reachiter.hh"
#include "tgbaalgos/scc.hh"
#include "tgbaalgos/dupexp.hh"

// The way we developed this algorithm is the following: We take an
// automaton, and reverse all these acceptance conditions.  We reverse
// them to go make the meaning of the signature easier. We are using
// bdd, and we want to let it make all the simplification. Because of
// the format of the acceptance condition, it doesn't allow easy
// simplification. Instead of encoding them as: "a!b!c + !ab!c", we
// use them as: "ab". We complement them because we want a
// simplification if the condition of the transition A implies the
// transition of B, and if the acceptance condition of A is included
// in the acceptance condition of B. To let the bdd makes the job, we
// revert them.

// Then, to check if a transition i-dominates another, we'll use the bdd:
// "sig(transA) = cond(trans) & acc(trans) & implied(class(trans->state))"
// Idem for sig(transB). The 'implied'
// (represented by a hash table 'relation_' in the implementation) is
// a conjunction of all the class dominated by the class of the
// destination. This is how the relation is included in the
// signature. It makes the simplifications alone, and the work is
// done.  The algorithm is cut into several step:
//
// 1. Run through the tgba and switch the acceptance condition to their
//    negation, and initializing relation_ by the 'init_ -> init_' where
//    init_ is the bdd which represents the class. This function is the
//    constructor of Simulation.
// 2. Enter in the loop (run).
//    - Rename the class.
//    - run through the automaton and computing the signature of each
//      state. This function is `update_sig'.
//    - Enter in a double loop to adapt the partial order, and set
//      'relation_' accordingly. This function is `update_po'.
// 3. Rename the class (to actualize the name in the previous_class and
//    in relation_).
// 4. Building an automaton with the result, with the condition:
// "a transition in the original automaton appears in the simulated one
// iff this transition is included in the set of i-maximal neighbour."
// This function is `build_output'.
// The automaton simulated is recomplemented to come back to its initial
// state when the object Simulation is destroyed.
//
// Obviously these functions are possibly cut into several little one.
// This is just the general development idea.

// How to use isop:
// I need all variable non_acceptance & non_class.
// bdd_support(sig(X)): All var
// bdd_support(sig(X)) - allacc - allclassvar


namespace spot
{
  namespace
  {
    // Some useful typedef:

    // Used to get the signature of the state.
    typedef Sgi::hash_map<const state*, bdd,
                          state_ptr_hash,
                          state_ptr_equal> map_state_bdd;

    typedef Sgi::hash_map<const state*, unsigned,
                          state_ptr_hash,
                          state_ptr_equal> map_state_unsigned;


    // Get the list of state for each class.
    typedef std::map<bdd, std::list<const state*>,
                     bdd_less_than> map_bdd_lstate;
    // One state for each class
    typedef std::map<bdd, const state*, bdd_less_than> map_bdd_state;


    // This class takes an automaton and creates a copy with all
    // acceptance conditions complemented.
    class acc_compl_automaton:
      public tgba_reachable_iterator_depth_first
    {
    public:
      acc_compl_automaton(const tgba* a, bdd init)
      : tgba_reachable_iterator_depth_first(a),
	size(0),
	ea(down_cast<tgba_explicit_number*>(const_cast<tgba*>(a))),
	init_(init),
	ac_(ea->all_acceptance_conditions(),
	    ea->neg_acceptance_conditions())
      {
      }

      void process_link(const state*, int,
                        const state*, int,
                        const tgba_succ_iterator* si)
      {
        bdd acc = ac_.complement(si->current_acceptance_conditions());

        const tgba_explicit_succ_iterator<state_explicit_number>* tmpit =
          down_cast<const tgba_explicit_succ_iterator
                                    <state_explicit_number>*>(si);

        typename tgba_explicit_number::transition* t =
          ea->get_transition(tmpit);

        t->acceptance_conditions = acc;
      }

      void process_state(const state* s, int, tgba_succ_iterator*)
      {
        ++size;
        previous_class_[s] = init_;
      }

      ~acc_compl_automaton()
      {
      }

    public:
      size_t size;
      map_state_bdd previous_class_;
      tgba_explicit_number* ea;
      bdd init_;

    private:
      acc_compl ac_;
    };


    class direct_simulation
    {
      // Shortcut used in update_po and go_to_next_it.
      typedef std::map<bdd, bdd, bdd_less_than> map_bdd_bdd;
    public:
      direct_simulation(const tgba* t)
      : a_(0),
	po_size_(0),
	all_class_var_(bddtrue),
	scc_map_(0)
      {
	a_ = tgba_dupexp_dfs(t);
	scc_map_ = new scc_map(a_);
	scc_map_->build_map();

	bdd_dict* dict = a_->get_dict();

	// This special BDD variable is used to mark transition that
	// are on a cycle.
	on_cycle_ =
	  bdd_ithvar(dict->register_anonymous_variables(1, a_));

	// Now, we have to get the bdd which will represent the
	// class. We register one bdd by state, because in the worst
	// case, |Class| == |State|.
	unsigned set_num = dict->register_anonymous_variables(1, a_);
	bdd init = bdd_ithvar(set_num);

	acc_compl_automaton acc_compl(a_, init);

	// We'll start our work by replacing all the acceptance
	// conditions by their complement.
	acc_compl.run();

	a_ = acc_compl.ea;

	all_proms_ = bdd_support(a_->all_acceptance_conditions());

	// We use the previous run to know the size of the
	// automaton, and to class all the reachable states in the
	// map previous_class_.
	size_a_ = acc_compl.size;

	set_num = dict->register_anonymous_variables(size_a_ - 1, a_);

	used_var_.push_back(init);
	all_class_var_ = init;

	// We fetch the result the run of acc_compl_automaton which
	// has recorded all the state in a hash table, and we set all
	// to init.
	for (map_state_bdd::iterator it
	       = acc_compl.previous_class_.begin();
	     it != acc_compl.previous_class_.end();
	     ++it)
          {
            previous_class_[it->first] = init;
          }

	// Put all the anonymous variable in a queue, and record all
	// of these in a variable all_class_var_ which will be used
	// to understand the destination part in the signature when
	// building the resulting automaton.
	for (unsigned i = set_num; i < set_num + size_a_ - 1; ++i)
          {
            free_var_.push(i);
            all_class_var_ &= bdd_ithvar(i);
          }

	relation_[init] = init;
      }

      // Reverse all the acceptance condition at the destruction of
      // this object, because it occurs after the return of the
      // function simulation.
      ~direct_simulation()
      {
	delete scc_map_;
	delete a_;
      }


      // We update the name of the class.
      void update_previous_class()
      {
	std::list<bdd>::iterator it_bdd = used_var_.begin();

	// We run through the map bdd/list<state>, and we update
	// the previous_class_ with the new data.
	it_bdd = used_var_.begin();
	for (map_bdd_lstate::iterator it = bdd_lstate_.begin();
	     it != bdd_lstate_.end();
	     ++it)
          {
            for (std::list<const state*>::iterator it_s = it->second.begin();
                 it_s != it->second.end();
                 ++it_s)
	      {
		// If the signature of a state is bddfalse (which is
		// roughly equivalent to no transition) the class of
		// this state is bddfalse instead of an anonymous
		// variable. It allows simplifications in the signature
		// by removing a transition which has as a destination a
		// state with no outgoing transition.
		if (it->first == bddfalse)
		  previous_class_[*it_s] = bddfalse;
		else
		  previous_class_[*it_s] = *it_bdd;
	      }
            ++it_bdd;
          }
      }

      // The core loop of the algorithm.
      tgba* run()
      {
	unsigned int nb_partition_before = 0;
	unsigned int nb_po_before = po_size_ - 1;
	while (nb_partition_before != bdd_lstate_.size()
	       || nb_po_before != po_size_)
          {
            update_previous_class();
            nb_partition_before = bdd_lstate_.size();
            bdd_lstate_.clear();
            nb_po_before = po_size_;
            po_size_ = 0;
            update_sig();
            go_to_next_it();
          }

	update_previous_class();
	return build_result();
      }

      // Compute the signature of a state.
      //
      // The signature of a state is a Boolean function that
      // sums a signature of its outgoing transitions.
      // The signature of a transition is a product
      // between the label, promises, and destination state.
      bdd compute_sig(const state* src, bool mark = true)
      {
	tgba_succ_iterator* sit = a_->succ_iter(src);
	bdd res = bddfalse;

	unsigned scc = scc_map_->scc_of_state(src);
	bool sccacc = scc_map_->accepting(scc);

	for (sit->first(); !sit->done(); sit->next())
          {
            const state* dst = sit->current_state();
	    bdd cl = previous_class_[dst];
            bdd acc;

	    if (mark)
	      {
		if (scc != scc_map_->scc_of_state(dst))
		  acc = !on_cycle_;
		else if (sccacc)
		  acc = on_cycle_ & sit->current_acceptance_conditions();
		else
		  acc = on_cycle_ & all_proms_;
	      }
	    else
	      {
		if (scc != scc_map_->scc_of_state(dst))
		  acc = !on_cycle_ & all_proms_;
		else if (sccacc)
		  acc = on_cycle_ & sit->current_acceptance_conditions();
		else
		  acc = on_cycle_ & all_proms_;
	      }

            bdd to_add = acc & sit->current_condition() & relation_[cl];
	    //std::cerr << "toadd " << to_add << std::endl;
            res |= to_add;
          }

	delete sit;
	//std::cerr << "sig(" << src << ")=" << res << std::endl;
	return res;
      }

      void update_sig()
      {
	// At this time, current_class_ must be empty.  It implies
	// that the "previous_class_ = current_class_" must be
	// done before.
	assert(current_class_.empty());

	// Here we suppose that previous_class_ always contains
	// all the reachable states of this automaton. We do not
	// have to make (again) a traversal. We just have to run
	// through this map.
	for (map_state_bdd::iterator it = previous_class_.begin();
	     it != previous_class_.end();
	     ++it)
          {
            const state* src = it->first;

            bdd_lstate_[compute_sig(src)].push_back(src);
          }
      }


      // This method rename the color set, update the partial order.
      void go_to_next_it()
      {
	int nb_new_color = bdd_lstate_.size() - used_var_.size();

	for (int i = 0; i < nb_new_color; ++i)
          {
            assert(!free_var_.empty());
            used_var_.push_back(bdd_ithvar(free_var_.front()));
            free_var_.pop();
          }

	assert(bdd_lstate_.size() == used_var_.size());

	// Now we make a temporary hash_table which links the tuple
	// "C^(i-1), N^(i-1)" to the new class coloring.  If we
	// rename the class before updating the partial order, we
	// loose the information, and if we make it after, I can't
	// figure out how to apply this renaming on rel_.
	// It adds a data structure but it solves our problem.
	map_bdd_bdd now_to_next;

	std::list<bdd>::iterator it_bdd = used_var_.begin();

	for (map_bdd_lstate::iterator it = bdd_lstate_.begin();
	     it != bdd_lstate_.end();
	     ++it)
          {
            // If the signature of a state is bddfalse (which is
            // roughly equivalent to no transition) the class of
            // this state is bddfalse instead of an anonymous
            // variable. It allows simplifications in the signature
            // by removing a transition which has as a destination a
            // state with no outgoing transition.
            if (it->first == bddfalse)
              now_to_next[it->first] = bddfalse;
            else
              now_to_next[it->first] = *it_bdd;
            ++it_bdd;
          }

	update_po(now_to_next);
      }

      // This function computes the new po with previous_class_
      // and the argument. `now_to_next' contains the relation
      // between the signature and the future name of the class.
      void update_po(const map_bdd_bdd& now_to_next)
      {
	// This loop follows the pattern given by the paper.
	// foreach class do
	// |  foreach class do
	// |  | update po if needed
	// |  od
	// od

	for (map_bdd_bdd::const_iterator it1 = now_to_next.begin();
	     it1 != now_to_next.end();
	     ++it1)
          {
            bdd accu = it1->second;

	    bdd f1 = bdd_restrict(it1->first, on_cycle_);
	    bdd g1 = bdd_restrict(it1->first, !on_cycle_);

	    //std::cerr << "\nit1 " << it1->first << std::endl;
	    //std::cerr << "f1 " << f1 << std::endl;
	    //std::cerr << "g1 " << g1 << std::endl;

            for (map_bdd_bdd::const_iterator it2 = now_to_next.begin();
                 it2 != now_to_next.end();
                 ++it2)
	      {
		// Skip the case managed by the initialization of accu.
		if (it1 == it2)
		  continue;

		bdd f2g2 = bdd_exist(it2->first, on_cycle_);
		bdd f2g2n = bdd_exist(f2g2, all_proms_);

		//std::cerr << "it2 " << it2->first << std::endl;
		//std::cerr << "f2g2  " << f2g2 << std::endl;
		//std::cerr << "f2g2n " << f2g2n << std::endl;

		// We used to have
		//   sig(s1) = (f1 | g1)
		//   sig(s2) = (f2 | g2)
		// and we say that s2 simulates s1 if sig(s1)=>sig(s2).
		// This amount to testing whether (f1|g1)=>(f2|g2),
		// which is equivalent to testing both
		//    f1=>(f2|g2)  and g1=>(f2|g2)
		// separately.
		//
		// Now we have a slightly improved version of this rule.
		// g1 and g2 are not on cycle, so they can make as many
		// promises as we wish, if that helps.  Adding promises
		// to g2 will not help, but adding promises to g1 can.
		//
		// So we test whether
		//    f1=>(f2|g2)
		//    g1=>noprom(f2|g2)
		// Where noprom(f2|g2) removes all promises from f2|g2.
		// (g1 do not have promises, and neither do g2).
		//
		// We detect that "a => b" by testing "a&b = a".
		if (bdd_implies(f1, f2g2) && bdd_implies(g1, f2g2n))
		  {
		    accu &= it2->second;
		    ++po_size_;
		    //std::cerr << "implication!" << std::endl;
		  }
	      }
            relation_[it1->second] = accu;
          }
	//for (map_bdd_bdd::const_iterator i = relation_.begin();
	//   i != relation_.end(); ++i)
	//std::cerr << "rel[" << i->first << "]" << i->second << std::endl;
      }

      // Build the minimal resulting automaton.
      tgba* build_result()
      {
	// Now we need to create a state per partition. But the
	// problem is that we don't know exactly the class. We know
	// that it is a combination of the acceptance condition
	// contained in all_class_var_. So we need to make a little
	// workaround. We will create a map which will associate bdd
	// and unsigned.
	std::map<bdd, unsigned, bdd_less_than> bdd2state;
	unsigned int current_max = 0;

	bdd all_acceptance_conditions
	  = a_->all_acceptance_conditions();

	// We have all the a_'s acceptances conditions
	// complemented.  So we need to complement it when adding a
	// transition.  We *must* keep the complemented because it
	// is easy to know if an acceptance condition is maximal or
	// not.
	acc_compl reverser(all_acceptance_conditions,
			   a_->neg_acceptance_conditions());

	typedef tgba_explicit_number::transition trs;
	tgba_explicit_number* res
	  = new tgba_explicit_number(a_->get_dict());
	res->set_acceptance_conditions(all_acceptance_conditions);

	// Non atomic propositions variables (= acc and class)
	bdd nonapvars = all_proms_ & bdd_support(all_class_var_) & on_cycle_;

	// Some states in non-accepting SCC have been discovered as
	// equivalent to states in accepting SCC.  This happens for
	// instance when translating XFGa (without LTL
	// simplifications).  In this case, we should make sure we do
	// not try to translate the non-accepting states.
	map_bdd_state ms;
	for (map_bdd_lstate::iterator it = bdd_lstate_.begin();
	     it != bdd_lstate_.end(); ++it)
          {
	    const state* s = *it->second.begin();
            bdd cl = relation_[previous_class_[s]];

	    //std::cerr << "! " << s << " " << cl << std::endl;

	    map_bdd_state::iterator i = ms.find(cl);
	    if (i == ms.end())
	      ms[cl] = s;
	    else if (scc_map_->scc_of_state(s) < scc_map_->scc_of_state(i->second))
	      // Overwrite a state in the same class only with one
	      // from a latter SCC.  This deals with the case of the
	      // translation of XGFa where the XGFa state is a
	      // transient state equivalent to GFa, Similarly with
	      // XFGa, where the state XFGa is equivalent to the sate
	      // FGa (which is still not accepting).
	      i->second = s;
	  }

	// Create one state per partition.
	for (map_bdd_state::iterator it = ms.begin(); it != ms.end(); ++it)
          {
            res->add_state(++current_max);
	    bdd2state[it->first] = current_max;
          }

	// For each partition, we will create
	// all the transitions between the states.
	for (map_bdd_state::iterator it = ms.begin(); it != ms.end(); ++it)
          {
	    int src = bdd2state[it->first];
	    assert(src != 0);

            // Get the signature.
            bdd sig = compute_sig(it->second, false);

	    //std::cerr << it->second << " " <<  sig << std::endl;

            // Get all the variable in the signature.
            bdd sup_sig = bdd_support(sig);

            // Get the variable in the signature which represents the
            // conditions.
            bdd sup_all_atomic_prop = bdd_exist(sup_sig, nonapvars);

            // Get the part of the signature composed only with the atomic
            // proposition.
            bdd all_atomic_prop = bdd_exist(sig, nonapvars);

	    // First loop over all possible valuations atomic properties.
            while (all_atomic_prop != bddfalse)
	      {
		bdd one = bdd_satoneset(all_atomic_prop,
					sup_all_atomic_prop,
					bddtrue);
		all_atomic_prop -= one;


		// For each possible valuation, iterate over all possible
		// destination classes.   We use minato_isop here, because
		// if the same valuation of atomic properties can go
		// to two different classes C1 and C2, iterating on
		// C1 + C2 with the above bdd_satoneset loop will see
		// C1 then (!C1)C2, instead of C1 then C2.
		// With minatop_isop, we ensure that the no negative
		// class variable will be seen (likewise for promises).
		minato_isop isop(sig & one);

		bdd cond_acc_dest;
		while ((cond_acc_dest = isop.next()) != bddfalse)
		  {
		    // Take the transition, and keep only the variable which
		    // are used to represent the class.
		    bdd dest = bdd_existcomp(cond_acc_dest,
					     all_class_var_);

		    // Keep only ones who are acceptance condition.
		    bdd acc = bdd_existcomp(cond_acc_dest, all_proms_);

		    // Keep the other !
		    bdd cond = bdd_existcomp(cond_acc_dest,
					     sup_all_atomic_prop);

		    // Because we have complemented all the acceptance
		    // condition on the input automaton, we must re
		    // invert them to create a new transition.
		    acc = reverser.reverse_complement(acc);

		    // Take the id of the source and destination.  To
		    // know the source, we must take a random state in
		    // the list which is in the class we currently
		    // work on.
		    int dst = bdd2state[dest];
		    //	    std::cerr << "src = " << previous_class_[it->second]
		    //	      << " dest = " << dest << std::endl;

		    // src or dst == 0 means "dest" or "prev..." isn't
		    // in the map.  so it is a bug.
		    assert(dst != 0);

		    // Create the transition, add the condition and the
		    // acceptance condition.
		    tgba_explicit_number::transition* t
		      = res->create_transition(src , dst);
		    res->add_conditions(t, cond);
		    res->add_acceptance_conditions(t, acc);
		  }
	      }
          }

	bdd cl = relation_[previous_class_[a_->get_init_state()]];
	res->set_init_state(bdd2state[cl]);
	res->merge_transitions();
	return res;
      }


      // Debug:
      // In a first time, print the signature, and the print a list
      // of each state in this partition.
      // In a second time, print foreach state, who is where,
      // where is the new class name.
      void print_partition()
      {
	for (map_bdd_lstate::iterator it = bdd_lstate_.begin();
	     it != bdd_lstate_.end();
	     ++it)
          {
            std::cerr << "partition: "
                      << bdd_format_set(a_->get_dict(), it->first) << std::endl;

            for (std::list<const state*>::iterator it_s = it->second.begin();
                 it_s != it->second.end();
                 ++it_s)
	      {
		std::cerr << "  - "
			  << a_->format_state(*it_s) << std::endl;
	      }
          }

	std::cerr << "\nPrevious iteration\n" << std::endl;

	for (map_state_bdd::const_iterator it = previous_class_.begin();
	     it != previous_class_.end();
	     ++it)
          {
            std::cerr << a_->format_state(it->first)
                      << " was in "
                      << bdd_format_set(a_->get_dict(), it->second)
                      << std::endl;
          }
      }

    private:
      // The automaton which is simulated.
      tgba_explicit_number* a_;

      // Relation is aimed to represent the same thing than
      // rel_. The difference is in the way it does.
      // If A => A /\ A => B, rel will be (!A U B), but relation_
      // will have A /\ B at the key A. This trick is due to a problem
      // with the computation of the resulting automaton with the signature.
      // rel_ will pollute the meaning of the signature.
      map_bdd_bdd relation_;

      // Represent the class of each state at the previous iteration.
      map_state_bdd previous_class_;

      // The class at the current iteration.
      map_state_bdd current_class_;

      // The list of states for each class at the current_iteration.
      // Computed in `update_sig'.
      map_bdd_lstate bdd_lstate_;

      // The queue of free bdd. They will be used as the identifier
      // for the class.
      std::queue<int> free_var_;

      // The list of used bdd. They are in used as identifier for class.
      std::list<bdd> used_var_;

      // Size of the automaton.
      unsigned int size_a_;

      // Used to know when there is no evolution in the po. Updated
      // in the `update_po' method.
      unsigned int po_size_;

      // All the class variable:
      bdd all_class_var_;

      // The SCC for the source automaton.
      scc_map* scc_map_;

      // Special BDD variable to mark transitions that are on a cycle.
      bdd on_cycle_;

      // The conjunction of all promises.
      bdd all_proms_;
    };
  } // End namespace anonymous.

  tgba*
  simulation(const tgba* t)
  {
    direct_simulation simul(t);

    return simul.run();
  }

} // End namespace spot.
