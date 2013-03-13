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


//#define UFTRACE
#ifdef UFTRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif

namespace spot
{
  union_find::union_find (acc_dict& acc) :
    acc_(acc), empty(acc_)
  {
  }

  union_find::~union_find ()
  {
    uf_map::const_iterator s = el.begin();
    while (s != el.end())
      {
	if (s->first)
	  s->first->destroy();
	++s;
      }
    el.clear();
  }

  void
  union_find::add (const fasttgba_state* s)
  {
    trace << "union_find::add" << std::endl;
    el[s] = id.size();
    id.push_back(id.size());
    rk.push_back(0);
    acc.push_back(empty);
  }

  int union_find::root (int i)
  {
    trace << "union_find::root "  << i << std::endl;
    int r;
    for(r = i; id[r] != r; r = id[r])
      ;
    return r;

    // int parent = id[i];
    // // The element is the root
    // if (i == parent)
    //   return parent;

    // // Grand'Pa is root!
    // int gparent = id[parent];
    // if (parent == gparent)
    //   return gparent;

    // parent = root (parent);	// Can we use gparent?
    // id[i] = parent;
    // return parent;
  }

  bool
  union_find::same_partition (const fasttgba_state* left,
			      const fasttgba_state* right)
  {
    trace << "union_find::same_partition" << std::endl;
    return root(el[left]) == root(el[right]);
  }

  void
  union_find::unite (const fasttgba_state* left,
		     const fasttgba_state* right)
  {
    trace << "union_find::unite" << std::endl;
    int root_left = root(el[left]);
    int root_right = root(el[right]);

    if (!root_left)
      {
    	id[root_right] = 0;
    	return;
      }
    else if (!root_right)
      {
    	id[root_left] = 0;
    	return;
      }

    int rk_left = rk[root_left];
    int rk_right = rk[root_right];

    // Use weigth
    if (rk_left < rk_right)
      {
	id [root_left] =  root_right;
	acc[root_left] |= acc[root_right];
      }
    else
      {
	id [root_right] =  root_left;
	acc[root_right] |= acc[root_left];

	if (rk_left == rk_right)
	  {
	    ++rk[root_left];
	  }
      }
  }

  markset
  union_find::get_acc (const fasttgba_state* s)
  {
    trace << "union_find::get_acc" << std::endl;
    int r = root(el[s]);
    return acc[r];
  }

  void
  union_find::add_acc (const fasttgba_state* s, markset m)
  {
    trace << "union_find::add_acc" << std::endl;
    int r = root(el[s]);
    if (r)
      acc[r] |= m;
  }

  bool union_find::contains (const fasttgba_state* s)
  {
    trace << "union_find::contains" << std::endl;
    return el.find(s) != el.end();
  }
}
