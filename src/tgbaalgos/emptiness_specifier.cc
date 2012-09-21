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
#include "tgba/tgbaproduct.hh"

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
    : sys_(a) , f_(a), state_cache_(0), right_cache_(0),
      strength_cache_(StrongSubaut), termacc_cache_(false),
      id_cache_(0), weak_accepting_cache_(false)
  {
    sm = new scc_map(f_);
    sm->build_map();
  }

  formula_emptiness_specifier::formula_emptiness_specifier (const tgba * a,
							    const tgba * f)
    : sys_(a) , f_(f),state_cache_(0), right_cache_(0),
      strength_cache_(StrongSubaut), termacc_cache_(false),
      id_cache_(0), weak_accepting_cache_(false)
  {
    sm = new scc_map(f_);
    sm->build_map();
  }

  bool
  formula_emptiness_specifier::is_part_of_weak_acc (const state *s)
  {
    if (state_cache_ == s)
      return weak_accepting_cache_;
    update_cache(s);
    return weak_accepting_cache_;
  }

  bool
  formula_emptiness_specifier::same_weak_acc (const state *s1,
					      const state *s2)
  {
    unsigned id_scc1;
    if (state_cache_ == s1)
      id_scc1 = id_cache_;
    else{
      state * sproj1 = (static_cast<const spot::state_product*> (s1))->right(); 
      id_scc1 = sm->scc_of_state(sproj1);
    }

    update_cache(s2);
    return (sm->weak_accepting(id_scc1)) && (id_scc1 == id_cache_);
  }

  // bool
  // formula_emptiness_specifier::is_guarantee (const state * s) const
  // {
  //   assert(s);
  //   bool res = false;

  //   state * sproj = sys_->project_state(s, f_);
  //   assert(sproj);
  //   unsigned id_scc = sm->scc_of_state(sproj);
  //   res = sm->terminal_subautomaton(id_scc);
  //   sproj->destroy();

  //   return res;
  // }

  // bool
  // formula_emptiness_specifier::is_persistence (const state *s) const
  // {
  //   bool res = false;

  //   state * sproj = sys_->project_state(s, f_);
  //   assert(sproj);
  //   unsigned id_scc = sm->scc_of_state(sproj);
  //   res = sm->weak_subautomaton(id_scc);
  //   sproj->destroy();

  //   return res;
  // }

  // bool
  // formula_emptiness_specifier::is_general (const state *s) const
  // {
  //   assert(s);
  //   strength str = typeof_subautomaton(s);
  //   return !is_guarantee(s) &&  !is_persistence(s);
  // }

  strength 
  formula_emptiness_specifier::typeof_subautomaton
  (const state *s)
  {
    // Is in the cache ?
    if (state_cache_ == s)
      return strength_cache_;

    update_cache(s);
    return strength_cache_;
  }

  bool
  formula_emptiness_specifier::is_terminal_accepting_scc
  (const state *s)
  {
    if (state_cache_ == s)
      return termacc_cache_;

    update_cache(s);
    return termacc_cache_;
  }


  inline void formula_emptiness_specifier::update_cache(const state *s)
  {
    if (state_cache_ == s) 
      return;
  //   state * sproj = sys_->project_state(s, f_);
  //   assert(sproj);
  //   unsigned id_scc = sm->scc_of_state(sproj);

    right_cache_ = (static_cast<const spot::state_product*> (s))->right();
    id_cache_ = sm->scc_of_state(right_cache_);
    strength_cache_ = sm->typeof_subautomaton(id_cache_);
    termacc_cache_ = sm->terminal_accepting(id_cache_);
    weak_accepting_cache_ = sm->weak_accepting(id_cache_);
    state_cache_ = s;
  }
}
