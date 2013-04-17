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

#include "union_find_shared.hh"
#include <iostream>

//#define UFTRACE
#ifdef UFTRACE
#define trace std::cerr
#else
#define trace while (0) std::cerr
#endif

namespace spot
{
  union_find_shared::union_find_shared (acc_dict& a, int n
					) :
    union_find(a), ufa_(n)
  {  }

  union_find_shared::~union_find_shared ()
  {
    // Let superclass do job.
    //std::cout << "Delete Shared UF\n";
  }

  bool
  union_find_shared::add (const fasttgba_state* s)
  {
    assert(s);
    ufa_.P_write();
    trace << "union_find_shared::add " <<
      idneg.size() << std::endl << std::flush;

    std::pair<uf_map::iterator, bool> i =
      el.insert(std::make_pair(s, idneg.size()));

    //assert(i.second);
    if (!i.second)
      {
	// In this case the element is already inside
	ufa_.V_write();
	return i.second;
      }

    idneg.push_back(-1);
    accp.push_back(&empty);
    ufa_.V_write();
    return i.second;
  }


  // FIX ME if You want efficience!
  // There is a write ! in this function
  int union_find_shared::root (int i)
  {
    int p = idneg[i];
    if (p <= 0)
      return i;
    if (idneg[p] <= 0)
      return p;
    return  // idneg[i] =
      root(p);
  }

  bool
  union_find_shared::same_partition (const fasttgba_state* left,
			      const fasttgba_state* right)
  {
    ufa_.P_read();
    assert(left);
    assert(right);
    int l  = root(el[left]);
    int r  = root(el[right]);
    trace << "union_find_shared::same_partition? "
	  << l << "("<< el[left] << ")   "
	  << r << "("<< el[right] << ")"
	  << std::endl << std::flush;
    ufa_.V_read();
    return r == l;
  }

  void
  union_find_shared::make_dead(const fasttgba_state* s)
  {
    ufa_.P_write();
    trace << "union_find_shared::make_dead " << el[s] << " root_ :"
    	  << root(el[s]) << std::endl << std::flush;
    idneg[root(el[s])] = 0;
    ufa_.V_write();
  }

  bool
  union_find_shared::is_dead(const fasttgba_state* s)
  {
    ufa_.P_read();
    bool res = idneg[root(el[s])] == 0;
    ufa_.V_read();
    return res;
  }

  void
  union_find_shared::unite (const fasttgba_state* left,
		     const fasttgba_state* right)
  {
    assert(contains(left));
    assert(contains(right));
    ufa_.P_read();
    int root_left = root(el[left]);
    int root_right = root(el[right]);

    trace << "union_find_shared::unite "
    	  << root_left << " " << root_right << std::endl << std::flush;

    assert(root_left);
    assert(root_right);

    int rk_left = idneg[root_left];
    int rk_right = idneg[root_right];
    ufa_.V_read();

    // Use ranking
    if (rk_left > rk_right)
      {
	ufa_.P_write();
    	idneg [root_left] =  root_right;

	// instanciate only when it's necessary
	if (accp[root_left] == &empty && accp[root_right] == &empty)
	  {
	    ufa_.V_write();
	    return;
	  }

	if (accp[root_left] == &empty)
	  accp[root_left]  = new markset(acc_);

	accp[root_left]->operator |= (*accp[root_right]);
	ufa_.V_write();
      }
    else
      {
	ufa_.P_write();
	if (accp[root_left] == &empty && accp[root_right] == &empty)
	  ;
	else if (accp[root_right] == &empty)
	  {
	    accp[root_right]  = new markset(acc_);
	    accp[root_right]->operator |= (*accp[root_left]);
	  }

    	if (rk_left == rk_right)
    	  {
	    --idneg[root_left];
    	  }
	ufa_.V_write();
      }
  }

  markset
  union_find_shared::get_acc (const fasttgba_state* s)
  {
    trace << "union_find_shared::get_acc" << std::endl << std::flush;
    ufa_.P_read();
    int r = root(el[s]);
    assert(r);
    markset m = *accp[r];
    ufa_.V_read();
    return m;
  }

  void
  union_find_shared::add_acc (const fasttgba_state* s
			      , markset m
			      )
  {
    ufa_.P_write();

    // trace << "union_find_shared::add_acc" << std::endl << std::flush;
    int r = root(el[s]);

    // assert(r > 0);
    if (idneg[r] == 0)
      {
	ufa_.V_write();
	return;
      }

    // Cinstanciate only if necessary
    if (*accp[r] == m)
      {
    	ufa_.V_write();
    	return;
      }

    if (accp[r] == &empty)
      accp[r]  = new markset(empty);

    accp[r]->operator |= (m);
    ufa_.V_write();
  }

  bool union_find_shared::contains (const fasttgba_state* s)
  {
    trace << "union_find_shared::contains" << std::endl << std::flush;
    ufa_.P_read();
    bool res = el.find(s) != el.end();
    ufa_.V_read();
    return res;
  }
}
