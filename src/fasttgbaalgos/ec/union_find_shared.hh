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

#ifndef SPOT_FASTTGBAALGOS_EC_UNION_FIND_SHARED_HH
# define SPOT_FASTTGBAALGOS_EC_UNION_FIND_SHARED_HH

#include "misc/hash.hh"
#include "fasttgba/fasttgba.hh"
#include "union_find.hh"

#include <mutex>
#include <condition_variable>

namespace spot
{

  class uf_access
  {
  public:

    uf_access (int n) :
      n_(n), current_(n)
    { }

    virtual ~uf_access()
    { }

    void P_read ()
    {
      // This block is used to acquire a lock
      // and decrement the counter
      std::unique_lock<std::mutex> lk(sem_uf_);
      cond_.wait(lk, [this]{
	  return current_ != 0;
	});
      --current_;
    }

    void P_write ()
    {
      std::lock_guard<std::mutex> lk(sem_w_);
      int tmp = 0;
      while (tmp != n_)
	{
	  std::unique_lock<std::mutex> lk(sem_uf_);
	  cond_.wait(lk, [this]{
	      return current_ != 0;
	    });
	  tmp += current_;
	  current_ = 0;
	}
    }

    void V_read ()
    {
      // This block acquire a lock and release
      // the counter
      std::lock_guard<std::mutex> lk(sem_uf_);
      ++current_;
      cond_.notify_all();
    }

    void V_write ()
    {
      std::lock_guard<std::mutex> lk(sem_uf_);
      current_ = n_;
      cond_.notify_all();
    }

  private:
    int n_;
    int current_;
    std::mutex sem_uf_;
    std::mutex sem_w_;
    std::condition_variable cond_;
  };

 class union_find_shared : public union_find
 {
 public:

   union_find_shared (acc_dict& acc,
		      int n);

   virtual ~union_find_shared ();

   bool add (const fasttgba_state* s);

   void unite (const fasttgba_state* left,
	       const fasttgba_state* right);

   bool same_partition (const fasttgba_state* left,
			const fasttgba_state* right);

   void add_acc (const fasttgba_state* s, markset m);

   markset get_acc (const fasttgba_state* s);

   bool contains (const fasttgba_state* s);

   void make_dead (const fasttgba_state* s);

   bool is_dead (const fasttgba_state* s);

 protected:

   int root (int i);

   uf_access ufa_;
 };
}

#endif // SPOT_FASTTGBAALGOS_EC_UNION_FIND_SHARED_HH
