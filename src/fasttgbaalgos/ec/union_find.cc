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
    id.push_back(0);
    rk.push_back(0);
    acc.push_back(empty);

    //uf.push_back(boost::make_tuple(0,0,empty));
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
    trace << "union_find::add " << id.size() << std::endl;
    el.insert(std::make_pair(s, id.size()));
    id.push_back(id.size());
    rk.push_back(0);
    acc.push_back(empty);


    // el.insert(std::make_pair(s, uf.size()));
    // uf.push_back(boost::make_tuple(uf.size(),0,empty));
  }

  int union_find::root (int i)
  {
    int p = id[i];
    if (i == p || p == id[p])
      return p;
    p = root(p);
    return id[i] = p;

    // int p = uf[i].get<0>();
    // if (i == p || p == uf[p].get<0>())
    //   return p;
    // return uf[i].get<0>() = root(p);
  }

  bool
  union_find::same_partition (const fasttgba_state* left,
			      const fasttgba_state* right)
  {
    // assert(left);
    // assert(right);
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
    id[root(el[s])] = 0;


    // trace << "union_find::make_dead " << el[s] << " root_ :"
    // 	  << root(el[s]) << std::endl;
    // uf[root(el[s])].get<0>() = 0;
  }

  bool
  union_find::is_dead(const fasttgba_state* s)
  {
    return root(el[s]) == 0;
  }

  void
  union_find::unite (const fasttgba_state* left,
		     const fasttgba_state* right)
  {
    // assert(contains(left));
    // assert(contains(right));
    int root_left = root(el[left]);
    int root_right = root(el[right]);

    trace << "union_find::unite "
    	  << root_left << " " << root_right << std::endl;

    // assert(root_left);
    // assert(root_right);

    int rk_left = rk[root_left];
    int rk_right = rk[root_right];

    // Use ranking
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





    // // assert(contains(left));
    // // assert(contains(right));
    // int root_left = root(el[left]);
    // int root_right = root(el[right]);

    // trace << "union_find::unite "
    // 	  << root_left << " " << root_right << std::endl;

    // // assert(root_left);
    // // assert(root_right);

    // int rk_left = uf[root_left].get<1>();
    // int rk_right = uf[root_right].get<1>();

    // // Use ranking
    // if (rk_left < rk_right)
    //   {
    // 	uf [root_left].get<0>() =  root_right;
    // 	uf [root_left].get<2>() |= uf[root_right].get<2>();
    //   }
    // else
    //   {
    // 	uf [root_right].get<0>() =  root_left;
    // 	uf [root_right].get<2>() |= uf[root_left].get<2>();

    // 	if (rk_left == rk_right)
    // 	  {
    // 	    ++uf[root_left].get<1>();
    // 	  }
    //   }
  }

  markset
  union_find::get_acc (const fasttgba_state* s)
  {
    trace << "union_find::get_acc" << std::endl;
    int r = root(el[s]);
    // assert(r);
    return acc[r];

    // return uf [root(el[s])].get<2>();
  }

  void
  union_find::add_acc (const fasttgba_state* s, markset m)
  {
    trace << "union_find::add_acc" << std::endl;
    int r = root(el[s]);
    // assert(r);
    acc[r] |= m;

    // uf [root(el[s])].get<2>() |= m;
  }

  bool union_find::contains (const fasttgba_state* s)
  {
    trace << "union_find::contains" << std::endl;
    return el.find(s) != el.end();
  }
}
