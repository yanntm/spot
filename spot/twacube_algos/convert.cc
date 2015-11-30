// -*- coding: utf-8 -*-
// Copyright (C) 2015, 2016 Laboratoire de Recherche et Developpement de
// l'Epita (LRDE).
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

#include <spot/twacube_algos/convert.hh>
#include <assert.h>

namespace spot
{
  spot::cube satone_to_cube(bdd one, cubeset& cubeset,
			    std::unordered_map<int, int>& binder)
  {
    auto cube = cubeset.alloc();
    while (one != bddtrue)
      {
	if (bdd_high(one) == bddfalse)
	  {
	    assert(binder.find(bdd_var(one)) != binder.end());
	    cubeset.set_false_var(cube, binder[bdd_var(one)]);
	    one = bdd_low(one);
	  }
	else
	  {
	    assert(binder.find(bdd_var(one)) != binder.end());
	    cubeset.set_true_var(cube, binder[bdd_var(one)]);
	    one = bdd_high(one);
	  }
      }
    return cube;
  }

  bdd cube_to_bdd(spot::cube cube, const cubeset& cubeset,
		  std::unordered_map<int, int>& reverse_binder)
  {
    bdd result = bddtrue;
    for (unsigned int i = 0; i < cubeset.size(); ++i)
      {
    	assert(reverse_binder.find(i) != reverse_binder.end());
	if (cubeset.is_false_var(cube, i))
    	  result &= bdd_nithvar(reverse_binder[i]);
	if (cubeset.is_true_var(cube, i))
    	  result &= bdd_ithvar(reverse_binder[i]);
      }
    return result;
  }
}
