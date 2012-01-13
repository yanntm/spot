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



#include "tgbaalgos/emptiness_specifier.hh"
#include "tgba/tgbatba.hh"

//#define SPECTRACE
#include <iostream>
#ifdef SPECTRACE
#define sptrace std::cerr
#else
#define sptrace while (0) std::cerr
#endif

namespace spot
{

  formula_emptiness_specifier::formula_emptiness_specifier (const tgba * a)
    : sys_(a) , f_(a), s_(a)
  {
    const tgba_explicit_formula* bf =
      dynamic_cast<const tgba_explicit_formula*> (a);
    assert(bf);
    both_formula = false;
  }

  formula_emptiness_specifier::formula_emptiness_specifier (const tgba * a,
							    const tgba * f)
    : sys_(a) , f_(f), s_(a)
  {
    const tgba_explicit_formula* bf =
      dynamic_cast<const tgba_explicit_formula*> (f);
    assert(bf);
    both_formula = false;
  }

  /// Create the specifier taking the automaton of the system and 
  /// the automaton of the formula 
  formula_emptiness_specifier::formula_emptiness_specifier (const tgba * a,
							    const tgba * f,
							    const tgba *s)
    : sys_(a) , f_(f), s_(s)
  {
    assert(f && a && s);
    const tgba_explicit_formula* bs =
      dynamic_cast<const tgba_explicit_formula*> (s_);
    const tgba_explicit_formula* bf =
      dynamic_cast<const tgba_explicit_formula*> (f_);
    assert(bf || bs);
    both_formula = (bf && bs);

    if (bf == 0 && !both_formula)
      {
	const tgba* tmp = f_;
	f_ = s_;
	s_ = tmp;
      }
  }

  formula_emptiness_specifier::~formula_emptiness_specifier()
  {
    //delete sm;
  }

  const ltl::formula*
  formula_emptiness_specifier::formula_from_state (const state *s) const
  {
    const ltl::formula* f = 0;
    if (!both_formula)
      {
	state * sproj = sys_->project_state(s, f_);
	assert(sproj);

	const state_explicit* fstate =
	  dynamic_cast<const state_explicit*> (sproj);
	f  = dynamic_cast<const ltl::formula*>
	  (dynamic_cast<const tgba_explicit_formula*> (f_)->get_label(fstate));
	sproj->destroy();
      }
    else
      // Both are formula we construct the result by a LOGICAL AND between 
      // the both formula that are given
      {
	// Compute first formula 
	state * sproj1 = sys_->project_state(s, f_);
	assert(sproj1);
	const state_explicit* fstate1 =
	  dynamic_cast<const state_explicit*> (sproj1);
	const ltl::formula* f1  = dynamic_cast<const ltl::formula*>
	  (dynamic_cast<const tgba_explicit_formula*> (f_)->get_label(fstate1));
	sproj1->destroy();

	// Compute second formula 
	state * sproj2 = sys_->project_state(s, s_);
	assert(sproj1);
	const state_explicit* fstate2 =
	  dynamic_cast<const state_explicit*> (sproj2);
	const ltl::formula* f2  = dynamic_cast<const ltl::formula*>
	  (dynamic_cast<const tgba_explicit_formula*> (s_)->get_label(fstate2));
	sproj1->destroy();

	// Construct the resulting formula
	f = dynamic_cast <const ltl::formula*>
	  (ltl::multop::instance(ltl::multop::And,
				 const_cast <ltl::formula*>(f1),
				 const_cast <ltl::formula*>(f2)));
      }
    sptrace << "\t > recurrence  ?" << f->is_syntactic_recurrence()
	    << std::endl
	    << "\t > persistence ?" << f->is_syntactic_persistence()
	    << std::endl
	    << "\t > obligation  ?" << f->is_syntactic_obligation()
	    << std::endl
	    << "\t > safety      ?" << f->is_syntactic_safety()
	    << std::endl
	    << "\t > guarantee   ?" << f->is_syntactic_guarantee()
	    << std::endl;

    return f;
  }

  bool
  formula_emptiness_specifier::is_part_of_weak_acc (const state *s) const
  {
    bool res = false;
    scc_map* sm = new scc_map(f_);
    sm->build_map();

    if (!both_formula)
      {
	state * sproj = sys_->project_state(s, f_);
	assert(sproj);

	const state_explicit* fstate =
	  dynamic_cast<const state_explicit*> (sproj);

	unsigned id_scc = sm->scc_of_state(fstate);
	res = sm->weak(id_scc);

	sproj->destroy();
      }
    else
      assert (false);

    delete sm;
    return res;
  }

  bool
  formula_emptiness_specifier::same_weak_acc (const state *s1,
					      const state *s2) const
  {
    bool res = false;
    scc_map* sm = new scc_map(f_);
    sm->build_map();

    if (!both_formula)
      {
	state * sproj1 = sys_->project_state(s1, f_);
	assert(sproj1);
	const state_explicit* fstate1 =
	  dynamic_cast<const state_explicit*> (sproj1);
	unsigned id_scc1 = sm->scc_of_state(fstate1);

	state * sproj2 = sys_->project_state(s2, f_);
	assert(sproj2);
	const state_explicit* fstate2 =
	  dynamic_cast<const state_explicit*> (sproj2);
	unsigned id_scc2 = sm->scc_of_state(fstate2);

	sproj1->destroy();
	sproj2->destroy();
	res = (sm->weak(id_scc1)) && (id_scc1 == id_scc2);
      }
    delete sm;
    return res;
  }
}
