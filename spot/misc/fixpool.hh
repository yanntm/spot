// -*- coding: utf-8 -*-
// Copyright (C) 2011, 2015-2017 Laboratoire de Recherche et
// Développement de l'Epita (LRDE)
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

#pragma once

#include <spot/misc/common.hh>
#include <new>
#include <cstddef>
#include <cstdlib>

namespace spot
{
  /// A fixed-size memory pool implementation.
  class SPOT_API fixed_size_pool
  {
  public:
    /// Create a pool allocating objects of \a size bytes.
    fixed_size_pool(size_t size);

    /// Free any memory allocated by this pool.
    ~fixed_size_pool()
    {
      while (chunklist_)
        {
          chunk_* prev = chunklist_->prev;
          ::operator delete(chunklist_);
          chunklist_ = prev;
        }
    }

    /// Allocate \a size bytes of memory.
    void*
    allocate()
    {
      block_* f = freelist_;
      // If we have free blocks available, return the first one.
      if (f)
        {
          freelist_ = f->next;
          return f;
        }

      // Else, create a block out of the last chunk of allocated
      // memory.

      // If all the last chunk has been used, allocate one more.
      if (free_start_ + size_ > free_end_)
        {
          const size_t requested = (size_ > 128 ? size_ : 128) * 8192 - 64;
          chunk_* c = reinterpret_cast<chunk_*>(::operator new(requested));
          c->prev = chunklist_;
          chunklist_ = c;

          free_start_ = c->data_ + size_;
          free_end_ = c->data_ + requested;
        }

      void* res = free_start_;
      free_start_ += size_;
      return res;
    }

    /// \brief Recycle \a size bytes of memory.
    ///
    /// Despite the name, the memory is not really deallocated in the
    /// "delete" sense: it is still owned by the pool and will be
    /// reused by allocate as soon as possible.  The memory is only
    /// freed when the pool is destroyed.
    void
    deallocate(void* ptr)
    {
      SPOT_ASSERT(ptr);
      block_* b = reinterpret_cast<block_*>(ptr);
      b->next = freelist_;
      freelist_ = b;
    }

  private:
    const size_t size_;
    struct block_ { block_* next; }* freelist_;
    char* free_start_;
    char* free_end_;
    // chunk = several agglomerated blocks
    union chunk_ { chunk_* prev; char data_[1]; }* chunklist_;
  };

  /// An allocator to be used with STL containers.
  /// It uses a fixed_size_pool to handle memory.
  /// It is intended to improve performance and locality of node-based
  /// containers (std::{unordered}{multi}{set,map}).
  /// It is geared towards efficiently allocating memory for one object at a
  /// time (the nodes of the node-based containers). Larger allocations are
  /// served by calling the global memory allocation mechanism (::operator new).
  /// Using it for contiguous containers (such as std::vector or std::deque)
  /// will be less efficient than using the default std::allocator.
  ///
  /// Short reminder on STL concept of Allocator:
  ///   allocate() may throw
  ///   deallocate() must not throw
  ///   equality testing (i.e. == and !=) must not throw
  ///   copying allocator (constructor and assignment) must not throw
  ///   moving allocator (constructor and assignment) must not throw
  /// It is valid for a noexcept function to call a throwing expression:
  /// any uncaught exception is turned to a call to std::terminate().
  ///
  /// WARNING this class is NOT thread-safe: the allocator relies on a static
  ///   fixed_size_pool (which is not thread-safe either).
  template<class T>
  class pool_allocator
  {
    static
    fixed_size_pool&
    pool()
    {
      static fixed_size_pool p = fixed_size_pool(sizeof(T));
      return p;
    }

  public:
    using value_type = T;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using size_type = size_t;

    constexpr pool_allocator() noexcept
    {}
    template<class U>
    constexpr pool_allocator(const pool_allocator<U>&) noexcept
    {}

    template<class U>
    struct rebind
    {
      using other = pool_allocator<U>;
    };

    pointer
    allocate(size_type n)
    {
      if (SPOT_LIKELY(n == 1))
        return static_cast<pointer>(pool().allocate());
      else
        return static_cast<pointer>(::operator new(n*sizeof(T)));
    }
    // FIXME we could also have a function allocate(n, cvptr)

    void
    deallocate(pointer ptr, size_type n) noexcept
    {
      if (SPOT_LIKELY(n == 1))
        pool().deallocate(static_cast<void*>(ptr));
      else
        ::operator delete(ptr);
    }

    bool
    operator==(const pool_allocator&) const noexcept
    {
      return true;
    }
    bool
    operator!=(const pool_allocator& o) const noexcept
    {
      return !(this->operator==(o));
    }
  };

}
