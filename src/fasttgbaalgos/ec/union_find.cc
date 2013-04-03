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
#include <iostream>

// #define UFTRACE
#ifdef UFTRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif

namespace spot
{
  union_find::union_find (acc_dict& a) :
    acc_(a), empty(acc_)
  {
    idneg.push_back(0);
    accp.push_back(&empty);
  }

  union_find::~union_find ()
  {
    for (unsigned i = 0; i < accp.size(); ++i)
      if (accp[i] != &empty)
	delete accp[i];

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
    trace << "union_find::add " << idneg.size() << std::endl;
    el.insert(std::make_pair(s, idneg.size()));
    idneg.push_back(-1);
    accp.push_back(&empty);
  }

  int union_find::root (int i)
  {
    int p = idneg[i];
    if (p <= 0)
      return i;
    if (idneg[p] <= 0)
      return p;
    return  idneg[i] = root(p);
  }

  bool
  union_find::same_partition (const fasttgba_state* left,
			      const fasttgba_state* right)
  {
    assert(left);
    assert(right);
    int l  = root(el[left]);
    int r  = root(el[right]);
    trace << "union_find::same_partition? "
	  << l << "("<< el[left] << ")   "
	  << r << "("<< el[right] << ")"
	  << std::endl;
    return r == l;
  }

  void
  union_find::make_dead(const fasttgba_state* s)
  {
    trace << "union_find::make_dead " << el[s] << " root_ :"
    	  << root(el[s]) << std::endl;
    idneg[root(el[s])] = 0;
  }

  bool
  union_find::is_dead(const fasttgba_state* s)
  {
    return idneg[root(el[s])] == 0;
  }

  void
  union_find::unite (const fasttgba_state* left,
		     const fasttgba_state* right)
  {
    assert(contains(left));
    assert(contains(right));
    int root_left = root(el[left]);
    int root_right = root(el[right]);

    trace << "union_find::unite "
    	  << root_left << " " << root_right << std::endl;

    assert(root_left);
    assert(root_right);

    int rk_left = idneg[root_left];
    int rk_right = idneg[root_right];

    // Use ranking
    if (rk_left > rk_right)
      {
    	idneg [root_left] =  root_right;

	// instanciate only when it's necessary
	if (accp[root_left] == &empty)
	  accp[root_left]  = new markset(acc_);

	accp[root_left]->operator |= (*accp[root_right]);
      }
    else
      {
    	idneg [root_right] =  root_left;

	// instanciate only when it's necessary
	if (accp[root_right] == &empty)
	  accp[root_right]  = new markset(acc_);

	accp[root_right]->operator |= (*accp[root_left]);

    	if (rk_left == rk_right)
    	  {
	    --idneg[root_left];
    	  }
      }
  }

  markset
  union_find::get_acc (const fasttgba_state* s)
  {
    trace << "union_find::get_acc" << std::endl;
    int r = root(el[s]);
    assert(r);
    return *accp[r];
  }

  void
  union_find::add_acc (const fasttgba_state* s, markset m)
  {
    trace << "union_find::add_acc" << std::endl;
    int r = root(el[s]);

    // instanciate only if necessary
    if (accp[r] == &empty)
      accp[r]  = new markset(acc_);

    accp[r]->operator |= (m);
  }

  bool union_find::contains (const fasttgba_state* s)
  {
    trace << "union_find::contains" << std::endl;
    return el.find(s) != el.end();
  }
}
