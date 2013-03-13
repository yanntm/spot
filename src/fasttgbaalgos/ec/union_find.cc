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
    acc_(acc), cpt(0)
  {
  }

  union_find::~union_find ()
  {
    uf_map::const_iterator s = el.begin();
    while (s != el.end())
      {
	s->first->destroy();
	++s;
      }
    el.clear();
  }

  void
  union_find::add (const fasttgba_state* s)
  {
    // A dead state is represented by a state s == 0
    // the union find structure consider that dead states
    // représentation is -1
    if (!s)
      return;

    //assert(el.find(s) != el.end());
    id.push_back(cpt);
    rk.push_back(std::make_pair(0, markset(acc_))); // rk.push_back(0);
    el[s] = cpt;
    ++cpt;
  }

  int union_find::root (int i)
  {
    if (i == -1)
      return -1;

    int parent = id[i];

    // The element is the root
    if (i == parent || parent == -1)
      return parent;

    // Grand'Pa is root!
    int gparent = id[parent];
    if (parent == gparent ||  gparent == -1)
      return gparent;

    parent = root (parent);	// Can we use gparent?
    id[i] = parent;
    return parent;
  }

  bool
  union_find::same_partition (const fasttgba_state* left,
			      const fasttgba_state* right)
  {
    if (left == right)
      return true;
    if (!left)
      return root(el[right]) == -1;
    if (!right)
      return root(el[left]) == -1;

    return root(el[left]) == root(el[right]);
  }

  void
  union_find::unite (const fasttgba_state* left,
		     const fasttgba_state* right)
  {
    // Both are in the same set
    if (left == right)
      return;
    // left is dead
    if (!left)
      {
	int val = root(el[right]);
	if (val != -1)
	  id[val] = -1;
	return;
      }
    // right is dead
    if (!right)
      {
	int val = root(el[left]);
	if (val != -1)
	  id[val] = -1;
	return;
      }

    // assert(el.find(left) != el.end());
    // assert(el.find(right) != el.end());

    int root_left = root(el[left]);
    int root_right = root(el[right]);

    if (root_left == -1)
      {
	id[root_right] = -1;
	return;
      }
    if (root_right == -1)
      {
	id[root_left] = -1;
	return;
      }

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
    if (!s)
      return markset(acc_);

    int r = root(el[s]);
    if (r == -1)
      return markset(acc_);

    return rk[r].second;
  }

  void
  union_find::add_acc (const fasttgba_state* s, markset m)
  {
    if (!s)
      return;

    int r = root(el[s]);
    if (r == -1)
      return;

    rk[r].second |= m;
  }

  bool union_find::contains (const fasttgba_state* s)
  {
    if (!s)
      return true;
    return el.find(s) != el.end();
  }

}
