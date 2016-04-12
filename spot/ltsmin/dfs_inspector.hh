// -*- coding: utf-8 -*-
// Copyright (C)  2015, 2016, 2017 Laboratoire de Recherche et
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
    virtual const state* dfs_state(int position) const = 0;

    /// \brief Return true if the parameter state has already been
    /// visited by the DFS.
    virtual bool visited(const state*) const = 0;

    /// \brief Return the iterator associated to a given DFS position.
    virtual twa_succ_iterator* get_iterator(unsigned dfs_position) const = 0;

    /// \brief Return the colors (if exists) associated to a state
    virtual std::vector<bool>& get_colors(const state*) const = 0;

    /// \brief Return the weight (if exists) associated to a state
    virtual int& get_weight(const state*) const = 0;

    /// \brief Retrun true if the state is dead, false otherwise.
    /// If there is not SCC computation this method will return false.
    virtual bool is_dead(const state*) const = 0;

    /// \brief Retrun true if the state is the current root of an SCC.
    /// If the state is not the current root of if there is no SCC computation
    /// this method will return false.
    virtual bool is_root(const state*) const = 0;

    /// \brief Return the highlink of a state or nullptr if this
    /// hoighlink is not yet known.
    virtual const state* get_highlink(const state*) const = 0;

    /// \brief Set the highlink of a state
    virtual void set_highlink(const state* source,
			      const state* highlink) const = 0;

    /// \brief Return the size of the DFS stack.
    virtual unsigned dfs_size() const = 0;

    // \brief Return the reference over the automaton to check
    virtual const const_twa_ptr& automaton() const =  0;


    virtual void* get_extra_data(const state* st) const = 0;
    virtual void set_extra_data(const state* st, void* extra) const = 0;
    virtual bool is_unknown() const = 0;


  };
}
