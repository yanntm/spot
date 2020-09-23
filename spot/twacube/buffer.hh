// -*- coding: utf-8 -*-
// Copyright (C) 2020 Laboratoire de Recherche et Développement de
// l'Epita.
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

#include "config.h"
#include <atomic>
#include <memory>

namespace spot
{
  template <class Stored>
  class buffered_inserter
  {
  public:
    buffered_inserter(size_t capacity, std::function<void(Stored&)> inserter)
      : capacity_(capacity)
      , inserter_(inserter)
    {
      buffer_ = std::make_unique<Stored[]>(capacity_);
    }

    void operator()(Stored value)
    {
      // spinlock while idx isn't in buffer range
      size_t idx;
      while ((idx = index_++) >= capacity_)
        continue;

      // mark our entry in critical section
      workers_++;

      buffer_[idx] = value;
      if (idx == (capacity_ - 1))
        {
          while (workers_ != 1)
            continue;

          // flush buffer
          for (size_t i = 0; i < capacity_; ++i)
            {
              inserter_(buffer_[i]);
            }

          index_ = 0;
        }

      // exit critical section
      workers_--;
    }

  private:
    size_t capacity_;
    std::unique_ptr<Stored[]> buffer_;
    std::atomic<size_t> workers_ = 0;
    std::atomic<size_t> index_ = 0;

    std::function<void(Stored&)> inserter_;
  };
}
