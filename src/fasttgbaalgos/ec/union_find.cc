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
  union_find::union_find (acc_dict& acc) :
    acc_(acc)
  {
  }

  union_find::~union_find ()
  {
  }

  void
  union_find::add (const fasttgba_state* s)
  {
    assert(el.find(s) != el.end());
    id.push_back(id.size());
    rk.push_back(std::make_pair(0, markset(acc_))); // rk.push_back(0);
    el[s] = el.size();
  }

  int union_find::root (int i)
  {
    int parent = id[i];

    // The element is the root
    if (i == parent)
      return parent;

    // Grand'Pa is root!
    int gparent = id[parent];
    if (parent == gparent)
      return parent;

    parent = root (parent);	// Can we use gparent?
    id[i] = parent;
    return parent;
  }

  bool
  union_find::same_partition (const fasttgba_state* left,
			      const fasttgba_state* right)
  {
    assert(el.find(left) != el.end());
    assert(el.find(right) != el.end());
    return root(el[left]) == root(el[right]);
  }

  void
  union_find::unite (const fasttgba_state* left,
		     const fasttgba_state* right)
  {
    assert(el.find(left) != el.end());
    assert(el.find(right) != el.end());

    int root_left = root(el[left]);
    int root_right = root(el[right]);

    // Both are in the same set
    if (root_left == root_right)
      return;

    int rk_left = rk[root_left].first;
    int rk_right = rk[root_right].first;

    // Use weigth
    if (rk_left < rk_right)
      {
	id [root_left] =  root_right;
	rk[root_left].second |= rk[root_right].second;
      }
    else
      {
	id [root_right] =  root_left;
	rk[root_right].second |= rk[root_left].second;

	if (rk_left == rk_right)
	  {
	    rk[root_left].first = root_left + 1;
	  }
      }
  }

  markset
  union_find::get_acc (const fasttgba_state* s)
  {
    int r = root(el[s]);
    return rk[r].second;
  }

  void
  union_find::add_acc (const fasttgba_state* s, markset m)
  {
    int r = root(el[s]);
    rk[r].second |= m;
  }
}
