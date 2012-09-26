// Copyright (C) 2012 Laboratoire de Recherche et Developpement de
// l'Epita (LRDE).
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

#ifndef SPOT_TGBAALGOS_ISWEAKSCC_CC
# define SPOT_TGBAALGOS_ISWEAKSCC_CC

#include "cycles.hh"
#include "tgba/tgbaexplicit.hh"
#include "ltlast/formula.hh"

namespace spot
{
  namespace
  {
    // Look for a non-accepting cycle.
    class weak_checker: public enumerate_cycles
    {
    public:
      bool result;

      weak_checker(const scc_map& map)
	: enumerate_cycles(map), result(true)
      {
      }

      virtual bool
      cycle_found(const state* start)
      {
	dfs_stack::const_reverse_iterator i = dfs_.rbegin();
	bdd acc = bddfalse;
	for (;;)
	  {
	    acc |= i->succ->current_acceptance_conditions();
	    if (i->ts->first == start)
	      break;
	    ++i;
	    // The const cast is here to please old g++ versions.
	    // At least version 4.0 needs it.
	    assert(i != const_cast<const dfs_stack&>(dfs_).rend());
	  }
	if (acc != aut_->all_acceptance_conditions())
	  {
	    // We have found an non-accepting cycle, so the SCC is not
	    // weak.
	    result = false;
	    return false;
	  }
	return true;
      }

    };
  }

  bool
  is_weak_scc(scc_map& map, unsigned scc, bool easydetect)
  {
    // If no cycle is accepting, the SCC is weak.
    if (!map.accepting(scc))
      return true;
    // If all transitions use all acceptance conditions, the SCC is weak.
    if (easydetect && map.useful_acc_of(scc) ==
    	bdd_support(map.get_aut()->neg_acceptance_conditions()))
      return true;
    // If the SCC is accepting, but one cycle is not, the SCC is not
    // weak.
    weak_checker w(map);
    w.run(scc);
    return w.result;
  }

  bool
  is_syntactic_weak_scc(const spot::tgba *a, scc_map& map, unsigned scc)
  {
    const std::list<const spot::state*> states = map.states_of(scc);
    std::list<const spot::state*>::const_iterator it;
    for (it =  states.begin(); it != states.end(); it++)
      {
	const state_explicit_formula *s =
	  dynamic_cast<const state_explicit_formula *>(*it);
	const ltl::formula *f = dynamic_cast<const ltl::formula*>
	  (((const tgba_explicit_formula*) a)->get_label(s));

	if ((f->is_syntactic_obligation() ||
	    f->is_syntactic_persistence() ||
	    f->is_syntactic_safety()))
	  return true;

      }
    return false;
  }


  bool
  is_syntactic_terminal_scc(const spot::tgba *a, scc_map& map, unsigned scc)
  {
    const std::list<const spot::state*> states = map.states_of(scc);
    std::list<const spot::state*>::const_iterator it;
    for (it =  states.begin(); it != states.end(); it++)
      {
	const state_explicit_formula *s =
	  dynamic_cast<const state_explicit_formula *>(*it);
	const ltl::formula *f = dynamic_cast<const ltl::formula*>
	  (((const tgba_explicit_formula*) a)->get_label(s));

	if (f->is_syntactic_guarantee())
	  return true;

      }
    return false;
  }



  bool
  is_weak_heuristic(scc_map& map, unsigned scc)
  {
    if (map.useful_acc_of(scc) ==
	bdd_support(map.get_aut()->neg_acceptance_conditions()))
      return true;
    return false;


    // bool weak  = true; // Presuppose every SCC is weak

    // // Grab all acceptance conditions of the TGBA 
    // bdd all = a->all_acceptance_conditions();
    // std::list<const spot::state*> states = map.states_of(scc);

    // // Walk all states included in the SCC
    // std::list<const spot::state*>::iterator iter =
    //   states.begin();
    // for (; iter != states.end(); ++iter)
    //   {
    // 	// For all of these state we look all succ that 
    // 	// belong to this SCC and check if the transition 
    // 	// is acc 
    // 	tgba_succ_iterator *sit = a->succ_iter (*iter);
    // 	assert(sit);
    // 	for (sit->first(); !sit->done(); sit->next())
    // 	  {

    // 	    // Get the current state
    // 	    const spot::state *stt =  sit->current_state();

    // 	    // Not same SCC
    // 	    if (map.scc_of_state(*iter) != map.scc_of_state(stt))
    // 	      {
    // 		stt->destroy();
    // 		continue;
    // 	      }

    // 	    if (sit->current_acceptance_conditions() != all &&
    // 		map.accepting(map.scc_of_state(stt)))
    // 	      {
    // 		stt->destroy();
    // 		weak = false;
    // 		break;
    // 	      }
    // 	    stt->destroy();
    // 	  }
    // 	delete sit;
    // 	if (!weak)
    // 	  break;
    //   }
    // return weak;
  }



  bool
  is_complete_scc(const spot::tgba *a, scc_map& map, unsigned scc)
  {
    const std::list<const spot::state*> states = map.states_of(scc);
    std::list<const spot::state*>::const_iterator it;
    for (it =  states.begin(); it != states.end(); it++)
      {
	const state *s = (*it);
	tgba_succ_iterator* it = a->succ_iter(s);
	it->first();

	// No successors cannot be complete
	if (it->done())
	  {
	    delete it;
	    return false;
	  }

	bdd sumall = bddfalse;
	while (! it->done())
	  {
	    const state *next = it->current_state();
	    unsigned sccnum = map.scc_of_state(next);

	    // check it's the same scc
	    if (sccnum == scc)
	      sumall |= it->current_condition();
	    next->destroy();
	    it->next();
	  }
	delete it;

	if (sumall != bddtrue)
	  {
	    return false;
	  } 
      }
    return true;
  }

  bool
  is_terminal_heuristic (const tgba *a, scc_map& map, unsigned scc)
  {
    // If all transitions use all acceptance conditions, the SCC is weak.
    if (map.useful_acc_of(scc) ==
    	bdd_support(map.get_aut()->neg_acceptance_conditions()))
      return is_complete_scc(a, map,scc);
    return false;


    // const std::list<const state*>& st = map.states_of(scc);

    // // Here it s the assumption that all terminal SCC 
    // // have been reduced to a single state that accept 
    // // all words 
    // if (st.size() != 1)
    //   {
    // 	return false;
    //   }

    // const state* s = *st.begin();
    // tgba_succ_iterator* it = a->succ_iter(s);
    // it->first();
    // if (it->done())
    //   {
    // 	// No successors
    // 	delete it;
    // 	return false;
    //   }
    // assert(!it->done());
    // state* dest = it->current_state();
    // bdd cond = it->current_condition();
    // it->next();
    // bool result = (!dest->compare(s)) && it->done() && (cond == bddtrue);
    // dest->destroy();
    // delete it;

    // return result;
  }
}

#endif // SPOT_TGBAALGOS_ISWEAKSCC_CC
