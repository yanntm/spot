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

#ifdef __cplusplus
extern "C" {
#endif

#include "fasttgbaalgos/ec/concur/lf_queue.h"

#ifdef __cplusplus
} // extern "C"
#endif

namespace spot
{
  enum op_type
    {
      unite,			// Ask for unite operation
      makeset,			// Ask for creation
      end			// This thread is iddle
    };

  struct shared_op
  {
    op_type op;
    const spot::fasttgba_state* arg1;
    const spot::fasttgba_state* arg2;
  };

  class queue
  {
  public :
    queue(int thread_number) : thread_number_(thread_number), nb_iddle(0)
    {
      effective_queue = new lf_queue_t*[thread_number_];
      //available_ = new std::vector<std::atomic<bool>>(thread_number_);
      for (int i = 0; i < thread_number_; ++i)
	{
	  effective_queue[i] = lf_queue_alloc();
	  //  (*available_)[i] = false;
	}
    }

    int put(shared_op* sop, int tn)
    {
      int res = lf_queue_put(effective_queue[tn], sop);
      //(*available_)[tn] = true;
      return res;
    }


    // Warning ! this method is not thread safe. To be
    // thread safe, must make curr atomic and modify
    // while condition
    shared_op* get(int* the_end)
    {
      assert(nb_iddle != thread_number_);
      shared_op* sop;

      while(true)
	{
	  sop = (shared_op*)lf_queue_get_one(effective_queue[curr]);
	  curr = (curr+1) % thread_number_;
	  if (sop)
	    break;
	}
      if (sop->op == end)
	++nb_iddle;

      *the_end = (nb_iddle == thread_number_);
      return sop;
    }

    virtual ~queue()
    {
      for (int i = 0; i < thread_number_; ++i)
	lf_queue_free (effective_queue[i], 0);
      delete[] effective_queue;
      //delete available_;
    }

  private:
    lf_queue_t** effective_queue;  ///< \brief one queue per thread
    int thread_number_;		   ///< \brief the number of thread
    //    std::vector<std::atomic<bool>>* available_;
    int nb_iddle;
    int curr = 0;
  };
}
#endif // SPOT_FASTTGBAALGOS_EC_CONCUR_QUEUE_HH
