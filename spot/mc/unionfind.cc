// -*- coding: utf-8 -*-
// Copyright (C) 2015, 2016 Laboratoire de Recherche et
// Developpement de l'Epita
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

#include <spot/mc/unionfind.hh>
#include <assert.h>

namespace spot
{
  int int_unionfind::root(int index)
  {
  assert(index >= 0);
    int p = id[index];
    if (p == DEAD)
      return DEAD;
    if (p < 0)
      return index;
    int gp = id[p];
    if (gp == DEAD)
      return DEAD;
    if (gp < 0)
      return p;
    p = root(p);
    id[index] = p;
    return p;
  }

  int_unionfind::int_unionfind() : id()
  {
  }

inline unsigned int_unionfind::find_node_map (int e)
{

return (index_node_map[e]);


}


  void int_unionfind::makeset(int e)
  {

    assert(!contains(e));
    index_node_map.insert(std::pair<int,int>(e, id.size())); 
    id.push_back(-1);
    dead.push_back(0);
  }

  bool int_unionfind::unite(int e1_state, int e2_state)
  {
    
int e1 = find_node_map (e1_state);
int e2 = find_node_map (e2_state);
    
    
    // IPC - Immediate Parent Check
    int p1 = id[e1];
    int p2 = id[e2];
    if ((p1 < 0 ? e1 : p1) == (p2 < 0 ? e2 : p2))
     return false;
    int root1 = root(e1);
    int root2 = root(e2);
    if (root1 == root2)
      return false;
    int rk1 = -id[root1];
    int rk2 = -id[root2];
    if (rk1 < rk2)
    {
      id[root1] = root2;
    }
    else {
      id[root2] = root1;
    }

    return true;
  }


  unsigned int_unionfind::count_root ()
  {
    unsigned count = 0;
    
    for (unsigned i = 0;i < dead.size();i++)        
    {
      if (0 != dead[i])
      {
        count++;
      }
    }
    return count;
  }





  bool int_unionfind::contains (unsigned e_state)
  {
    
    if(index_node_map.find(e_state) == index_node_map.end())
    {
      return false;
    }
    else
    {
      return true;
    }
  }


  void int_unionfind::markdead(int e_state)
  {

    int e = find_node_map (e_state);
    int root_value = root(e);
   if (root_value != DEAD )
    {
    dead[root_value] = 1; 
    id[root_value] = DEAD;
   }
  }

  bool int_unionfind::sameset(int e1_state, int e2_state)
  {
  assert(contains(e1_state)  && contains(e2_state)); 
  int e1 = find_node_map (e1_state);
  int e2 = find_node_map (e2_state);
   
    return root(e1) == root(e2);
  }

  bool int_unionfind::isdead(int e_state)
  {
    int e = find_node_map (e_state);
    
    
    return root(e) == DEAD;
  }
}
