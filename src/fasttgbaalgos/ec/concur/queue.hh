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

#ifndef SPOT_FASTTGBAALGOS_EC_CONCUR_QUEUE_HH
# define SPOT_FASTTGBAALGOS_EC_CONCUR_QUEUE_HH

#include "fasttgba/fasttgba.hh"
#include <iosfwd>
#include <atomic>
#include <array>
#include <iostream>
#ifdef __cplusplus
extern "C" {
#endif

#include "fasttgbaalgos/ec/concur/lf_queue.h"
#include "fasttgbaalgos/ec/concur/hashtable.h"

#ifdef __cplusplus
} // extern "C"
#endif

namespace spot
{
// static int
// opvalues_cmp_wrapper(void *hash1, void *hash2)
// {
//   spot::shared_op* left = (spot::shared_op*)hash1;
//   spot::shared_op* right = (spot::shared_op*)hash2;

//   if (left->op_ == right->op_ &&
//       left->arg1_->compare(right->arg1_) == 0 &&
//       left->arg2_->compare(right->arg2_) == 0)
//     return 0;

//   if (left->op_ == right->op_ &&
//     left->arg2_->compare(right->arg1_) == 0 &&
//     left->arg1_->compare(right->arg2_) == 0)
//     return 0;

//   //really?
//   return left->arg1_->compare(right->arg2_);
// }

// uint32_t
// opvalues_hash_wrapper(void *elt, void *ctx)
// {
//   spot::shared_op* e = (spot::shared_op*)elt;
//   int hash1 = e->arg1_? e->arg1_->hash() : 0;
//   int hash2 = e->arg2_? e->arg2_->hash() : 0;
//   return hash1 + hash2;
//   (void)ctx;
// }

// void *
// opvalues_clone_wrapper(void *key, void *ctx)
// {
//   assert(key);
//   return key;
//   (void)ctx;
// }

// void
// opvalues_free_wrapper(void *elt)
// {
//   assert(elt);

//   // Really?
//   // delete (spot::shared_op*)elt;
//   // elt = 0;
//   (void) elt;
// }

// const size_t INIT_SCALE = 12;
// static const datatype_t DATATYPE_OPVALUES =
//   {
//     opvalues_cmp_wrapper,
//     opvalues_hash_wrapper,
//     opvalues_clone_wrapper,
//     opvalues_free_wrapper
//   };


  class queue
  {
    //    hashtable_t *table_;

  public :
    queue(int thread_number) : thread_number_(thread_number), curr(0)
    {
      effective_queue = new lf_queue_t*[thread_number_];
      for (int i = 0; i < thread_number_; ++i)
	{
	  effective_queue[i] = lf_queue_alloc();
	}
      // (void)table_;
      //      table_ = ht_alloc(&DATATYPE_OPVALUES, INIT_SCALE);
    }

    int put(//shared_op* sop,
	    op_type op,
	    const spot::fasttgba_state* arg1,
	    const spot::fasttgba_state* arg2,
	    unsigned long acc,
	    int tn)
    {



      // map_key_t clone = 0;
      // map_val_t old =
      // 	ht_cas_empty (table_, (map_key_t)sop,
      // 		      (map_val_t)1, &clone, (void*)NULL);
      // std::cout << "LA\n";
      // if (old == DOES_NOT_EXIST)
      // 	{
      int res = lf_queue_put(effective_queue[tn],
			     op, (void*)arg1, (void*)arg2, acc
			     );
      return res;
      // 	}
      // return 1;
    }

    // Warning ! this method is not thread safe. To be
    // thread safe, must modify  while condition
    //
    // Warning! do not delete the shared_op provided by
    // this method
    shared_op* get()
    {
      shared_op* sop = 0;

      while(true)
	{
	  sop = (shared_op*)lf_queue_get_one(effective_queue[curr]);
	  curr = (curr+1) % thread_number_;
	  if (sop)
	    break;
	}

      return sop;
    }

    virtual ~queue()
    {
      for (int i = 0; i < thread_number_; ++i)
	{
      	  shared_op* sop;
      	  while ((sop = (shared_op*)lf_queue_get_one(effective_queue[i]))
      		 != 0)
	    ;
	  lf_queue_free (effective_queue[i]);
      	  effective_queue[i] = 0;
      	}
      delete[] effective_queue;
    }

  private:
    lf_queue_t** effective_queue;  ///< \brief one queue per thread
    int thread_number_;		   ///< \brief the number of thread
    int curr;
  };
}
#endif // SPOT_FASTTGBAALGOS_EC_CONCUR_QUEUE_HH
