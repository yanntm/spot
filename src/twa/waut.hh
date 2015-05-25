// -*- coding: utf-8 -*-
// Copyright (C) 2015 Laboratoire de Recherche et
// Développement de l'Epita (LRDE).
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

#include "misc/hash.hh"
#include "misc/casts.hh"
#include "twa/acc.hh"
#include <unordered_map>
#include <set>

namespace spot
{
  // -----------------------------------------------------------------
  // Definitions for generalized weighted automata
  // An automaton is weighted if states holds some values (plus
  // traditionnal acceptances marks)
  // -----------------------------------------------------------------

  // These declaration are needed for w_aut
  class w_state;
  class w_aut_succ_iterator;

  /// \brief The effective declaration of a weighted automaton
  class SPOT_API w_aut
  {
  public:
    virtual const w_state* get_init() const = 0;
    virtual w_aut_succ_iterator*
      succ_iter(const w_state* st) const = 0;
    virtual spot::acc_cond& acceptance() const = 0;
  };

  /// \brief represent weighted states.
  class SPOT_API w_state
  {
  public:
    w_state(int weight, acc_cond::mark_t acc = 0U) : w_(weight), acc_(acc)
    { }

    int weight() const
    {
      return w_;
    }

    acc_cond::mark_t acc() const
    {
      return acc_;
    }

    virtual void destroy() const
    {
      return;
    }

    virtual int compare(const spot::w_state* other) const
    {
      auto o = down_cast<const w_state*>(other);
      assert(o);

      // Do not simply return "other - this", it might not fit in an int.
      if (o < this)
	return -1;
      if (o > this)
	return 1;
      return 0;
    }

    virtual size_t hash() const
    {
      return
	reinterpret_cast<const char*>(this) - static_cast<const char*>(nullptr);
    }

  protected:
    ~w_state()
      {}

  private:
    int w_;
    acc_cond::mark_t acc_;
  };

  struct w_state_ptr_less_than
  {
    bool
    operator()(const w_state* left, const w_state* right) const
    {
      assert(left);
      return left->compare(right) < 0;
    }
  };

  struct w_state_ptr_equal
  {
    bool
    operator()(const w_state* left, const w_state* right) const
    {
      assert(left);
      return 0 == left->compare(right);
    }
  };

  struct w_state_ptr_hash
  {
    size_t
    operator()(const w_state* that) const
    {
      assert(that);
      return that->hash();
    }
  };

  // Some useful typedef to manipulate this kind of automaton
  typedef std::unordered_set<const w_state*,
			     w_state_ptr_hash, w_state_ptr_equal> w_state_set;
  typedef std::unordered_map<const w_state*, int,
			     w_state_ptr_hash,
			     w_state_ptr_equal> w_state_map;


  class SPOT_API w_aut_succ_iterator
  {
  public:
    virtual
    ~w_aut_succ_iterator()
    { }
    virtual bool first() = 0;
    virtual bool next() = 0;
    virtual bool done() const = 0;
    virtual w_state* current_state() const = 0;
  };
}
