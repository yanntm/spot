// -*- coding: utf-8 -*-
// Copyright (C) 2020 Laboratoire de Recherche et Developpement de
// l'Epita (LRDE).
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

#include "config.h"

#include <deque>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <spot/twacube_algos/determinize.hh>
#include <bddx.h>

namespace spot
{
  namespace
  {
    // TODO: remove this when multithreaded
    const unsigned THREAD_ID = 0;

    // forward declarations
    class compute_succs;
    struct safra_build;

    struct safra_state final
    {
      // each brace points to its parent.
      // braces_[i] is the parent of i
      // Note that braces_[i] < i, -1 stands for "no parent" (top-level)
      std::vector<int> braces_;

      using state_t = unsigned;
      // index of the node, and index of its enclosing brace, if any (else -1)
      std::vector<std::pair<state_t, int>> nodes_;

      safra_state();
      safra_state(state_t state_number);
      safra_state(const safra_build& s, const compute_succs& cs, unsigned& color);

      safra_state
      compute_succ(const compute_succs& cs, const cube& ap, unsigned& color) const;
      unsigned
      finalize_construction(const std::vector<int>& buildbraces,
                            const compute_succs& cs);

      size_t hash() const;
      bool operator==(const safra_state&) const;
    }; // struct safra_state

    // Returns true if lhs has a smaller nesting pattern than rhs
    // If lhs and rhs are the same, return false.
    // NB the nesting patterns are backwards.
    bool nesting_cmp(const std::vector<int>& lhs,
                     const std::vector<int>& rhs)
    {
      unsigned m = std::min(lhs.size(), rhs.size());
      auto lit = lhs.rbegin();
      auto rit = rhs.rbegin();
      for (unsigned i = 0; i != m; ++i)
        {
          if (*lit != *rit)
            return *lit < *rit;
        }
      return lhs.size() > rhs.size();
    }

    // a helper class for building the successor of a safra_state
    struct safra_build final
    {
      std::vector<int> braces_;
      std::map<unsigned, int> nodes_;

      bool
      compare_braces(int a, int b)
      {
        std::vector<int> a_pattern;
        std::vector<int> b_pattern;
        a_pattern.reserve(a+1);
        b_pattern.reserve(b+1);
        while (a != b)
          {
            if (a > b)
              {
                a_pattern.emplace_back(a);
                a = braces_[a];
              }
            else
              {
                b_pattern.emplace_back(b);
                b = braces_[b];
              }
          }
        return nesting_cmp(a_pattern, b_pattern);
      }

      // Used when creating the list of successors
      // A new intermediate node is created with src's braces and with dst as id
      // A merge is done if dst already existed in *this
      void
      update_succ(int brace, unsigned dst, const acc_cond::mark_t& acc)
      {
        int newb = brace;
        if (acc)
          {
            assert(acc.has(0) && acc.count() == 1 && "Only TBA are accepted");
            // Accepting edges generate new braces: step A1
            newb = braces_.size();
            braces_.emplace_back(brace);
          }
        auto i = nodes_.emplace(dst, newb);
        if (!i.second) // dst already exists
          {
            // Step A2: Only keep the smallest nesting pattern.
            // Use nesting_cmp to compare nesting patterns.
            if (compare_braces(newb, i.first->second))
              {
                i.first->second = newb;
              }
            else
              {
                if (newb != brace) // new brace was created but is not needed
                  braces_.pop_back();
              }
          }
      }

      // Same as above, specialized for brace == -1
      // Acceptance parameter is passed as a template parameter to improve
      // performance.
      // If a node for dst already existed, the newly inserted node has smaller
      // nesting pattern iff is_acc == true AND nodes_[dst] == -1
      template<bool is_acc>
      void
      update_succ_toplevel(unsigned dst)
      {
        if (is_acc)
          {
            // Accepting edges generate new braces: step A1
            int newb = braces_.size();
            auto i = nodes_.emplace(dst, newb);
            if (i.second || i.first->second == -1)
              {
                braces_.emplace_back(-1);
                i.first->second = newb;
              }
          }
        else
          {
            nodes_.emplace(dst, -1);
          }
      }

    }; // safra_build

