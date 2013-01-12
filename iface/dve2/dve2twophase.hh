// Copyright (C) 2012 Laboratoire de Recherche et Developpement
// de l'Epita (LRDE)
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

#ifndef SPOT_IFACE_DVE2_DVE2TWOPHASE_HH
# define SPOT_IFACE_DVE2_DVE2TWOPHASE_HH

# include "dve2kripke.hh"
# include "dve2state.hh"

namespace spot
{
  class dve2_twophase
  {
  protected:
    typedef Sgi::hash_set<dve2_state*, state_ptr_hash,
			  state_ptr_equal> state_set;

  public:
    dve2_twophase(const dve2_kripke* k);

    const dve2_state* phase1(const int* in, bdd form_vars = bddfalse) const;

  protected:
    typedef por_callback::trans trans;

    bool internal(const trans& t, int p) const;
    bool only_one_enabled(const int* s, int p,
			  const std::list<trans>& tr, trans& t,
			  const state_set& visited) const;
    bool deterministic(const int* s, unsigned p, const std::list<trans>& tr,
		       trans& res, const state_set& visited,
		       bdd form_vars = bddfalse) const;

    const dve2_kripke* k_;
  };
}

#endif // SPOT_IFACE_DVE2_DVE2TWOPHASE_HH
