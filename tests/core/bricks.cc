// -*- coding: utf-8 -*-
// Copyright (C) 2016, 2018, 2020 Laboratoire de Recherche et Développement
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

#include "config.h"
#include <spot/bricks/brick-hashset>
#include <spot/bricks/brick-hash>
#include <chrono>

using namespace std::chrono_literals;

struct both
{
  both() = default;
  int x;
  int y;

  auto hash() const
  {
    // Not modulo 31 according to brick::hashset specifications.
    unsigned u = x % (1<<30);
    return u;
  }

  inline bool operator==(const both rhs) 
  {
    std::cout << "                       Compare it\n";
    return x == rhs.x && y == rhs.y;
  }

};


static inline bool equals(const both* lhs, const both* rhs) 
{
  std::cout << "                       Compare it (ptr)\n";
  return lhs->x == rhs->x && lhs->x == rhs->y;
}


/// \brief The haser for the previous state.
struct both_hasher : brq::hash_adaptor<both>
{
  both_hasher() = default;

  auto hash(const both lhs) const
  {
    // Not modulo 31 according to brick::hashset specifications.
    std::cout << "                       HASH it!\n";
    unsigned u = lhs.x % (1<<30);
    return u;
  }
};

static int main_raw_element()
{
  // Declare concurrent hash table
  brq::concurrent_hash_set<both> ht;

  // Declare workers and provide them some jobs.
   std::vector<std::thread> workers;
  for (int i = 0; i < 4; i++)
    workers.
      push_back(std::thread([](int tid, brq::concurrent_hash_set<both> ht) throw()
                            {
                              for (int i = 0; i< 200; ++i)
                                {
                                  std::cout << "i: " << i << std::endl;
                                  ht.insert(both{i%2, tid}, both_hasher());
                                }
                            }, i, ht));

  //Wait the end of all threads.
  for (auto& t: workers)
    t.join();

  // Display the whole table.
  for (unsigned i = 0; i < ht.capacity(); ++ i)
    if (ht.valid(i))
      std::cout << i << ": {"
                << ht.valueAt(i).x << ','
                << ht.valueAt(i).y  << "}\n";
  return 0;
}

/// \brief The haser for the previous state.
struct both_ptr_hasher : brq::hash_adaptor<both*>
{
  both_ptr_hasher() = default;

  auto hash(const both* lhs) const
  {
    // Not modulo 31 according to brick::hashset specifications.
    std::cout << "                       HASH it!\n";
    unsigned u = lhs->x % (1<<30);
    return u;
  }

  // Ouch i do not like that.
  using hash64_t = uint64_t;
  template< typename cell >
  typename cell::pointer match( cell &c, const both* t, hash64_t h ) const
  {
    // NOT very sure that dereferecing will not just kill some brick property
    return c.match( h ) && *(c.fetch()) == *(t) ? c.value() : nullptr;
  }
};

static int main_ptr_element()
{
  // Declare concurrent hash table
  brq::concurrent_hash_set<both*> ht;

  // Declare workers and provide them some jobs.
   std::vector<std::thread> workers;
  for (int i = 0; i < 1; i++)
    workers.
      push_back(std::thread([](int tid, brq::concurrent_hash_set<both*> ht) throw()
                            {
                              for (int i = 0; i< 200; ++i)
                                {
                                  std::cout << "i: " << i << std::endl;
                                  // FIXME Dealloc new
                                  ht.insert(new both{i%2, tid}, both_ptr_hasher());
                                }
                            }, i, ht));

  //Wait the end of all threads.
  for (auto& t: workers)
    t.join();

  // Display the whole table.
  for (unsigned i = 0; i < ht.capacity(); ++ i)
    if (ht.valid(i))
      std::cout << i << ": {"
                << ht.valueAt(i)->x << ','
                << ht.valueAt(i)->y  << "}\n";
  return 0;
}

int main()
{
  main_raw_element();
  std::cout << "----------------------------------------------------\n";
  main_ptr_element();
  return 0;
}