    // Given a certain transition_label, compute all the successors of a
    // safra_state under that label, and return the new nodes in res.
    class compute_succs final
    {
      friend class spot::safra_state;

      const safra_state* src;
      const std::vector<cube>* all_cubes;
      const const_twacube_ptr& aut;

      // work vectors for safra_state::finalize_construction()
      mutable std::vector<char> empty_green;
      mutable std::vector<int> highest_green_ancestor;
      mutable std::vector<unsigned> decr_by;
      mutable safra_build ss;

    public:
      compute_succs(const const_twacube_ptr& aut)
        : src(nullptr)
        , all_cubes(nullptr)
        , aut(aut)
      {}

      void
      set(const safra_state& s, const std::vector<cube>& v)
      {
        src = &s;
        all_cubes = &v;
      }

      struct iterator
      {
        const compute_succs& cs_;
        std::vector<cube>::const_iterator cubeit;
        safra_state ss;
        unsigned color_;

        iterator(const compute_succs& c, std::vector<cube>::const_iterator it)
          : cs_(c)
          , cubeit(it)
        {
          compute_();
        }

        bool
        operator!=(const iterator& other) const
        {
          return cubeit != other.cubeit;
        }

        iterator&
        operator++()
        {
          ++cubeit;
          compute_();
          return *this;
        }
        // no need to implement postfix increment

        const cube&
        cond() const
        {
          return *cubeit;
        }

        const safra_state&
        operator*() const
        {
          return ss;
        }
        const safra_state*
        operator->() const
        {
          return &ss;
        }

      private:
        std::vector<safra_state> stutter_path_;

        void
        compute_()
        {
          if (cubeit == cs_.all_cubes->end())
            return;

          const cube& ap = *cubeit;

          ss = cs_.src->compute_succ(cs_, ap, color_);
        }
      };

      iterator
      begin() const
      {
        return iterator(*this, all_cubes->begin());
      }
      iterator
      end() const
      {
        return iterator(*this, all_cubes->end());
      }
    }; // compute_succs

    safra_state::safra_state()
      : nodes_{std::make_pair(0, -1)}
    {}

    safra_state::safra_state(state_t state_number)
      : nodes_{std::make_pair(state_number, -1)}
    {
      braces_.emplace_back(-1);
      nodes_.back().second = 0;
    }

    safra_state::safra_state(const safra_build& s,
                             const compute_succs& cs,
                             unsigned& color)
      : nodes_(s.nodes_.begin(), s.nodes_.end())
    {
      color = finalize_construction(s.braces_, cs);
    }

