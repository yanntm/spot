// -*- coding: utf-8 -*-
// Copyright (C) 2015-2019 Laboratoire de Recherche et
// Développement de l'Epita.
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
#include <algorithm>
#include <deque>
#include <stack>
#include <utility>
#include <unordered_map>
#include <set>
#include <map>

#include <spot/misc/bddlt.hh>
#include <spot/twaalgos/sccinfo.hh>
#include <spot/twaalgos/determinize2.hh>
#include <spot/twaalgos/degen.hh>
#include <spot/twaalgos/sccfilter.hh>
#include <spot/twaalgos/simulation.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/parity.hh>
#include <spot/priv/robin_hood.hh>

namespace spot
{
  namespace
  {
    // forward declaration
    struct safra_build;
    class compute_succs;

    class safra_state final
    {
    public:
      // a helper method to check invariants
      void
      check() const
      {
        // do not refer to braces that do not exist
        for (const auto& p : nodes_)
          if (p.second >= 0)
            if (((unsigned)p.second) >= braces_.size())
              assert(false);

        // braces_ describes the parenthood relation, -1 meaning toplevel
        // so braces_[b] < b always, and -1 is the only negative number allowed
        for (int b : braces_)
          {
            if (b < 0 && b != -1)
              assert(false);
            if (b >= 0 && braces_[b] > b)
              assert(false);
          }

        // no unused braces
        std::set<int> used_braces;
        for (const auto& n : nodes_)
          {
            int b = n.second;
            while (b >= 0)
              {
                used_braces.insert(b);
                b = braces_[b];
              }
          }
        assert(used_braces.size() == braces_.size());
      }

    public:
      using state_t = unsigned;
      using safra_node_t = std::pair<state_t, std::vector<int>>;

      bool operator==(const safra_state&) const;
      size_t hash() const;

      // Print the number of states in each brace
      // default constructor
      safra_state();
      safra_state(state_t state_number, bool acceptance_scc = false);
      safra_state(const safra_build& s, const compute_succs& cs, unsigned& color);
      // Compute successor for transition ap
      safra_state
      compute_succ(const compute_succs& cs, const bdd& ap, unsigned& color) const;
      unsigned
      finalize_construction(const std::vector<int>& buildbraces,
                            const compute_succs& cs);

      // each brace points to its parent.
      // braces_[i] is the parent of i
      // Note that braces_[i] < i, -1 stands for "no parent" (top-level)
      std::vector<int> braces_;
      std::vector<std::pair<state_t, int>> nodes_;
    };

    struct hash_safra
    {
      size_t
      operator()(const safra_state& s) const noexcept
      {
        return s.hash();
      }
    };

    template<class T>
    struct ref_wrap_equal
    {
      bool
      operator()(const std::reference_wrapper<T>& x,
                 const std::reference_wrapper<T>& y) const
      {
        return std::equal_to<T>()(x.get(), y.get());
      }
    };

    using power_set =
      robin_hood::unordered_node_map<safra_state, unsigned, hash_safra>;

    std::string
    nodes_to_string(const const_twa_graph_ptr& aut,
                    const safra_state& states);

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

    };

    // Given a certain transition_label, compute all the successors of a
    // safra_state under that label, and return the new nodes in res.
    class compute_succs final
    {
      friend class spot::safra_state;

      const safra_state* src;
      const std::vector<bdd>* all_bdds;
      const const_twa_graph_ptr& aut;

      // work vectors for safra_state::finalize_construction()
      mutable std::vector<char> empty_green;
      mutable std::vector<int> highest_green_ancestor;
      mutable std::vector<unsigned> decr_by;
      mutable safra_build ss;

    public:
      compute_succs(const const_twa_graph_ptr& aut)
      : src(nullptr)
      , all_bdds(nullptr)
      , aut(aut)
      {}

      void
      set(const safra_state& s, const std::vector<bdd>& v)
      {
        src = &s;
        all_bdds = &v;
      }

      struct iterator
      {
        const compute_succs& cs_;
        std::vector<bdd>::const_iterator bddit;
        safra_state ss;
        unsigned color_;

        iterator(const compute_succs& c, std::vector<bdd>::const_iterator it)
        : cs_(c)
        , bddit(it)
        {
          compute_();
        }

        bool
        operator!=(const iterator& other) const
        {
          return bddit != other.bddit;
        }

