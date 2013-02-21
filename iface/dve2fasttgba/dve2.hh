// Copyright (C) 2011 Laboratoire de Recherche et Developpement
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

#ifndef SPOT_IFACE_DVE2FASTTGBA_DVE2_HH
# define SPOT_IFACE_DVE2FASTTGBA_DVE2_HH

// #include "kripke/kripke.hh"
// #include "ltlvisit/apcollect.hh"
// #include "ltlast/constant.hh"

#include "fasttgba/fasttgba.hh"
#include "fasttgba/fasttgba_product.hh"

namespace spot
{
  class fasttgba_kripke_product : public fasttgba_product
  {
  public:
    fasttgba_kripke_product(const fasttgba *a1,
			    const fasttgba *a2,
			    bool verbose = true)
      : fasttgba_product(a1, a2, true),
	verbose_(verbose)
    {
      match_formula_ap();
    }

    ///
    ///
    ///
    void match_formula_ap ();

  private:
    bool verbose_;
  };


  // \brief Load a DVE model.
  //
  // The filename given can be either a *.dve source or a *.dve2C
  // dynamic library compiled with "divine compile --ltsmin file".
  // When the *.dve source is supplied, the *.dve2C will be updated
  // only if it is not newer.
  //
  // The dead parameter is used to control the behavior of the model
  // on dead states (i.e. the final states of finite sequences).
  // If DEAD is "false", it means we are not
  // interested in finite sequences of the system, and dead state
  // will have no successor.  If DEAD is
  // "true", we want to check finite sequences as well as infinite
  // sequences, but do not need to distinguish them.  In that case
  // dead state will have a loop labeled by true.  If DEAD is any
  // other string, this is the name a property that should be true
  // when looping on a dead state, and false otherwise.
  //
  // This function returns 0 on error.
  //
  // \a file the name of the *.dve source file or of the *.dve2C
  //         dynamic library
  // \a to_observe the list of atomic propositions that should be observed
  //               in the model
  // \a dict the BDD dictionary to use
  // \a dead an atomic proposition or constant to use for looping on
  //         dead states
  // \a verbose whether to output verbose messages
  const spot::fasttgba* load_dve2fast(const std::string& file,
				      spot::ap_dict& aps,
				      spot::acc_dict& accs,
				      bool verbose = true);
}

#endif // SPOT_IFACE_DVE2FASTTGBA_DVE2_HH
