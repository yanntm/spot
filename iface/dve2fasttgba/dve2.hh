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

#include "tgbaalgos/degen.hh"
#include "tgba/sba.hh"


#include "fasttgba/fasttgba.hh"
#include "fasttgba/fasttgba_product.hh"

#include "tgba/tgba.hh"

// This is for creating Wrappers
#include "fasttgbaalgos/ec/ec.hh"
#include "fasttgbaalgos/tgba2fasttgba.hh"


#include <mutex>

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
      if (verbose)
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



  // A mutex to protect Buddy access
  std::mutex mutex_load_dve;

  /// Wrapper around the product automaton of a system and a
  /// fasttgba
  class dve2product_instance: public instance_automaton
  {
  public:
    dve2product_instance(const spot::dve2product_instance&) = delete;

    dve2product_instance (const spot::tgba* tgba,
			  std::string filename):
      tgba_(tgba), filename_(filename)
    {
      std::lock_guard<std::mutex> lk(mutex_load_dve);

      // Instanciate dictionnaries
      aps_ = new spot::ap_dict();
      accs_ = new spot::acc_dict();

      // Instanciate kripke
      kripke =  spot::load_dve2fast(filename, *aps_, *accs_, true);

      // Instanciate
      ftgba1 = spot::tgba_2_fasttgba(tgba_, *aps_, *accs_);

      prod = new spot::fasttgba_kripke_product (kripke, ftgba1);

      // -------------------------------------------------------------
      // Sometimes we need to work on  BA, in this case the product
      // must be done with BA
      // -------------------------------------------------------------
      if (ftgba1->number_of_acceptance_marks() <= 1)
	{
	  ba_prod = prod;
	  return;
	}

      ba_ = spot::degeneralize(tgba_);


      // Instanciate dictionnaries
      ba_aps_ = new spot::ap_dict();
      ba_accs_ = new spot::acc_dict();

      // Instanciate kripke
      ba_kripke =  spot::load_dve2fast(filename, *ba_aps_, *ba_accs_, true);

      // Instanciate
      ba_ftgba = spot::tgba_2_fasttgba(ba_, *ba_aps_, *ba_accs_);

      ba_prod = new spot::fasttgba_kripke_product (ba_kripke, ba_ftgba);
    }


    virtual ~dve2product_instance()
    {
      std::lock_guard<std::mutex> lk(mutex_load_dve);
      bool is_ba = prod->number_of_acceptance_marks() <= 1;

      // Clean up for Tgba
      delete prod;
      delete ftgba1;
      delete kripke;
      delete accs_;
      delete aps_;
      prod = 0;
      ftgba1 = 0;
      kripke = 0;
      accs_ = 0;
      aps_ = 0;

      // Clean up for BA
      if (!is_ba)
	{
	  delete ba_;
	  delete ba_prod;
	  delete ba_ftgba;
	  delete ba_kripke;
	  delete ba_accs_;
	  delete ba_aps_;
	  ba_prod = 0;
	  ba_ftgba = 0;
	  ba_kripke = 0;
	  ba_accs_ = 0;
	  ba_aps_ = 0;
	}
    }

    // The automaton to check
    const fasttgba* get_automaton () const
    {
      return prod;
    }


    // Sometimes the product need to be with a number
    // acceptance set less or equal to 1
    const fasttgba* get_ba_automaton () const
    {
      return ba_prod;
    }

  private:
    //
    const spot::tgba* tgba_;
    std::string filename_;
    const spot::fasttgba* ftgba1;
    const spot::fasttgba* kripke;
    const spot::fasttgba_kripke_product *prod;
    spot::ap_dict* aps_;
    spot::acc_dict* accs_;


    const spot::tgba* ba_;
    const spot::fasttgba* ba_ftgba;
    const spot::fasttgba* ba_kripke;
    const spot::fasttgba_kripke_product *ba_prod;
    spot::ap_dict* ba_aps_;
    spot::acc_dict* ba_accs_;
  };


  /// A dve2 product instanciator
  ///
  /// FIXME: This class should contain the dictionnary to
  /// avoid redefinitions for each instance.
  class dve2product_instanciator: public instanciator
  {
  private:
    const spot::tgba* tgba_;
    std::string filename_;

  public:
    dve2product_instanciator (const spot::tgba* tgba,
			      std::string filename):
      tgba_(tgba), filename_(filename)
    {
    }

    const instance_automaton* new_instance ( )
    {
      return new dve2product_instance(tgba_, filename_);
    }
  };













}

#endif // SPOT_IFACE_DVE2FASTTGBA_DVE2_HH
