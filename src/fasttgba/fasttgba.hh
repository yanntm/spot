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

#ifndef SPOT_FASTTGBA_FASTTGBA_HH
# define SPOT_FASTTGBA_FASTTGBA_HH

#include <string>
#include "fasttgba_state.hh"
#include "fasttgba_succ_iterator.hh"
#include "markset.hh"
#include "ap_dict.hh"
#include "acc_dict.hh"

namespace spot
{
  /// Spot is centered around the spot::tgba type. Here we provide a simplified
  /// interface for designing faster implementations.
  class fasttgba
  {
  protected:
    fasttgba();

  public:
    virtual ~fasttgba();

    /// \brief Get the initial state of the automaton.
    ///
    /// The state has been allocated with \c new.  It is the
    /// responsability of the caller to \c destroy it when no
    /// longer needed.
    virtual
    fasttgba_state* get_init_state() const = 0;

    /// \brief Get an iterator over the successors of \a state.
    ///
    /// \param state The state whose successors are to be explored.
    /// This pointer is not adopted in any way by \c succ_iter, and
    /// it is still the caller's responsability to destroy it when
    /// appropriate (this can be done during the lifetime of
    /// the iterator).
    virtual fasttgba_succ_iterator*
    succ_iter(const fasttgba_state* state) const = 0;

    /// \brief Get a Swarm iterator over the sucessors of a \a state
    ///
    /// Such an iterator visits randomly once each outgoing transitions
    /// \param state the states whose successors are to be explored.
    virtual fasttgba_succ_iterator*
    swarm_succ_iter(const fasttgba_state* state) const = 0;

    /// \brief Get the dictionary associated to the automaton.
    virtual
    ap_dict& get_dict() const = 0;

    /// \brief Get the acceptance dictionary associated to the automaton.
    virtual
    acc_dict& get_acc() const = 0;

    /// \brief Format the state as a string for printing.
    ///
    /// This formating is the responsability of the automata
    /// that owns the state.
    virtual std::string format_state(const fasttgba_state* state) const = 0;

    /// \brief Return a possible annotation for the transition
    /// pointed to by the iterator.
    virtual std::string
    transition_annotation(const fasttgba_succ_iterator* t) const = 0;

    /// \brief Project a state on an automaton.
    virtual
    fasttgba_state* project_state(const fasttgba_state* s,
				  const fasttgba* t) const = 0;

    /// \brief Return the set of all acceptance marks used
    /// by this automaton.
    ///
    /// The goal of the emptiness check is to ensure that
    /// a strongly connected component walks through each
    /// of these acceptance conditions.  I.e., the union
    /// of the acceptance conditions of all transition in
    /// the SCC should be equal to the result of this function.
    virtual
    markset all_acceptance_marks() const = 0;

    /// The number of acceptance marks
    virtual
    unsigned int number_of_acceptance_marks() const = 0;

  protected:
    mutable int num_acc_;	///< The number of acceptance mark
    mutable int num_var_;	///< The number of variables
  };
}

#endif // SPOT_FASTTGBA_FASTTGBA_HH
