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
#include "ltlast/constant.hh"

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
    : sys_(a) , f_(a)
  {
    sm = new scc_map(f_);
    sm->build_map();
  }

  formula_emptiness_specifier::formula_emptiness_specifier (const tgba * a,
							    const tgba * f)
    : sys_(a) , f_(f)
  {
    sm = new scc_map(f_);
    sm->build_map();
  }

  bool
  formula_emptiness_specifier::is_part_of_weak_acc (const state *s) const
  {
    bool res = false;

    state * sproj = sys_->project_state(s, f_);
    assert(sproj);
    unsigned id_scc = sm->scc_of_state(sproj);
    res = sm->weak_accepting(id_scc);
    sproj->destroy();

    return res;
  }

  bool
  formula_emptiness_specifier::same_weak_acc (const state *s1,
					      const state *s2) const
  {
    bool res = false;

    state * sproj1 = sys_->project_state(s1, f_);
    assert(sproj1);
    unsigned id_scc1 = sm->scc_of_state(sproj1);
    state * sproj2 = sys_->project_state(s2, f_);
    assert(sproj2);
    unsigned id_scc2 = sm->scc_of_state(sproj2);
    sproj1->destroy();
    sproj2->destroy();
    res = (sm->weak_accepting(id_scc1)) && (id_scc1 == id_scc2);

    return res;
  }

  bool
  formula_emptiness_specifier::is_guarantee (const state * s) const
  {
    assert(s);
    bool res = false;
    
    state * sproj = sys_->project_state(s, f_);
    assert(sproj);
    unsigned id_scc = sm->scc_of_state(sproj);
    res = sm->terminal_subautomaton(id_scc);    
    sproj->destroy();

    return res;
  }

  bool
  formula_emptiness_specifier::is_persistence (const state *s) const
  {
    bool res = false;

    state * sproj = sys_->project_state(s, f_);
    assert(sproj);
    unsigned id_scc = sm->scc_of_state(sproj);
    res = sm->weak_subautomaton(id_scc);
    sproj->destroy();

    return res;
  }

  bool
  formula_emptiness_specifier::is_general (const state *s) const
  {
    assert(s);
    return !is_guarantee(s) &&  !is_persistence(s);
  }

  bool
  formula_emptiness_specifier::is_terminal_accepting_scc
  (const state *s) const
  {
    bool res = false;

    state * sproj = sys_->project_state(s, f_);
    assert(sproj);
    unsigned id_scc = sm->scc_of_state(sproj);
    res = sm->terminal_accepting(id_scc);
    sproj->destroy();

    return res;
  }
}
