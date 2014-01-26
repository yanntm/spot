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

#ifndef SPOT_FASTTGBA_FASTTGBA_PRODUCT_HH
# define SPOT_FASTTGBA_FASTTGBA_PRODUCT_HH

#include <list>
#include "fasttgba.hh"
#include "misc/hash.hh"
#include "misc/fixpool.hh"

namespace spot
{
  /// \brief This class represents a product state
  ///
  /// This state is composed of a left and a right part
  /// that also are fast states
  ///
  class fast_product_state : public fasttgba_state
  {
  public:
    // ------------------------------------------------------------
    // Fast tgba state interface
    // ------------------------------------------------------------
    fast_product_state(const fasttgba_state* left,
		       const fasttgba_state* right,
		       fixed_size_pool* p);
    virtual int compare(const fasttgba_state* other) const;
    virtual size_t hash() const;
    virtual fasttgba_state* clone() const;
    virtual void* external_information() const;
    virtual void destroy() const;

    /// \brief An accessor to the left part of a state
    virtual const fasttgba_state* left() const;

    /// \brief An accessor to the right part of a state
    virtual const fasttgba_state* right() const;

  private:
    const fasttgba_state* left_;
    const fasttgba_state* right_;
    mutable int count_;
    fixed_size_pool* pool_;
  };

  /// \brief This is an iterator for the product
  ///
  /// This iterator is in charge to compute only valid successors
  /// it means sucessors that are correct in the meaning of cube.
  /// If t1 and t2 are resp. labelled by a and !a then no synchro-
  /// -nisation is possible
  ///
  ///
  /// When performing the synchronisation of two tansitions
  /// t1(src_1, ap_1, \alpha, dst_1) and t2(src_2, ap_2, \beta, dst_2)
  /// if ap_1 and ap_2 are compatible then the resulting transition
  /// would be :
  ///   t ((src_1, src_2), (ap_1 & ap_2), (\alpha|\beta), (dst_1, dst_2))
  ///
  class fast_product_iterator : public fasttgba_succ_iterator
  {
  public:
    // ------------------------------------------------------------
    // Fast tgba succ iterator interface
    // ------------------------------------------------------------
    fast_product_iterator(fasttgba_succ_iterator* left,
			  fasttgba_succ_iterator* right,
			  fixed_size_pool* p,
			  bool left_is_kripke = false);
    virtual ~fast_product_iterator();
    virtual void first();
    virtual void next();
    virtual bool done() const;
    virtual fasttgba_state* current_state() const;
    virtual cube current_condition() const;
    virtual markset current_acceptance_marks() const;

    /// An accessor to the left iterator
    virtual fasttgba_succ_iterator* left() const;

    /// An accessor to the right iterator
    virtual fasttgba_succ_iterator* right() const;

  protected:
    /// This method is used to compute a step in the iterator
    /// considering the two sub iterators.
    void step();

    fasttgba_succ_iterator* left_;  ///< Reference on the left iterator
    fasttgba_succ_iterator* right_; ///< Reference on the right iterator
    fixed_size_pool* pool_;
    bool kripke_left;	            ///< The left automaton is a Kripke
  };


  /// \brief This class allows to do a synchronized product
  /// between two fast tgba
  ///
  /// Warning : left and right must have the same atomic proposition
  ///           dictionnary and the same acceptance dictionnary.
  ///           This is not a restriction since the two FastTgba can
  ///           share the same dictionnary even if they do not work
  ///           with the same set of variables.
  ///
  /// Remark :  in order to do parallel emptiness checks, this product
  ///           do not write in this two dicitonnaries
  ///
  class SPOT_API fasttgba_product : public fasttgba
  {
  public:
    // ------------------------------------------------------------
    // Fast tgba interface
    // ------------------------------------------------------------
    fasttgba_product(const fasttgba* left,
		     const fasttgba* right,
		     bool left_is_kripke = false);
    virtual ~fasttgba_product();

    virtual fasttgba_state* get_init_state() const;

    virtual fasttgba_succ_iterator*
    succ_iter(const fasttgba_state* local_state) const;

    virtual fasttgba_succ_iterator*
    swarm_succ_iter(const fasttgba_state* local_state, int seed) const;

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

  protected:
    const fasttgba* left_;	///< The left aut. of the product
    const fasttgba* right_;	///< The right aut. of the product
    mutable fixed_size_pool pool_;
    bool kripke_left;		///< The left automaton is a Kripke
  };
}

#endif // SPOT_FASTTGBA_FASTTGBA_PRODUCT_HH
