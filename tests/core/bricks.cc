// -*- coding: utf-8 -*-
// Copyright (C) 2016 Laboratoire de Recherche et Développement
// de l'Epita.
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

#include <bricks/brick-hashset>
#include <bricks/brick-hash>

struct both
{
  int x;
  int y;
};

template<typename T>
struct mytest_hasher_both
{
  template<typename X>
  mytest_hasher_both(X&)
  { }

  mytest_hasher_both() = default;

  brick::hash::hash128_t hash(both t) const
  {
    return std::make_pair(t.x*10, t.x);
  }
  bool valid(both t) const
  {
    return t.x != 0;
  }
  bool equal(both a, both b) const
  {
    return a.x == b.x;
  }
};

int main()
{
  // Declare concurrent hash table
  brick::hashset::FastConcurrent<both , mytest_hasher_both<both>> ht2;

  // Declare workers and provide them some jobs.
  std::vector<std::thread> workers;
  for (int i = 0; i < 6; i++)
    workers.
      push_back(std::thread([&ht2](int tid)
                            {
                              for (int i = 0; i< 2000; ++i)
                                ht2.insert({i, tid});
                            }, i));

  // Wait the end of all threads.
  for (auto& t: workers)
    t.join();

  // Display the whole table.
  for (unsigned i = 0; i < ht2.size(); ++ i)
    if (ht2.valid(i))
      std::cout << i << ": {"
                << ht2.valueAt(i).x << ','
                << ht2.valueAt(i).y  << "}\n";
  return 0;
}
