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

  class queue
  {
  public :
    queue(int thread_number) : thread_number_(thread_number), curr(0)
    {
      effective_queue = new lf_queue_t*[thread_number_];
      for (int i = 0; i < thread_number_; ++i)
	{
	  effective_queue[i] = lf_queue_alloc();
	}
    }

    int put(op_type op,
	    const spot::fasttgba_state* arg1,
	    const spot::fasttgba_state* arg2,
	    unsigned long acc,
	    int tn)
    {
      int res = lf_queue_put(effective_queue[tn],
			     op, (void*)arg1, (void*)arg2, acc);
      return res;
    }

    // Warning! do not delete the shared_op provided by
    // this method
    shared_op* get(int *stop)
    {
      shared_op* sop = 0;

      while(!*stop)
	{
	  int tmp = curr;
	  curr = (curr+1) % thread_number_;
	  sop = (shared_op*)lf_queue_get_one(effective_queue[tmp]);
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
    std::atomic<int> curr;
  };
}
#endif // SPOT_FASTTGBAALGOS_EC_CONCUR_QUEUE_HH