    // Return the emitted color, red or green
    unsigned
    safra_state::finalize_construction(const std::vector<int>& buildbraces,
                                       const compute_succs& cs)
    {
      unsigned red = -1U;
      unsigned green = -1U;
      // use std::vector<char> to avoid std::vector<bool>
      // a char encodes several bools:
      //  * first bit says whether the brace is empty and red
      //  * second bit says whether the brace is green
      // brackets removed from green pairs can be safely be marked as red,
      // because their enclosing green has a lower number
      // beware of pairs marked both as red and green: they are actually empty
      constexpr char is_empty = 1;
      constexpr char is_green = 2;
      cs.empty_green.assign(buildbraces.size(), is_empty | is_green);

      for (const auto& n : nodes_)
        if (n.second >= 0)
          {
            int brace = n.second;
            // Step A4: For a brace to be green it must not contain states
            // on its own.
            cs.empty_green[brace] &= ~is_green;
            while (brace >= 0 && (cs.empty_green[brace] & is_empty))
              {
                cs.empty_green[brace] &= ~is_empty;
                brace = buildbraces[brace];
              }
          }

      // Step A4 Remove brackets within green pairs
      // for each bracket, find its highest green ancestor
      // 0 cannot be in a green pair, its highest green ancestor is itself
      // Also find red and green signals to emit
      // And compute the number of braces to remove for renumbering
      cs.highest_green_ancestor.assign(buildbraces.size(), 0);
      cs.decr_by.assign(buildbraces.size(), 0);
      unsigned decr = 0;
      for (unsigned b = 0; b != buildbraces.size(); ++b)
        {
          cs.highest_green_ancestor[b] = b;
          const int& ancestor = buildbraces[b];
          // Note that ancestor < b
          if (ancestor >= 0
              && (cs.highest_green_ancestor[ancestor] != ancestor
                  || (cs.empty_green[ancestor] & is_green)))
            {
              cs.highest_green_ancestor[b] = cs.highest_green_ancestor[ancestor];
              cs.empty_green[b] |= is_empty; // mark brace for removal
            }

          if (cs.empty_green[b] & is_empty)
            {
              // Step A5 renumber braces
              ++decr;

              // Step A3 emit red
              red = std::min(red, 2*b);
            }
          else if (cs.empty_green[b] & is_green)
            {
              // Step A4 emit green
              green = std::min(green, 2*b+1);
            }

          cs.decr_by[b] = decr;
        }

      // Update nodes with new braces numbers
      braces_ = std::vector<int>(buildbraces.size() - decr, -1);
      for (auto& n : nodes_)
        {
          if (n.second >= 0)
            {
              unsigned i = cs.highest_green_ancestor[n.second];
              int j = buildbraces[i] >=0
                ? buildbraces[i] - cs.decr_by[buildbraces[i]]
                : -1;
              n.second = i - cs.decr_by[i];
              braces_[n.second] = j;
            }
        }

      return std::min(red, green);
    }

    safra_state
    safra_state::compute_succ(const compute_succs& cs,
                              const cube& ap, unsigned& color) const
    {
      safra_build& ss = cs.ss;
      ss.braces_ = braces_; // copy
      ss.nodes_.clear();
      for (const auto& node: nodes_)
        {
          // get successors of node
          auto [state_num, _] = node;
          const auto succs = cs.aut->succ(state_num);
          while (!succs->done())
            {
              const auto& trans_data = cs.aut->trans_data(succs, THREAD_ID);
              const auto& trans_storage = cs.aut->trans_storage(succs, THREAD_ID);
              // skip if transition isn't accessible with this ap
              if (!cs.aut->get_cubeset()
                  .intersect(trans_data.cube_, ap))
                continue;

              ss.update_succ(node.second, trans_storage.dst, trans_data.acc_);
            }
        }
      return safra_state(ss, cs, color);
    }

    bool
    safra_state::operator==(const safra_state& other) const
    {
      return nodes_ == other.nodes_ && braces_ == other.braces_;
    }

    size_t
    safra_state::hash() const
    {
      size_t res = 0;
      //std::cerr << this << " [";
      for (const auto& p : nodes_)
        {
          res ^= (res << 3) ^ p.first;
          res ^= (res << 3) ^ p.second;
          //  std::cerr << '(' << p.first << ',' << p.second << ')';
        }
      //    std::cerr << "][ ";
      for (const auto& b : braces_)
        {
          res ^= (res << 3) ^ b;
          //  std::cerr << b << ' ';
        }
      //    std::cerr << "]: " << std::hex << res << std::dec << '\n';
      return res;
    }

    struct hash_safra
    {
      size_t
      operator()(const safra_state& s) const noexcept
      {
        return s.hash();
      }
    };
  }

  // set containing indices of atomic propositions included in the support
  using cube_support = std::unordered_set<size_t>;

  /// \brief finds all the variables that \a c depends on.
  cube_support
  compute_cube_support(const cube& c, const cubeset& cs)
  {
    std::unordered_set<size_t> res;

    for (size_t i = 0; i < cs.size(); ++i)
      {
        // if is free var
        if (cs.is_true_var(c, i) || cs.is_false_var(c, i))
          res.insert(i);
      }

    return res;
  }

