// -*- coding: utf-8 -*-
// Copyright (C) 2015, 2016, 2018 Laboratoire de Recherche et Développement
// de l'Epita.
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

#pragma once

#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/sccinfo.hh>
#include <spot/misc/bddlt.hh>

namespace spot
{
  namespace
  {
    // forward declarations
    struct safra_build;
    class compute_succs;
  }

  class SPOT_API safra_state final
  {
#ifndef SWIG
  public:
    // a helper method to check invariants
    void check() const;

    using state_t = unsigned;
    using safra_node_t = std::pair<state_t, std::vector<int>>;

    bool operator<(const safra_state&) const;
    bool operator==(const safra_state&) const;
    size_t hash() const;

    // default constructor
    safra_state();
    safra_state(state_t state_number, bool acceptance_scc = false);
    safra_state(const safra_build& s, const compute_succs& cs, unsigned& color);
    // Compute successor for transition ap
    safra_state
    compute_succ(const compute_succs& cs, const bdd& ap, unsigned& color) const;
    void
    merge_redundant_states(const std::vector<std::vector<char>>& implies);
    unsigned
    finalize_construction(const std::vector<int>& buildbraces,
                          const compute_succs& cs);

    bool contains(state_t s) const;

    // each brace points to its parent.
    // braces_[i] is the parent of i
    // Note that braces_[i] < i, -1 stands for "no parent" (top-level)
    std::vector<int> braces_;
    std::vector<std::pair<state_t, int>> nodes_;
#endif
  };

  struct hash_safra
  {
    size_t
    operator()(const safra_state& s) const noexcept
    {
      return s.hash();
    }
  };

  class SPOT_API safra_support
  {
    std::vector<bdd> state_supports;
    std::unordered_map<bdd, std::vector<bdd>, bdd_hash> cache;
  public:
    explicit safra_support(const const_twa_graph_ptr& a,
                           bool use_stutter,
                           const scc_info& scc);

    const std::vector<bdd>& get(const safra_state& s);
  };

  using safra_map = std::unordered_map<safra_state, unsigned, hash_safra>;

  class SPOT_API determinizer
  {
  private:
    explicit determinizer(const const_twa_graph_ptr& aut,
                          const std::vector<bdd>& implications,
                          bool pretty_print,
                          bool use_scc,
                          bool use_simulation,
                          bool use_stutter);
  public:
    // factory method
    static determinizer build(const const_twa_graph_ptr& aut,
                              bool pretty_print,
                              bool use_scc,
                              bool use_simulation,
                              bool use_stutter);

    // activate an unactive state
    void activate(unsigned);
    void deactivate_all();

    // run the determinization
    void run();
    // retrieve the current deterministic automaton
    twa_graph_ptr get() const;
    // retrieve the base automaton
    const_twa_graph_ptr aut() const { return aut_; }

    // retrieve a safra_state from a state number in res_
    // throw an exception if no corresponding state is found
    const safra_state& get_safra(unsigned) const;

  private:
    const const_twa_graph_ptr aut_;
    twa_graph_ptr res_;
    const scc_info scc_;
    std::vector<std::vector<char>> implies_;
    safra_support safra2letters_;

    std::vector<char> toadd_;
    std::vector<char> active_;
    std::set<unsigned> nondet_states_;
    safra_map seen_;
    std::vector<safra_state> num2safra_;
    unsigned sets_;

    bool pretty_print_;
    bool use_scc_;
    bool use_simulation_;
    bool use_stutter_;

    void reset_res();
  };

  /// \ingroup twa_algorithms
  /// \brief Determinize a TGBA
  ///
  /// The main algorithm works only with automata using Büchi acceptance
  /// (preferably transition-based).  If generalized Büchi is input, it
  /// will be automatically degeneralized first.
  ///
  /// The output will be a deterministic automaton using parity acceptance.
  ///
  /// This procedure is based on an algorithm by Roman Redziejowski
  /// (Fundamenta Informaticae 119, 3-4 (2012)).  Redziejowski's
  /// algorithm is similar to Piterman's improvement of Safra's
  /// algorithm, except it is presented on transition-based acceptance
  /// and use simpler notations.  We implement three additional
  /// optimizations (they can be individually disabled) based on
  ///
  ///   - knowledge about SCCs of the input automaton
  ///   - knowledge about simulation relations in the input automaton
  ///   - knowledge about stutter-invariance of the input automaton
  ///
  /// The last optimization is an idea described by Joachim Klein &
  /// Christel Baier (CIAA'07) and implemented in ltl2dstar.  In fact,
  /// ltl2dstar even has a finer version (letter-based stuttering)
  /// that is not implemented here.
  ///
  /// \param aut the automaton to determinize
  ///
  /// \param pretty_print whether to decorate states with names
  ///                     showing the paths they track (this only
  ///                     makes sense if the input has Büchi
  ///                     acceptance already, otherwise the input
  ///                     automaton will be degeneralized and the
  ///                     names will refer to the states in the
  ///                     degeneralized automaton).
  ///
  /// \param use_scc whether to simplify the construction based on
  ///                the SCCs in the input automaton.
  ///
  /// \param use_simulation whether to simplify the construction based
  ///                       on simulation relations between states in
  ///                       the original automaton.
  ///
  /// \param use_stutter whether to simplify the construction when the
  ///                    input automaton is known to be
  ///                    stutter-invariant.  (The stutter-invariant
  ///                    flag of the input automaton is used, so it
  ///                    might be worth to call
  ///                    spot::check_stutter_invariance() first if
  ///                    possible.)
  SPOT_API twa_graph_ptr
  tgba_determinize(const const_twa_graph_ptr& aut,
                   bool pretty_print = false,
                   bool use_scc = true,
                   bool use_simulation = true,
                   bool use_stutter = true);

  SPOT_API twa_graph_ptr
  tgba_determinize_incr(const const_twa_graph_ptr& aut,
                        bool pretty_print = false,
                        bool use_scc = true,
                        bool use_simulation = true,
                        bool use_stutter = true);
}
