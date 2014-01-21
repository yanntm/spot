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

#ifndef SPOT_FASTTGBA_FASTTGBA_EXPLICIT_HH
# define SPOT_FASTTGBA_FASTTGBA_EXPLICIT_HH

#include <list>
#include "fasttgba.hh"
#include "misc/hash.hh"

namespace spot
{
  class fast_explicit_state;

  ///
  /// \brief the structure that will store the successors of a state
  ///
  struct transition
  {
    cube conditions;			///< condition over an arc
    markset acceptance_marks;	        ///< acceptance mark over an arc
    const fast_explicit_state* dst;	///< the destination state

    virtual ~transition(){};

    transition (cube c, markset a, const fast_explicit_state* d)
      : conditions(c),
	acceptance_marks(a)
    {
      dst = d;
    }

  };


  /// \brief this enum is used to provide the stregth of
  /// the SCC
  enum scc_strength {
    STRONG_SCC,
    WEAK_SCC,
    TERMINAL_SCC,
    NON_ACCEPTING_SCC,
    UNKNOWN_SCC
  };

  enum aut_strength {
    STRONG_AUT,
    WEAK_AUT,
    TERMINAL_AUT,
  };


  ///
  /// This class represent an explicit numbered state which
  /// is usefull to represent formulae
  ///
  class fast_explicit_state : public fasttgba_state
  {
  protected:
    int label_;
    enum scc_strength strength_;
  public:
    fast_explicit_state(int label);
    virtual int compare(const fasttgba_state* other) const;
    virtual size_t hash() const;
    virtual fasttgba_state* clone() const;
    virtual void* external_information() const;
    virtual int label() const;
    virtual void destroy() const;

    //// \brief the structure to store transitions
    void add_successor(const struct transition *t);

    /// \brief the strength of the SCC
    void set_strength(enum scc_strength str);

    enum scc_strength get_strength() const;

    std::vector<const struct transition*>  successors;	///< list of successors

    mutable int count_;
  };


  ///
  /// \brief an iterator for fast_explicit_state
  ///
  /// This class brings noting particular just an implementation
  ///
  class fast_explicit_iterator : public fasttgba_succ_iterator
  {
  protected :
    const fast_explicit_state* start_; ///< src of iterator
    std::vector
    <const struct transition*>::const_iterator it_; ///< current iterator
    bool swarming_;
    std::vector<int> crossref_;
    std::vector<int>::const_iterator it_ref;
  public:
    fast_explicit_iterator(const fast_explicit_state* state,
			   bool swarming = false);
    virtual ~fast_explicit_iterator();
    virtual void first();
    virtual void next();
    virtual bool done() const;
    virtual fasttgba_state* current_state() const;
    virtual cube current_condition() const;
    virtual markset current_acceptance_marks() const;
  };


  ///
  /// This class is the concrete implementation of the fasttgba
  /// interface. Moreover this interface provides all what is needed
  /// to build such an automaton
  ///
  class SPOT_API fasttgbaexplicit : public fasttgba
  {
  public:

    // -------------------------------------------------------
    // The FASTTGBA interface
    // -------------------------------------------------------

    /// \brief Constructor for a fasttgba
    ///
    /// \param aps a set of atomic proposition name
    /// \param acc a set of acceptance variable name
    fasttgbaexplicit(ap_dict* aps,
		     acc_dict* acc);

    virtual ~fasttgbaexplicit();

    virtual fasttgba_state* get_init_state() const;

    virtual fasttgba_succ_iterator*
    succ_iter(const fasttgba_state* local_state) const;

    virtual fasttgba_succ_iterator*
    swarm_succ_iter(const fasttgba_state* state) const;

    virtual
    ap_dict& get_dict() const;

    virtual
    acc_dict& get_acc() const;

    virtual
    std::string format_state(const fasttgba_state* state) const;

    virtual std::string
    transition_annotation(const fasttgba_succ_iterator* t) const;

    virtual fasttgba_state* project_state(const fasttgba_state* s,
					  const fasttgba* t) const;
    virtual
    markset all_acceptance_marks() const;

    virtual
    unsigned int number_of_acceptance_marks() const;

    /// \brief Ease to detect the strength of an automaton
    ///
    /// The strength of an automaton is the strength of
    /// the highest SCC. If we are not able to detect the
    /// strength of an SCC, the automaton must be consider
    /// strong.
    aut_strength get_strength() const
    {
      return strength_;
    }

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
    fast_explicit_state* add_state(int s);

    /// \brief Provide the way to create a transision between two states
    ///
    /// \param src the label of the source state
    /// \param dst the label of the destination state
    /// \param cond the atomic proposition labelling the transition
    /// \param cond the accepting mark on this transition
    void add_transition(int src, int dst,
			cube cond, markset acc);

    /// \brief set the strength of an automaton
    void set_strength(aut_strength s)
    {
      strength_ = s;
    }

  protected:
    markset all_marks_;	            ///< the set of acceptance mark
    ap_dict* aps_;                  ///< The set of atomic proposition
    acc_dict* acc_;                 ///< The set of acceptance mark
    const fasttgba_state* init_;

    typedef Sgi::hash_map<int, fast_explicit_state*, identity_hash<int> > sm;
    sm state_map_;		///< The states of the automaton
    aut_strength strength_;
  };
}

#endif // SPOT_FASTTGBA_FASTTGBA_EXPLICIT_HH