  std::vector<cube>
  permutations(cube_support support_set, const cubeset& cs)
  {
    // switch set to vector, easier
    std::vector<size_t> support(support_set.begin(), support_set.end());

    return cs.permutations(support);
  }

  // for a given powerset state, return all letters (atomic propositions) that
  // are involved in a transition from this state
  std::vector<cube>
  get_letters(const safra_state& s, const std::vector<cube_support>& supports, const cubeset& cs)
  {
    cube_support safra_support;

    // for each "sub state"
    for (const auto& [state, _] : s.nodes_)
      {
        cube_support state_support = supports[state];
        // compute the union of their support
        safra_support.insert(state_support.begin(), state_support.end());
      }


    return permutations(safra_support, cs);
  }

  twacube_ptr
  twacube_determinize(const twacube_ptr aut)
  {
    // TODO(am): check is_existential + is_universal before launching useless
    // computation

    // TODO(am): degeneralize ? might need to filter out TGBAs for now

    auto res = make_twacube(aut->get_ap());

    const cubeset& cs = aut->get_cubeset();

    // computing each state's support, i.e. all variables upon which the
    // transition taken depends
    std::vector<cube_support> supports(aut->num_states());
    for (unsigned i = 0; i != aut->num_states(); ++i)
      {
        // alloc() returns a cube with all variables marked free
        cube_support res;

        const auto succs = aut->succ(i);
        while (!succs->done())
          {
            auto& trans = aut->trans_data(succs, THREAD_ID);
            cube_support support = compute_cube_support(trans.cube_, cs);

            // union of support and res
            res.insert(support.begin(), support.end());
          }

        supports[i] = res;
      }

    // association between safra_state and destination state in resulting
    // automaton
    std::unordered_map<safra_state, unsigned, hash_safra> seen;

    // a safra state and its corresponding state id in the resulting automaton
    std::deque<std::pair<safra_state, unsigned>> todo;

    // find association between safra state and res state, or create one
    auto get_state = [&res, &seen, &todo](const safra_state& s) -> unsigned
    {
      auto it = seen.find(s);
      if (it == seen.end())
        {
          unsigned dst_num = res->new_state();
          it = seen.emplace(s, dst_num).first;
          todo.emplace_back(*it);
        }
      return it->second;
    };

    // initial state creation
    {
      unsigned init_state = aut->get_initial();
      safra_state init(init_state);
      unsigned res_init = get_state(init);
      res->set_initial(res_init);
    }
    unsigned sets = 0;

    compute_succs succs(aut);

    // core algorithm
    //
    // for every safra state,
    //     for each possible safra successor,
    //         compute successor emitted color
    //         create transition in res automaton, with color
    while (!todo.empty())
      {
        const safra_state curr = todo.front().first;
        const unsigned src_num = todo.front().second;
        todo.pop_front();

        succs.set(curr, get_letters(curr, supports, cs));

        // iterator over successors of curr
        for (auto s = succs.begin(); s != succs.end(); ++s)
          {
            if (s->nodes_.empty())
              continue;

            unsigned dst_num = get_state(*s);
            if (s.color_ != -1U)
              {
                res->create_transition(src_num, s.cond(), { s.color_ }, dst_num);
              }
            else
              {
                res->create_transition(src_num, s.cond(), {} , dst_num);
                sets = std::max(s.color_ + 1, sets);
              }
          }
      }

    // Green and red colors work in pairs, so the number of parity conditions is
    // necessarily even.
    sets += sets & 1;

    // TODO: number of sets? in twa/twa.hh
    // res->set_acceptance(sets, acc_cond::acc_code::parity_min_odd(sets));
    res->acc() = acc_cond::acc_code::parity_min_odd(sets);

    // TODO: set these properties when twacube supports them
    // res->prop_universal(true);
    // res->prop_state_acc(false);

    // TODO: optimization not implemented on twacube
    // cleanup_parity_here(res);

    return res;
  }
}
