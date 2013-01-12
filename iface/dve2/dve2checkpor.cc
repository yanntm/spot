// Copyright (C) 2012 Laboratoire de Recherche et Developpement de
// l'Epita (LRDE)
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

#include "dve2checkpor.hh"
#include "misc/casts.hh"
#include "tgba/tgbaproduct.hh"

namespace spot
{
  dve2_checkpor::keep_states::keep_states(const tgba* a)
  : tgba_reachable_iterator_depth_first(a)
    , visited ()
  {
  }

  void
  dve2_checkpor::keep_states::process_state(const state* s,
					    int, tgba_succ_iterator*)
  {
    const state_product* sp = down_cast<const state_product*> (s);
    assert (sp);

    const dve2_state* left = down_cast<const dve2_state*> (sp->left());
    assert(left);

    visited.insert(left->clone ());
  }

  dve2_checkpor::dve2_checkpor(const tgba* prod, const tgba* full_prod)
  : tgba_reachable_iterator_depth_first(prod)
    , ok (false)
    , ks_ (full_prod)
  {
    ks_.run ();
  }

  void
  dve2_checkpor::process_state(const state* s, int, tgba_succ_iterator*)
  {
    const state_product* sp = down_cast<const state_product*> (s);
    assert (sp);

    const dve2_state* left = down_cast<const dve2_state*> (sp->left());
    assert(left);

    if (ks_.visited.find(left) == ks_.visited.end())
      {
	std::cout << "ERROR: state not in the full state space!" << std::endl;
	ok = false;
      }
  }
}