        iterator&
        operator++()
        {
          ++bddit;
          compute_();
          return *this;
        }
        // no need to implement postfix increment

        const bdd&
        cond() const
        {
          return *bddit;
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
          if (bddit == cs_.all_bdds->end())
            return;

          const bdd& ap = *bddit;

          ss = cs_.src->compute_succ(cs_, ap, color_);
        }
      };

      iterator
      begin() const
      {
        return iterator(*this, all_bdds->begin());
      }
      iterator
      end() const
      {
        return iterator(*this, all_bdds->end());
      }
    };

    const char* const sub[10] =
      {
        "\u2080",
        "\u2081",
        "\u2082",
        "\u2083",
        "\u2084",
        "\u2085",
        "\u2086",
        "\u2087",
        "\u2088",
        "\u2089",
      };

    std::string subscript(unsigned start)
    {
      std::string res;
      do
        {
          res = sub[start % 10] + res;
          start /= 10;
        }
      while (start);
      return res;
    }

    struct compare
    {
      bool
      operator() (const safra_state::safra_node_t& lhs,
                  const safra_state::safra_node_t& rhs) const
      {
        return lhs.second < rhs.second;
      }
    };

    // Return the nodes sorted in ascending order
    std::vector<safra_state::safra_node_t>
    sorted_nodes(const safra_state& s)
    {
      std::vector<safra_state::safra_node_t> res;
      for (const auto& n: s.nodes_)
        {
          int brace = n.second;
          std::vector<int> tmp;
          while (brace >= 0)
            {
              // FIXME is not there a smarter way?
              tmp.insert(tmp.begin(), brace);
              brace = s.braces_[brace];
            }
          res.emplace_back(n.first, std::move(tmp));
        }
      std::sort(res.begin(), res.end(), compare());
      return res;
    }

    std::string
    nodes_to_string(const const_twa_graph_ptr& aut,
                    const safra_state& states)
    {
      auto copy = sorted_nodes(states);
      std::ostringstream os;
      std::stack<int> s;
      bool first = true;
      for (const auto& n: copy)
        {
          auto it = n.second.begin();
          // Find brace on top of stack in vector
          // If brace is not present, then we close it as no other ones of that
          // type will be found since we ordered our vector
          while (!s.empty())
            {
              it = std::lower_bound(n.second.begin(), n.second.end(),
                                    s.top());
              if (it == n.second.end() || *it != s.top())
                {
                  os << subscript(s.top()) << '}';
                  s.pop();
                }
              else
                {
                  if (*it == s.top())
                    ++it;
                  break;
                }
            }
          // Add new braces
          while (it != n.second.end())
            {
              os << '{' << subscript(*it);
              s.push(*it);
              ++it;
              first = true;
            }
          if (!first)
            os << ' ';
          os << aut->format_state(n.first);
          first = false;
        }
      // Finish unwinding stack to print last braces
      while (!s.empty())
        {
          os << subscript(s.top()) << '}';
          s.pop();
        }
      return os.str();
    }

    std::vector<std::string>*
    print_debug(const const_twa_graph_ptr& aut,
                const power_set& states)
    {
      auto res = new std::vector<std::string>(states.size());
      for (const auto& p: states)
        (*res)[p.second] = nodes_to_string(aut, p.first);
      return res;
    }

    // Compute a vector of letters from a given support
    std::vector<bdd>
    letters(const bdd& allap)
    {
      std::vector<bdd> res;
      bdd all = bddtrue;
      while (all != bddfalse)
        {
          bdd one = bdd_satoneset(all, allap, bddfalse);
          all -= one;
          res.emplace_back(one);
        }
      return res;
    }

    class safra_support
    {
      const std::vector<bdd>& state_supports;
      robin_hood::unordered_flat_map<bdd, std::vector<bdd>, bdd_hash> cache;

    public:
      safra_support(const std::vector<bdd>& s): state_supports(s) {}

