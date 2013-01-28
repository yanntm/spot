// -*- coding: utf-8 -*-
// Copyright (C) 2012 Laboratoire de Recherche et Développement
// de l'Epita (LRDE)
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Spot; see the file COPYING.  If not, write to the Free
// Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.

#include "dve2succiter.hh"

#include <cstring>

namespace spot
{
  ////////////////////////////////////////////////////////////////////////
  // DVE2 ITERATOR

  dve2_succ_iterator::dve2_succ_iterator(const dve2_callback_context* cc,
					 bdd cond)
    : kripke_succ_iterator(cond)
      , cc_(cc)
  {
  }

  dve2_succ_iterator::~dve2_succ_iterator()
  {
    delete cc_;
  }

  void
  dve2_succ_iterator::first()
  {
    it_ = cc_->transitions.begin();
  }

  void
  dve2_succ_iterator::next()
  {
    ++it_;
  }

  bool
  dve2_succ_iterator::done() const
  {
    return it_ == cc_->transitions.end();
  }

  state*
  dve2_succ_iterator::current_state() const
  {
    return (*it_)->clone();
  }

  ////////////////////////////////////////////////////////////////////////
  // ONE STATE ITERATOR

  one_state_iterator::one_state_iterator(const dve2_state* state, bdd cond)
    : kripke_succ_iterator(cond)
      , state_(state)
      , done_(false)
  {
  }

  void
  one_state_iterator::first()
  {
    done_ = false;
  }

  void
  one_state_iterator::next()
  {
    done_ = true;
  }

  bool
  one_state_iterator::done() const
  {
    return done_;
  }

  state*
  one_state_iterator::current_state() const
  {
    return state_->clone();
  }
}
