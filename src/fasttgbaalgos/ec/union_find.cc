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

#include "union_find.hh"

namespace spot
{
  union_find::union_find (acc_dict& acc):
    acc_(acc)
  {
  }

  union_find::~union_find ()
  {
  }


  const fasttgba_state*
  union_find::find (const fasttgba_state* state)
  {
    const fasttgba_state* s = state;
    uf_element* ue = UF[s];
    while (ue->parent() != s)
      {
	s = ue->parent();
	ue = UF[s];
      }
    return s;
  }


  markset
  union_find::findacc (const fasttgba_state* state)
  {
    const fasttgba_state* s = find(state);
    return UF[s]->acceptance();
  }

  void
  union_find::make_union (const fasttgba_state* left,
			  const fasttgba_state* right)
  {
    const fasttgba_state* l = find(left);
    const fasttgba_state* r = find(right);

    uf_element* e = UF[r];
    e->set_parent (l);
    UF[l]->add_acceptance(e->acceptance());
  }

  void
  union_find::make_set (const fasttgba_state* state, markset m)
  {
    if (UF.find(state) == UF.end())
      {
	UF[state] = new uf_element(state, m);
      }
    else
      {
	const fasttgba_state* s = find(state);
	UF[s]->add_acceptance (m);
      }
  }

  void
  union_find::make_dead_set (const fasttgba_state* state)
  {
    // Remember that 0 is Dead
    if (UF.find(state) == UF.end())
      {
	UF[state] = new uf_element(0, markset(acc_));
      }
    else
      {
	UF[state]->set_parent (0);
      }
  }

  void
  union_find::insert_dead ()
  {
    // The Dead element is denoted by the element 0
    // since 0 is not a valid pointer for a state.
    UF[0] = new uf_element (0, markset(acc_));
  }
}