      const std::vector<bdd>&
      get(const safra_state& s)
      {
        bdd supp = bddtrue;
        for (const auto& n : s.nodes_)
          supp &= state_supports[n.first];
        auto i = cache.emplace(supp, std::vector<bdd>());
        if (i.second) // insertion took place
          i.first->second = letters(supp);
        return i.first->second;
      }
    };
  }

  safra_state
  safra_state::compute_succ(const compute_succs& cs,
                            const bdd& ap, unsigned& color) const
  {
    safra_build& ss = cs.ss;
    ss.braces_ = braces_; // copy
    ss.nodes_.clear();
    for (const auto& node: nodes_)
      {
        for (const auto& t: cs.aut->out(node.first))
          {
            if (!bdd_implies(ap, t.cond))
              continue;

            ss.update_succ(node.second, t.dst, t.acc);
          }
      }
    return safra_state(ss, cs, color);
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

  safra_state::safra_state()
  : nodes_{std::make_pair(0, -1)}
  {}

  // Called only to initialize first state
  safra_state::safra_state(state_t val, bool accepting_scc)
  : nodes_{std::make_pair(val, -1)}
  {
    if (accepting_scc)
      {
        braces_.emplace_back(-1);
        nodes_.back().second = 0;
      }
  }

  safra_state::safra_state(const safra_build& s,
                           const compute_succs& cs,
                           unsigned& color)
  : nodes_(s.nodes_.begin(), s.nodes_.end())
  {
    color = finalize_construction(s.braces_, cs);
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

  bool
  safra_state::operator==(const safra_state& other) const
  {
    return nodes_ == other.nodes_ && braces_ == other.braces_;
  }

  twa_graph_ptr
  tgba_determinize2(const const_twa_graph_ptr& a,
                    bool pretty_print,
                    const output_aborter* aborter)
  {
    if (!a->is_existential())
      throw std::runtime_error
        ("tgba_determinize() does not support alternation");
    if (is_universal(a))
      return std::const_pointer_cast<twa_graph>(a);

    // Degeneralize
    const_twa_graph_ptr aut;
    std::vector<bdd> implications;
    {
      twa_graph_ptr aut_tmp = spot::degeneralize_tba(a);
      if (pretty_print)
        aut_tmp->copy_state_names_from(a);
      aut = aut_tmp;
    }
    scc_info_options scc_opt = scc_info_options::TRACK_SUCCS;
    scc_info scc = scc_info(aut, scc_opt);


    // Compute the support of each state
    std::vector<bdd> support(aut->num_states());

    for (unsigned i = 0; i != aut->num_states(); ++i)
      {
        bdd res = bddtrue;
        for (const auto& e : aut->out(i))
          res &= bdd_support(e.cond);
        support[i] = res;
      }


    safra_support safra2letters(support);

    auto res = make_twa_graph(aut->get_dict());
    res->copy_ap_of(aut);
    res->prop_copy(aut,
                   { false, // state based
                       false, // inherently_weak
                       false, false, // deterministic
                       true, // complete
                       true // stutter inv
                       });

    // Given a safra_state get its associated state in output automata.
    // Required to create new edges from 2 safra-state
    power_set seen;
    // As per the standard, references to elements in a std::unordered_set or
    // std::unordered_map are invalidated by erasure only.
    std::deque<std::reference_wrapper<power_set::value_type>> todo;
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

    {
      unsigned init_state = aut->get_init_state_number();
      bool start_accepting = true; // always true without use_scc opti
      safra_state init(init_state, start_accepting);
      unsigned num = get_state(init); // inserts both in seen and in todo
      res->set_init_state(num);
    }
    unsigned sets = 0;

    compute_succs succs(aut);
    // The main loop
    while (!todo.empty())
      {
        if (aborter && aborter->too_large(res))
          return nullptr;
        const safra_state& curr = todo.front().get().first;
        unsigned src_num = todo.front().get().second;
        todo.pop_front();
        succs.set(curr, safra2letters.get(curr));
        for (auto s = succs.begin(); s != succs.end(); ++s)
          {
            // Don't construct sink state as complete does a better job at this
            if (s->nodes_.empty())
              continue;
            unsigned dst_num = get_state(*s);
            if (s.color_ != -1U)
              {
                res->new_edge(src_num, dst_num, s.cond(), {s.color_});
                sets = std::max(s.color_ + 1, sets);
              }
            else
              res->new_edge(src_num, dst_num, s.cond());
          }
      }
    // Green and red colors work in pairs, so the number of parity conditions is
    // necessarily even.
    sets += sets & 1;
    // Acceptance is now min(odd) since we can emit Red on paths 0 with new opti
    res->set_acceptance(sets, acc_cond::acc_code::parity_min_odd(sets));
    res->prop_universal(true);
    res->prop_state_acc(false);

    cleanup_parity_here(res);

    if (pretty_print)
      res->set_named_prop("state-names", print_debug(aut, seen));
    return res;
  }
}
