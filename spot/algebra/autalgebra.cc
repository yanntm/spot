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

#include <spot/algebra/autalgebra.hh>

#include <spot/twa/twagraph.hh>
#include <spot/misc/bddlt.hh>
#include <spot/misc/minato.hh>
#include <spot/twa/bddprint.hh>

namespace spot
{

  acc_matrix::acc_matrix(unsigned n)
  : matrix_(n*n)
  , size_(n)
  {}

  size_t
  acc_matrix::hash() const
  {
    size_t res = 0;
    auto h = std::hash<acc_cond::mark_t>();
    for (const auto& s : matrix_)
      for (const auto& m : s)
        res ^= h(m) + 0x9e3779b9 + (res << 6) + (res >> 2);
    return res;
  }

  bool
  acc_matrix::operator==(const acc_matrix& o) const
  {
    return matrix_ == o.matrix_;
  }

  acc_matrix::mark_set&
  acc_matrix::operator()(unsigned i, unsigned j)
  {
    assert(i < size_);
    assert(j < size_);
    return matrix_[i + j*size_];
  }

  const acc_matrix::mark_set&
  acc_matrix::operator()(unsigned i, unsigned j) const
  {
    assert(i < size_);
    assert(j < size_);
    return matrix_[i + j*size_];
  }

  // The product of two sets of acceptance conditions is the set of the
  // pair-wise union of its components.
  // E * F = { e \cup f | e \in E, f \in F }
  static
  acc_matrix::mark_set
  mark_set_prod(const acc_matrix::mark_set& x, const acc_matrix::mark_set& y)
  {
    acc_matrix::mark_set res;
    for (const auto& e : x)
      for (const auto& f : y)
        res.insert(e|f);
    return res;
  }
  // The sum of two sets of acceptance conditions is their union.
  // E + F = E \cup F

  static
  acc_matrix
  matrix_prod(const acc_matrix& lhs, const acc_matrix& rhs)
  {
    assert(lhs.size() == rhs.size());
    acc_matrix res(lhs.size());
    // this is a product of matrices, maybe it could be optimized
    for (unsigned i = 0; i != lhs.size(); ++i)
      for (unsigned j = 0; j != lhs.size(); ++j)
        for (unsigned k = 0; k != lhs.size(); ++k)
          {
            auto tmp = mark_set_prod(lhs(i, k), rhs(k, j));
            res(i, j).insert(tmp.begin(), tmp.end());
          }
    return res;
  }

  acc_matrix
  acc_matrix::product(const acc_matrix& lhs, const acc_matrix& rhs)
  {
    return matrix_prod(lhs, rhs);
  }

  static
  std::vector<bdd>
  bdd2letters(bdd b, const bdd& ap_vars, const bdd_dict_ptr&)
  {
    std::vector<bdd> letters;
    while (b != bddfalse)
      {
        bdd letter = bdd_satoneset(b, ap_vars, bddfalse);
        b -= letter;
        letters.emplace_back(letter);
      }

    return letters;
  }

  // class twa_wilke

  void
  twa_wilke::print_morphism(std::ostream& os) const
  {
    os << "morphism is\n";
    for (const auto& it : this->morphism_)
      {
        bdd_format_formula(aut_->get_dict(), it.first);
        os << bdd_format_formula(aut_->get_dict(), it.first) << " --> "
          << generators()[it.second] << '\n';
      }
    os << std::endl;
  }

  void
  twa_wilke::print(std::ostream& os) const
  {
    print_morphism(os);
    smart_language<smart_canon<acc_matrix, bdd>>::print(os);
  }

  morphism_t<bdd, acc_matrix>
  aut_morphism(const const_twa_graph_ptr& aut)
  {
    if (aut->acc().uses_fin_acceptance())
      throw std::runtime_error("Wilke algebra requires Fin-less acceptance");

    morphism_t<bdd, acc_matrix> morphism;
    // walk all the transitions
    for (auto e : aut->edges())
      {
        for (const auto& letter : bdd2letters(e.data().cond,
                                              aut->ap_vars(),
                                              aut->get_dict()))
          {
            if (morphism.find(letter) == morphism.end())
              morphism[letter] = acc_matrix(aut->num_states());
            morphism[letter](e.src, e.dst).insert(e.data().acc);
          }
      }
    return morphism;
  }

  std::shared_ptr<twa_wilke>
  twa_wilke::build(const const_twa_graph_ptr& aut)
  {
    morphism_t<bdd, acc_matrix> morphism = aut_morphism(aut);

    // the matrix product
    auto prod = [](const acc_matrix& lhs, const acc_matrix& rhs)
      {
        return matrix_prod(lhs, rhs);
      };
    // method to find accepting elements
    auto acc = [&](const linked_pair<acc_matrix>& p)
      {
        acc_matrix s = p.prefix()[0];
        unsigned k = 1;
        for (; k != p.prefix().size(); ++k)
          s = prod(s, p.prefix()[k]);
        acc_matrix e = p.power()[0];
        k = 1;
        for (; k != p.power().size(); ++k)
          e = prod(e, p.power()[k]);

        unsigned q0 = aut->get_init_state_number();
        for (unsigned i = 0; i != s.size(); ++i)
          {
            if (s(q0, i).empty())
              continue;
            for (const auto& m : e(i, i))
              {
                if (aut->acc().accepting(m))
                  return true;
              }
          }
        return false;
      };
    return std::make_shared<twa_wilke>(aut, acc, morphism, prod);
  }

  std::shared_ptr<twa_wilke>
  aut_to_semigroup(const const_twa_ptr& aut)
  {
    const_twa_graph_ptr ag = std::dynamic_pointer_cast<const twa_graph>(aut);
    if (!ag)
      throw std::runtime_error("Computing the Wilke algebra is available for \
          twa_graph only.");

    return twa_wilke::build(ag);
  }

  std::ostream&
  print_monoid(std::ostream& os,
               const const_twa_ptr& a,
               const char*)
  {
    auto smart = aut_to_semigroup(a);
    smart->print(os);
    auto syn_wilke = smart->syntactic_wilke();
    syn_wilke.print(os);
    return os;
  }

}

