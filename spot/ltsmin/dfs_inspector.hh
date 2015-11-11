// -*- coding: utf-8 -*-
// Copyright (C)  2015, 2017 Laboratoire de Recherche et
// Developpement de l'Epita (LRDE)
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

#include <vector>
#include <spot/twa/twa.hh>

namespace spot
{
  class SPOT_API dfs_inspector
  {
  public:
    /// \brief Return -1 if the parameter state \a is unknown or
    /// not in the DFS. Otherwise return the position of this state.
    virtual int dfs_position(const state*) const = 0;

    /// \brief Return true if the parameter state has already been
    /// visited by the DFS.
    virtual bool visited(const state*) const = 0;

    /// \brief Return the iterator associated to a given DFS position.
    virtual twa_succ_iterator* get_iterator(unsigned dfs_position) const = 0;

    /// \brief Return the colors (if exists) associated to a state
    virtual std::vector<bool>& get_colors(const state*) const = 0;
  };
}
