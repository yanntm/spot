// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et Développement de l'Epita
// (LRDE).
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 3 of the License, or (at your option) any later
// version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <set>
#include <spot/algebra/languagewilke.hh>
#include <spot/algebra/wilkecanon.hh>
#include <spot/misc/bddlt.hh>
#include <spot/twa/twa.hh>

namespace spot
{

  class acc_matrix
  {
  public:
    /// a helper typedef
    // TODO this is probably not the most efficient data structure to use...
    using mark_set = std::set<acc_cond::mark_t>;

    /// constructor
    explicit acc_matrix(unsigned n);
    /// default constructor
    acc_matrix(): acc_matrix(0) {}

    /// Accessors.
    mark_set& operator()(unsigned i, unsigned j);
    const mark_set& operator()(unsigned i, unsigned j) const;

    /// Matrix multiplication
    static acc_matrix product(const acc_matrix&, const acc_matrix&);

    /// Administrative trivia.
    unsigned size() const { return size_; }
    size_t hash() const;
    bool operator==(const acc_matrix&) const;
    bool operator!=(const acc_matrix& o) const { return !operator==(o); }
    bool operator<(const acc_matrix& o) const { return matrix_ < o.matrix_; }

  private:
    // for each pair of states of the automaton, indicate the sets of acceptance
    // marks seen along the paths from p to q.
    std::vector<mark_set> matrix_;
    unsigned size_;
  };

  template<>
  struct hash<bdd> : public bdd_hash {};
  template<>
  struct less_than<bdd> : public bdd_less_than {};

  /// \brief A class to represent the Wilke algebra of a twa.
  ///
  /// This works only on twa_graph in fact.
  /// This is only valid on fin-less (i.e. monotonic) acceptance conditions.
  class SPOT_API twa_wilke : public smart_language<smart_canon<acc_matrix, bdd>>
  {
    const_twa_graph_ptr aut_;

  public:
    /// forwarding constructor
    /// users should use the factory method below rather than this constructor.
    template<class ... Args>
    explicit twa_wilke(const const_twa_graph_ptr& a, Args&&... args)
    : smart_language<smart_canon<acc_matrix, bdd>>(std::forward<Args>(args)...)
    , aut_(a)
    {}

    void print_morphism(std::ostream& os) const;
    void print(std::ostream& os) const override;

    /// A factory method.
    static
    std::shared_ptr<twa_wilke>
    build(const const_twa_graph_ptr& aut);
  };

  /// \brief Computes the TGBA associated to a semigroup.
  ///
  /// Currently not implemented.
  SPOT_API
  twa_ptr
  semigroup_to_aut(const std::shared_ptr<const twa_wilke>&);

  /// \brief Computes the Wilke algebra of an automaton.
  ///
  /// Currently only implemented for twa_graph with Fin-less acceptance
  /// conditions.
  SPOT_API
  std::shared_ptr<twa_wilke>
  aut_to_semigroup(const const_twa_ptr&);

  /// \brief Computes the generators of the Wilke algebra of an automaton.
  ///
  /// Currently only implemented for twa_graph with Fin-less acceptance
  /// conditions.
  SPOT_API
  morphism_t<bdd, acc_matrix>
  aut_morphism(const const_twa_graph_ptr&);

  // TODO add a method to obtain the syntactic semigroup of an automaton.

  SPOT_API
  std::ostream&
  print_monoid(std::ostream& os,
               const const_twa_ptr& a,
               const char* options = nullptr);

}
