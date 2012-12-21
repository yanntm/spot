// Copyright (C) 2012 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
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

#include <string>
#include <iostream>
#include "tgba2fasttgba.hh"
#include "fasttgba/fasttgbaexplicit.hh"
#include "tgbaalgos/reachiter.hh"
#include "ltlast/formula.hh"
#include "ltlast/atomic_prop.hh"
#include "tgba/bdddict.hh"
#include "fasttgba/markset.hh"
#include "fasttgba/ap_dict.hh"

namespace spot
{
  namespace
  {
    /// This internal class acts like a converter from a TGBA to a
    /// FASTTGBA. It walk all transitions and states of the original
    /// tgba and create the associated fasttgba.
    class converter_bfs : public tgba_reachable_iterator_breadth_first
    {
    public:
      converter_bfs(const tgba* a, ap_dict* aps)
	: tgba_reachable_iterator_breadth_first(a),
	  aps_(aps),
	  acceptance_number (0),
	  variable_number (0)
      {
      }

      void
      start()
      {
	// Here this method is used to grab main values needed
	// to construct a fasttgba

	// First grab Atomic propositions
	bdd_dict* aps = aut_->get_dict();
	std::map<const ltl::formula*, int> var_map = aps->var_map;
	std::map<const ltl::formula*, int>::iterator sii =
	  var_map.begin();
	std::map<const ltl::formula*, int>::iterator end =
	  var_map.end();

	for (; sii != end; ++sii)
	  {
	    const ltl::formula *f = sii->first;
	    //ap_dict.push_back(((const ltl::atomic_prop*)f)->name());
	    aps_->add_ap((const ltl::atomic_prop*)f);
	  }

	// Second grab Acceptance variables
	std::map<const ltl::formula*, int> acc_map = aps->acc_map;
	std::map<const ltl::formula*, int>::iterator sii2 =
	  acc_map.begin();
	std::map<const ltl::formula*, int>::iterator end2 =
	  acc_map.end();

	std::vector<std::string> acc_dict;
	for (; sii2 != end2; ++sii2)
	  {
	    const ltl::formula *f = sii2->first;
	    acc_dict.push_back(((const ltl::atomic_prop*)f)->name());
	  }

	// To speed up other processing
	acceptance_number = acc_dict.size();
	variable_number = aps_->size();

	// Here initialize the fasttgba
	result_ = new fasttgbaexplicit(aps_, acc_dict);
      }

      void
      end()
      {
      }

      void
      process_state(const state* , int s , tgba_succ_iterator*)
      {
	result_->add_state(s);
      }

      void
      process_link(const state* , int src,
		   const state* , int dst,
		   const tgba_succ_iterator* it)
      {
	//
	// First we process all acceptance conditions
	//
 	bdd acc  = it->current_acceptance_conditions();
	markset current_mark (acceptance_number);
	while (acc != bddfalse)
	  {
	    bdd one = bdd_satone(acc);
	    acc -= one;

	    int i = 0;
	    while (one != bddtrue)
	      {
		if (bdd_high(one) == bddfalse)
		  {
		    one = bdd_low(one);
		  }
		else
		  {
		    one = bdd_high(one);
		    current_mark.set_mark(i);
		    break;
		  }
		++i;
	      }
	  }

	//
	// Second we process the conditions
	//
 	bdd cond  = it->current_condition();
	while (cond != bddfalse)
	  {
	    cube current_cond (result_->get_dict());
	    bdd one = bdd_satone(cond);
	    cond -= one;

	    int i = 0;
	    while (one != bddtrue)
	      {
		if (bdd_high(one) == bddfalse)
		  {
		    one = bdd_low(one);
		    current_cond.set_false_var(i);
		  }
		else
		  {
		    one = bdd_high(one);
		    current_cond.set_true_var(i);
		  }
		++i;
	      }

	    // Now we can create the transition
	    result_->add_transition(src, dst, current_cond, current_mark);
	  }
      }

      const fasttgba*
      result ()
      {
	return result_;
      }

    private:
      fasttgbaexplicit *result_;
      ap_dict* aps_;
      int acceptance_number;
      int variable_number;
    };
  }

  const fasttgba*
  tgba_2_fasttgba(const spot::tgba* a, ap_dict* aps)
  {
    converter_bfs cb_(a, aps);
    cb_.run();
    return cb_.result();
  }

}
