// Copyright (C) 2012 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
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

#ifndef SPOT_FASTTGBAALGOS_EC_EC_HH
# define SPOT_FASTTGBAALGOS_EC_EC_HH

#include "fasttgba/fasttgba.hh"

namespace spot
{
  /// This interface must be implemented by all
  /// emptiness check algorithm
  class ec
  {
  public:

    /// Launch the emptiness check
    virtual bool check() = 0;

    virtual ~ec() {}
  };


  /// \brief A simple wrapper for an automaton
  class instance_automaton
  {
  public :
    /// \brief return the automaton.
    /// Warning ! You don't have to delete the
    // automaton just this class to destroy this automaton
    virtual const spot::fasttgba* get_automaton() const = 0;

    /// \brief return the automaton as a B\¨uchi Automaton.
    /// Warning ! You don't have to delete the
    // automaton just this class to destroy this automaton
    virtual const spot::fasttgba* get_ba_automaton() const = 0;

    virtual ~instance_automaton()
    {
    }
  };

  /// A wrapper around the automaton to check is needed for
  /// some emptiness check that needs to work on multiple distinc instance
  /// of the automaton
  ///
  /// In this case, the emptiness check just have to know a specific
  /// instanciator to perform the emptiness check
  class instanciator
  {
  public:
    /// \brief return a new instance of the automaton
    virtual const instance_automaton* new_instance () = 0;

    virtual ~instanciator()
    {
    }
  };



 class simple_instance: public instance_automaton
  {
  public:
    simple_instance(const spot::simple_instance&) = delete;

    simple_instance (const spot::fasttgba* tgba):
      ftgba_(tgba)
    {
    }

    virtual ~simple_instance()
    {
    }

    const spot::fasttgba* get_automaton () const
    {
      return ftgba_;
    }

    const spot::fasttgba* get_ba_automaton () const
    {
      // Not Yet Supported
      assert(false);
      return 0;
    }

  private:
    const spot::fasttgba* ftgba_;
  };




  class simple_instanciator: public instanciator
  {
  private:
    const spot::fasttgba* tgba_;

  public:
    simple_instanciator (const spot::fasttgba* tgba):
      tgba_(tgba)
    {
    }

    const instance_automaton* new_instance ()
    {
      return new simple_instance(tgba_);
    }
  };


}

#endif // SPOT_FASTTGBAALGOS_EC_EC_HH
