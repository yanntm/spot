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

#ifndef SPOT_FASTTGBA_FASTTGBAEXPLICIT_HH
# define SPOT_FASTTGBA_FASTTGBAEXPLICIT_HH

#include "fasttgba.hh"
#include "misc/hash.hh"

namespace spot
{

  /// This class represent an explicit numbered state which
  /// is usefull to represent formulae
  class fast_explicit_state : public faststate
  {
  protected:
    int label_;

  public:
    fast_explicit_state(int label):
      label_(label)
    {
    }

    virtual int compare(const faststate* other) const
    {
      return label_ - ((const fast_explicit_state*)other)->label_;
    }

    virtual size_t hash() const
    {
      return label_;
    }

    virtual faststate* clone() const
    {
      return new fast_explicit_state(label_);
    }

    virtual void* external_information() const
    {
      assert(false);
    }
  };

  /// This class is the concrete implementation of the fasttgba
  /// interface. Moreover this interface provides all what is needed
  /// to build such an automaton
  ///
  /// This class is a template class to avoid redefinition
  class fasttgbaexplicit : public fasttgba
  {
  public:

    // -------------------------------------------------------
    // The FASTTGBA interface
    // -------------------------------------------------------

    fasttgbaexplicit(int num_acc, std::vector<std::string> aps);

    virtual ~fasttgbaexplicit();

    virtual faststate* get_init_state() const;

    virtual fasttgba_succ_iterator*
    succ_iter(const faststate* local_state) const;

    virtual
    std::vector<std::string> get_dict() const;

    virtual
    std::string format_state(const faststate* state) const;

    virtual std::string
    transition_annotation(const fasttgba_succ_iterator* t) const;

    virtual faststate* project_state(const faststate* s,
				     const fasttgba* t) const;
    virtual
    cube all_acceptance_conditions() const;

    virtual
    unsigned int number_of_acceptance_conditions() const;

    virtual
    cube neg_acceptance_conditions() const;

    // -------------------------------------------------------
    // This part is for creating a new FASTTGBAEXPLICIT
    // -------------------------------------------------------

    /// \brief Provide the manner to add a new state to this automaton
    ///
    /// If it's the first state added to this automaton, then this
    /// state becomes the initial one
    ///
    /// \param s the integer name for the state
    /// \return true if the state was not already in the automaton
    bool add_state(int s);

    /// \brief Provide the way to create a transision between two states
    ///
    /// \param src the source state
    /// \param dst the destination state
    /// \param cond the atomic proposition labelling the transition
    /// \param cond the accepting mark on this transition
    void add_transition(faststate* src, faststate* dst,
			cube cond, cube acc);

  protected:
    cube all_cond_;	            ///< the set of acceptance mark
    cube all_cond_neg_;            ///< the negation of all_cond
    std::vector<std::string> aps_; ///< The set of atomic proposition


    typedef Sgi::hash_map<int, fast_explicit_state, identity_hash<int> > sm;
    sm state_map_;
  };
}

#endif // SPOT_FASTTGBA_FASTTGBAEXPLICIT_HH
