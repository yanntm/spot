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
#include "faststate.hh"
#include "fastsucciter.hh"

namespace spot
{
  /// Spot is centered around the spot::tgba type. Here we provide a simplified
  /// interface for designing faster implementations
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
    virtual faststate* get_init_state() const = 0;

    /// \brief Get an iterator over the successors of \a local_state.
    ///
    /// \param local_state The state whose successors are to be explored.
    /// This pointer is not adopted in any way by \c succ_iter, and
    /// it is still the caller's responsability to destroy it when
    /// appropriate (this can be done during the lifetime of
    /// the iterator).
    virtual tgba_succ_iterator*
    succ_iter(const faststate* local_state) const = 0;

    /// \brief Get the dictionary associated to the automaton.
    virtual std::vector<std::string>* get_dict() const = 0;

    /// \brief Format the state as a string for printing.
    ///
    /// This formating is the responsability of the automata
    /// that owns the state.
    virtual std::string format_state(const faststate* state) const = 0;

    /// \brief Return a possible annotation for the transition
    /// pointed to by the iterator.
    virtual std::string
    transition_annotation(const tgba_succ_iterator* t) const;

    /// \brief Project a state on an automaton.
    virtual faststate* project_state(const faststate* s,
				     const fasttgba* t) const;

    /// \brief Return the set of all acceptance conditions used
    /// by this automaton.
    ///
    /// The goal of the emptiness check is to ensure that
    /// a strongly connected component walks through each
    /// of these acceptiong conditions.  I.e., the union
    /// of the acceptiong conditions of all transition in
    /// the SCC should be equal to the result of this function.
    virtual
    boost::dynamic_bitset<> all_acceptance_conditions() const = 0;

    /// The number of acceptance conditions.
    virtual unsigned int number_of_acceptance_conditions() const;

    /// \brief Return the conjuction of all negated acceptance
    /// variables.
    ///
    /// For instance if the automaton uses variables <tt>Acc[a]</tt>,
    /// <tt>Acc[b]</tt> and <tt>Acc[c]</tt> to describe acceptance sets,
    /// this function should return <tt>!Acc[a]\&!Acc[b]\&!Acc[c]</tt>.
    ///
    /// This is useful when making products: each operand's condition
    /// set should be augmented with the neg_acceptance_conditions() of
    /// the other operand.
    virtual
    boost::dynamic_bitset<> neg_acceptance_conditions() const = 0;

  protected:
    mutable int num_acc_;
    mutable int num_var_;
  };
}

#endif // SPOT_FASTTGBA_FASTTGBA_HH
