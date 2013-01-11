// Copyright (C) 2012 Laboratoire de Recherche et Developpement
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

#ifndef SPOT_IFACE_DVE2_DVE2SUCCITER_HH
# define SPOT_IFACE_DVE2_DVE2SUCCITER_HH

# include "dve2callback.hh"
# include "dve2state.hh"
# include "dve2kripke.hh"

namespace spot
{
  class dve2_succ_iterator: public kripke_succ_iterator
  {
  public:

    dve2_succ_iterator(const dve2_callback_context* cc, bdd cond);

    ~dve2_succ_iterator();

    virtual void first();
    virtual void next();
    virtual bool done() const;

    virtual state* current_state() const;

  private:
    const dve2_callback_context* cc_;
    dve2_callback_context::transitions_t::const_iterator it_;
  };

  class one_state_iterator: public kripke_succ_iterator
  {
  public:
    one_state_iterator(const dve2_state* state, bdd cond);

    virtual void first();
    virtual void next();
    virtual bool done() const;

    virtual state* current_state() const;

  protected:
    const dve2_state* state_;
    bool done_;
  };
}

#endif // SPOT_IFACE_DVE2_DVE2SUCCITER_HH
