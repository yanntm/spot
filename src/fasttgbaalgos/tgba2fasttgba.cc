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
#include "fasttgba/fasttgba_explicit.hh"
#include "tgbaalgos/reachiter.hh"
#include "ltlast/formula.hh"
#include "ltlast/atomic_prop.hh"
#include "ltlast/constant.hh"
#include "tgba/bdddict.hh"
#include "fasttgba/markset.hh"
#include "fasttgba/ap_dict.hh"
#include "tgba/tgbaexplicit.hh"

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
      converter_bfs(const tgba* a, ap_dict& aps,  acc_dict& accs)
	: tgba_reachable_iterator_breadth_first(a),
	  result_(0),
	  aps_(aps),
	  accs_(accs),
	  strong_(false),
	  weak_(false),
	  terminal_(false),
	  toadd_(bddfalse)
      {
	sm = new scc_map(aut_);
	sm->build_map();
      }

      ~converter_bfs()
      {
	delete sm;
      }

      void
      start()
      {
	// Here initialize the fasttgba
	result_ = new fasttgbaexplicit(&aps_, &accs_);

	// First grab Atomic propositions
	bdd_dict* aps = aut_->get_dict();
	std::map<const ltl::formula*, int> var_map = aps->var_map;
	std::map<const ltl::formula*, int>::iterator sii =
	  var_map.begin();
	std::map<const ltl::formula*, int>::iterator end =
	  var_map.end();

	for (; sii != end; ++sii)
	  {
	    positions_.push_back(sii->second);
	    const ltl::formula *f = sii->first;
	    //f->clone();
	    int p = aps_.register_ap_for_aut
	      (down_cast<const ltl::atomic_prop*>(f), result_);
	    positions2_.push_back(p);
	  }

	// Second grab Acceptance variables
	std::map<const ltl::formula*, int> acc_map = aps->acc_map;
	std::map<const ltl::formula*, int>::iterator sii2 =
	  acc_map.begin();
	std::map<const ltl::formula*, int>::iterator end2 =
	  acc_map.end();

	for (; sii2 != end2; ++sii2)
	  {
	    const ltl::formula *f = sii2->first;

	    // Is it a trick ?
	    if (dynamic_cast<const sba_explicit_number*>(aut_) &&
		aut_->number_of_acceptance_conditions())
	      {
		if (!(ltl::constant::true_instance() == f))
		  continue;
	      }

	    // It's not a variable for me!
	    // When a degeneralisation is performed some acceptance
	    // sets  are removed off the tgba
	    if (!aps->is_registered_acceptance_variable(f, aut_))
	      continue;
	      //toadd_ |= bdd_ithvar(sii2->second);

	    acceptances_.push_back(sii2->second);

	    int p = accs_.register_acc_for_aut(f->dump(), result_);
	    acceptances2_.push_back(p);
	  }
      }

      void
      end()
      {
      }

      void
      process_state(const state* o, int s , tgba_succ_iterator*)
      {
	fast_explicit_state* st = result_->add_state(s);

	int n = sm->scc_of_state(o);
	st->formula_scc_number = n;
	st->formula_scc_accepting = sm->accepting(n);

	if (!sm->accepting(n))
	  {
	    //std::cout << "Non Accepting" << std::endl;
	    st->set_strength (NON_ACCEPTING_SCC);
	  }
	else if (is_weak_scc (*sm, n))
	  {
	    if (is_complete_scc (*sm, n))
	      {
		//std::cout << "Terminal\n";
		st->set_strength (TERMINAL_SCC);
		terminal_ = true;
	      }
	    else
	      {
		//std::cout << "Weak\n";
		st->set_strength (WEAK_SCC);
		weak_ = true;
	      }
	  }
	else
	  {
	    //std::cout << "Strong\n";
	    st->set_strength (STRONG_SCC);
	    strong_ = true;
	  }
      }

      void
      process_link(const state*, int src,
		   const state*, int dst,
		   const tgba_succ_iterator* it)
      {
	//
	// First we process all acceptance conditions
	//
 	bdd acc  =  it->current_acceptance_conditions();
	if (acc != bddfalse)
	  acc |= toadd_;
	markset current_mark (accs_);
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
		    std::vector<int>::iterator pp =
		      std::find(acceptances_.begin(),
				acceptances_.end(),
				bdd_var(one));
		    int nth = std::distance(acceptances_.begin(), pp);

		    if (nth < 0 ||   nth > (int) acceptances2_.size())
		      {
			break;
		      }

		    current_mark.set_mark(acceptances2_[nth]);
 		    one = bdd_high(one);
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
	    	    std::vector<int>::iterator pp =
		      std::find(positions_.begin(),
				positions_.end(),
				bdd_var(one));
	    	    int nth = std::distance(positions_.begin(), pp);
	    	    current_cond.set_false_var(positions2_[nth]);
	    	    one = bdd_low(one);
	    	  }
	    	else
	    	  {
	    	    std::vector<int>::iterator pp =
		      std::find(positions_.begin(),
				positions_.end(),
				bdd_var(one));
	    	    int nth = std::distance(positions_.begin(), pp);
	    	    current_cond.set_true_var(positions2_[nth]);
	    	    one = bdd_high(one);
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
	if (strong_)
	  result_->set_strength(STRONG_AUT);
	else if (weak_)
	  result_->set_strength(WEAK_AUT);
	else
	  result_->set_strength(TERMINAL_AUT);
	return result_;
      }

    private:
      fasttgbaexplicit *result_;
      ap_dict& aps_;
      acc_dict& accs_;
      scc_map *sm;	        	///< The map used to decompose
      std::vector<int> positions_;
      std::vector<int> positions2_;
      std::vector<int> acceptances_;
      std::vector<int> acceptances2_;
      bool strong_;
      bool weak_;
      bool terminal_;
      bdd toadd_;
    };
  }

  const fasttgba*
  tgba_2_fasttgba(const spot::tgba* a, ap_dict& aps, acc_dict& accs)
  {
    converter_bfs cb_(a, aps, accs);
    cb_.run();
    return cb_.result();
  }

}
