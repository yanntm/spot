// Copyright (C) 2013 Laboratoire de Recherche et Développement
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

#ifndef SPOT_FASTTGBAALGOS_EC_CONCUR_SHAREDHASHTABLE_HH
# define SPOT_FASTTGBAALGOS_EC_CONCUR_SHAREDHASHTABLE_HH

#include "fasttgba/fasttgba.hh"
#include <iosfwd>
#include <atomic>

// For comparison def!
#include "uf.hh"


#ifdef __cplusplus
extern "C" {
#endif

#include "fasttgbaalgos/ec/concur/hashtable.h"

#ifdef __cplusplus
} // extern "C"
#endif


/// \brief this class acts like a wrapper to the
/// C code of the Hash Table .
namespace spot
{
  class sharedhashtable
  {
  public :
    enum sharedvalues  {
      Alive_state = 1,
      Dead_state = 2
    };


    /// Constructor
    sharedhashtable(int thread_number) : thread_number_(thread_number), size_(0)
    {
      effective_table_ = ht_alloc(&DATATYPE_INT_PTRINT, INIT_SCALE);
    }

    /// Basic destructor
    virtual ~sharedhashtable()
    {

      if (size_)
	{
	  ht_print(effective_table_, true);
	}

      ht_free(effective_table_);
    }


    const fasttgba_state* find_or_put(const fasttgba_state* key)
    {
      assert(key);
      map_val_t old = 0;
      map_key_t clone = 0;

      // Hashmap.insert(map,   key, val, res, context)
      //      Ne pas inserer de clefs negatives
      //      Ne pas inserer de valeur negatives
      old = ht_cas_empty (effective_table_, (map_key_t)key,
      			  (map_val_t)Alive_state, &clone, (void*)NULL);

      // printf("key : %zu, clone : %zu,old :  %zu \n",
      // 	     (size_t)key, (size_t)clone, (size_t)old);
      if (old == DOES_NOT_EXIST)
	{
	  ++size_;
	  return key;
	}

      return (const fasttgba_state*) clone;
    }

   void mark_dead(const fasttgba_state* key)
    {
      assert(false);
      // map_key_t clone = 0;
      // ht_cas (effective_table_, (map_key_t)key, CAS_EXPECT_EXISTS,
      // 	      (map_val_t)Dead_state, &clone, NULL);
    }

    bool is_dead(const fasttgba_state* key)
    {
      if (ht_get(effective_table_, (map_key_t) key) == DOES_NOT_EXIST)
	return false;
      return true;

      // map_val_t old = 0;
      // old = ht_get(effective_table_, (map_key_t) key);
      // if (old == DOES_NOT_EXIST)
      // 	{
      // 	  return false;
      // 	}
      // // State is alive
      // if (old == Alive_state)
      // 	return false;

      // // Not alive
      // return true;
    }


    int size()
    {
      return size_;
    }

  private:
    int thread_number_;			///< The number of threads
    hashtable_t *effective_table_;
    std::atomic<int> size_;
  };
}
#endif // SPOT_FASTTGBAALGOS_EC_CONCUR_SHAREDHASHTABLE_HH
