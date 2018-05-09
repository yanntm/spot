// -*- coding: utf-8 -*-
// Copyright (C) 2015-2018 Laboratoire de Recherche et
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

#include <spot/twaalgos/sccinfo.hh>
#include <spot/twaalgos/determinize.hh>
#include <spot/twaalgos/degen.hh>
#include <spot/twaalgos/sccfilter.hh>
#include <spot/twaalgos/simulation.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/parity.hh>
#include <spot/priv/allocator.hh>

namespace spot
{
  void
  safra_state::check() const
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

  bool
  safra_state::contains(safra_state::state_t s) const
  {
    for (const auto& p : nodes_)
      if (p.first == s)
        return true;
    return false;
  }

  namespace
  {
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

    using power_set = safra_map;

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
      std::map<unsigned, int,
               std::less<unsigned>,
               pool_allocator<std::pair<const unsigned, int>>> nodes_;

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
        if (acc.count())
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
      const power_set& seen;
      const scc_info& scc;
      const std::vector<std::vector<char>>& implies;
      const std::vector<char>& active;
      bool use_scc;
      bool use_simulation;
      bool use_stutter;

      // work vectors for safra_state::finalize_construction()
      mutable std::vector<char> empty_green;
      mutable std::vector<int> highest_green_ancestor;
      mutable std::vector<unsigned> decr_by;
      mutable safra_build ss;

    public:
      compute_succs(const const_twa_graph_ptr& aut,
                    const power_set& seen,
                    const scc_info& scc,
                    const std::vector<std::vector<char>>& implies,
                    const std::vector<char>& active,
                    bool use_scc,
                    bool use_simulation,
                    bool use_stutter)
      : src(nullptr)
      , all_bdds(nullptr)
      , aut(aut)
      , seen(seen)
      , scc(scc)
      , implies(implies)
      , active(active)
      , use_scc(use_scc)
      , use_simulation(use_simulation)
      , use_stutter(use_stutter)
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
        void
        compute_()
        {
          if (bddit == cs_.all_bdds->end())
            return;

          const bdd& ap = *bddit;

          if (cs_.use_stutter && cs_.aut->prop_stutter_invariant())
            {
              ss = *cs_.src;
              bool stop = false;
              std::deque<safra_state> path;
              std::unordered_set<
                std::reference_wrapper<const safra_state>,
                hash_safra,
                ref_wrap_equal<const safra_state>> states;
              unsigned mincolor = -1U;
              while (!stop)
                {
                  path.emplace_back(std::move(ss));
                  auto i = states.insert(path.back());
                  SPOT_ASSUME(i.second);
                  ss = path.back().compute_succ(cs_, ap, color_);
                  mincolor = std::min(color_, mincolor);
                  stop = states.find(ss) != states.end();
                }

              // also insert last element (/!\ it thus appears twice in path)
              path.emplace_back(std::move(ss));
              const safra_state& loopstart = path.back();
              bool in_seen = cs_.seen.find(ss) != cs_.seen.end();
              unsigned tokeep = path.size()-1;
              unsigned idx = path.size()-2;
              // The loop is guaranteed to end, because path contains too
              // occurrences of loopstart
              while (!(loopstart == path[idx]))
                {
                  // if path[tokeep] is already in seen, replace it with a
                  // smaller state also in seen.
                  if (in_seen && cs_.seen.find(path[idx]) != cs_.seen.end())
                    if (path[idx] < path[tokeep])
                      tokeep = idx;

                  // if path[tokeep] is not in seen, replace it either with a
                  // state in seen or with a smaller state
                  if (!in_seen)
                    {
                      if (cs_.seen.find(path[idx]) != cs_.seen.end())
                        {
                          tokeep = idx;
                          in_seen = true;
                        }
                      else if (path[idx] < path[tokeep])
                        tokeep = idx;
                    }
                  --idx;
                }
              // clean references to path before move (see next line)
              states.clear();
              // move is safe, no dangling references
              ss = std::move(path[tokeep]);
              color_ = mincolor;
            }
          else
            {
              ss = cs_.src->compute_succ(cs_, ap, color_);
            }
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
  }

  safra_support::safra_support(const const_twa_graph_ptr& a,
                               bool use_stutter,
                               const scc_info& scc)
  : state_supports(a->num_states())
  {
    if (use_stutter && a->prop_stutter_invariant())
      {
        // FIXME this could be improved
        // supports of states should account for possible stuttering if we plan
        // to use stuttering invariance
        for (unsigned c = 0; c != scc.scc_count(); ++c)
          {
            bdd c_supp = scc.scc_ap_support(c);
            for (const auto& su: scc.succ(c))
              c_supp &= state_supports[scc.one_state_of(su)];
            for (unsigned st: scc.states_of(c))
              state_supports[st] = c_supp;
          }
      }
    else
      {
        for (unsigned i = 0; i != a->num_states(); ++i)
          {
            bdd res = bddtrue;
            for (const auto& e : a->out(i))
              res &= bdd_support(e.cond);
            state_supports[i] = res;
          }
      }
  }

  const std::vector<bdd>&
  safra_support::get(const safra_state& s)
  {
    bdd supp = bddtrue;
    for (const auto& n : s.nodes_)
      supp &= state_supports[n.first];
    auto i = cache.emplace(supp, std::vector<bdd>());
    if (i.second) // insertion took place
      i.first->second = letters(supp);
    return i.first->second;
  }

  std::vector<char> find_scc_paths(const scc_info& scc);

  safra_state
  safra_state::compute_succ(const compute_succs& cs,
                            const bdd& ap, unsigned& color) const
  {
    safra_build& ss = cs.ss;
    ss.braces_ = braces_; // copy
    ss.nodes_.clear();
    for (const auto& node: nodes_)
      {
        // skip inactive nodes
        if (!cs.active.empty() && cs.active[node.first] == 0)
          continue;

        for (const auto& t: cs.aut->out(node.first))
          {
            if (!bdd_implies(ap, t.cond))
              continue;

            // Check if we are leaving the SCC, if so we delete all the
            // braces as no cycles can be found with that node
            if (cs.use_scc && cs.scc.scc_of(node.first) != cs.scc.scc_of(t.dst))
              if (cs.scc.is_accepting_scc(cs.scc.scc_of(t.dst)))
                // Entering accepting SCC so add brace
                ss.update_succ_toplevel<true>(t.dst);
              else
                // When entering non accepting SCC don't create any braces
                ss.update_succ_toplevel<false>(t.dst);
            else
              ss.update_succ(node.second, t.dst, t.acc);
          }
      }
    return safra_state(ss, cs, color);
  }

  // When a node a implies a node b, remove the node a.
  void
  safra_state::merge_redundant_states(
      const std::vector<std::vector<char>>& implies)
  {
    auto it1 = nodes_.begin();
    while (it1 != nodes_.end())
      {
        const auto& imp1 = implies[it1->first];
        bool erased = false;
        for (auto it2 = nodes_.begin(); it2 != nodes_.end(); ++it2)
          {
            if (it1 == it2)
              continue;
            if (imp1[it2->first])
              {
                erased = true;
                it1 = nodes_.erase(it1);
                break;
              }
          }
        if (!erased)
          ++it1;
      }
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
    if (cs.use_simulation)
      merge_redundant_states(cs.implies);
    color = finalize_construction(s.braces_, cs);
  }

  bool
  safra_state::operator<(const safra_state& other) const
  {
    // FIXME what is the right, if any, comparison to perform?
    return braces_ == other.braces_ ? nodes_ < other.nodes_
                                    : braces_ < other.braces_;
  }
  size_t
  safra_state::hash() const
  {
    size_t res = 0;
    for (const auto& p : nodes_)
      {
        res ^= (res << 3) ^ p.first;
        res ^= (res << 3) ^ p.second;
      }
    for (const auto& b : braces_)
      res ^= (res << 3) ^ b;
    return res;
  }

  bool
  safra_state::operator==(const safra_state& other) const
  {
    return nodes_ == other.nodes_ && braces_ == other.braces_;
  }

  // res[i + scccount*j] = 1 iff SCC i is reachable from SCC j
  std::vector<char>
  find_scc_paths(const scc_info& scc)
  {
    unsigned scccount = scc.scc_count();
    std::vector<char> res(scccount * scccount, 0);
    for (unsigned i = 0; i != scccount; ++i)
      res[i + scccount * i] = 1;
    for (unsigned i = 0; i != scccount; ++i)
      {
        std::stack<unsigned> s;
        s.push(i);
        while (!s.empty())
          {
            unsigned src = s.top();
            s.pop();
            for (unsigned d: scc.succ(src))
              {
                s.push(d);
                unsigned idx = scccount * i + d;
                res[idx] = 1;
              }
          }
      }
    return res;
  }

  template<bool incremental>
  twa_graph_ptr
  tgba_determinize_aux(const const_twa_graph_ptr& a,
                       bool pretty_print, bool use_scc,
                       bool use_simulation, bool use_stutter)
  {
    if (!a->is_existential())
      throw std::runtime_error
        ("tgba_determinize() does not support alternation");
    if (is_universal(a))
      return std::const_pointer_cast<twa_graph>(a);

    // Degeneralize
    const_twa_graph_ptr aut;
    {
      twa_graph_ptr aut_tmp = spot::degeneralize_tba(a);
      if (pretty_print)
        aut_tmp->copy_state_names_from(a);
      aut = aut_tmp;
    }

    determinizer d =
      determinizer::build(aut,
                          pretty_print, use_scc, use_simulation, use_stutter);

    if (incremental)
      {
        // test incremental build, even though it is not very useful here
        d.deactivate_all();
        unsigned n_states = d.aut()->num_states();
        for (unsigned i = 0; i != n_states; ++i)
          {
            d.activate(i);
            d.run();
          }

        d.get()->merge_edges();
        d.get()->purge_unreachable_states();
      }
    else
      {
        d.run();
      }

    cleanup_parity_here(d.get());
    return d.get();
  }

  twa_graph_ptr
  tgba_determinize(const const_twa_graph_ptr& a,
                   bool pretty_print, bool use_scc,
                   bool use_simulation, bool use_stutter)
  {
    return tgba_determinize_aux<false>(a, pretty_print, use_scc,
                                       use_simulation, use_stutter);
  }

  twa_graph_ptr
  tgba_determinize_incr(const const_twa_graph_ptr& a,
                        bool pretty_print, bool use_scc,
                        bool use_simulation, bool use_stutter)
  {
    return tgba_determinize_aux<true>(a, pretty_print, use_scc,
                                      use_simulation, use_stutter);
  }

  const safra_state&
  determinizer::get_safra(unsigned s) const
  {
    return num2safra_.at(s);
  }

  determinizer
  determinizer::build(const const_twa_graph_ptr& a,
                      bool pretty_print,
                      bool use_scc,
                      bool use_simulation,
                      bool use_stutter)
  {
    // the input automaton should be Büchi, to avoid the handling of the
    // edges correspondence between a TGBA and its degeneralization.
    if (!(a->acc().is_buchi() or a->acc().is_t()))
      throw std::runtime_error("incremental determinization only handles \
          Büchi automata");

    const_twa_graph_ptr aut = a;
    std::vector<bdd> implications;
    if (use_simulation)
      {
        aut = spot::scc_filter(aut);
        auto aut2 = simulation(aut, &implications);
        if (pretty_print)
          aut2->copy_state_names_from(aut);
        aut = aut2;
      }

    return determinizer(aut, implications,
                        pretty_print, use_scc, use_simulation, use_stutter);
  }

  determinizer::determinizer(const const_twa_graph_ptr& aut,
                             const std::vector<bdd>& implications,
                             bool pretty_print,
                             bool use_scc,
                             bool use_simulation,
                             bool use_stutter)
  : aut_(aut)
  , res_(nullptr)
  , scc_(aut_, use_stutter && aut_->prop_stutter_invariant() ?
               scc_info_options::TRACK_SUCCS | scc_info_options::TRACK_STATES :
               scc_info_options::TRACK_SUCCS)
  , safra2letters_(aut_, use_stutter, scc_)
  , active_(aut_->num_states(), false)
  , seen_()
  , num2safra_()
  , sets_(0)
  , pretty_print_(pretty_print)
  , use_scc_(use_scc)
  , use_simulation_(use_simulation)
  , use_stutter_(use_stutter)
  {
    // If use_simulation is false, implications is empty, so nothing is built
    implies_ = std::vector<std::vector<char>>(
        implications.size(),
        std::vector<char>(implications.size(), 0));
    {
      std::vector<char> is_connected = find_scc_paths(scc_);
      for (unsigned i = 0; i != implications.size(); ++i)
        {
          // NB spot::simulation() does not remove unreachable states, as it
          // would invalidate the contents of 'implications'.
          // so we need to explicitely test for unreachable states
          // FIXME based on the scc_info, we could remove the unreachable
          // states, both in the input automaton and in 'implications'
          // to reduce the size of 'implies'.
          if (!scc_.reachable_state(i))
            continue;
          for (unsigned j = 0; j != implications.size(); ++j)
            {
              if (!scc_.reachable_state(j))
                continue;

              // index to see if there is a path from scc2 -> scc1
              unsigned idx = scc_.scc_count() * scc_.scc_of(j) + scc_.scc_of(i);
              implies_[i][j] =    !is_connected[idx]
                              &&  bdd_implies(implications[i], implications[j]);
            }
        }
    }

    reset_res();
  }

  void determinizer::reset_res()
  {
    seen_.clear();
    num2safra_.clear();
    sets_ = 0;

    res_ = make_twa_graph(aut_->get_dict());
    res_->copy_ap_of(aut_);
    res_->prop_copy(aut_, { false, // state based
                            false, // inherently_weak
                            false, false, // deterministic
                            true, // complete
                            true // stutter inv
                          });

    // add initial state
    unsigned init_state = aut_->get_init_state_number();
    bool start_accepting =
      !use_scc_ || scc_.is_accepting_scc(scc_.scc_of(init_state));
    safra_state init(init_state, start_accepting);
    unsigned num = res_->new_state();
    seen_.emplace(init, num); // insert initial state in seen_
    num2safra_.emplace_back(init);
    res_->set_init_state(num); // mark as initial state
  }

  void
  determinizer::activate(unsigned s)
  {
    toadd_[s] = true;
  }

  void
  determinizer::deactivate_all()
  {
    toadd_ = std::vector<char>(aut_->num_states(), false);
    reset_res();
  }

  twa_graph_ptr
  determinizer::get() const
  {
    return res_;
  }

  void
  determinizer::run()
  {
    // In fact, simulation can be used as usual.
    // Note that the simulation is based on the full automaton, so the
    // deterministic automaton may NOT be equivalent to the current
    // sub-automaton, but its language is included in the one of the input full
    // automaton.

    // As per the standard, references to elements in a std::unordered_set or
    // std::unordered_map are invalidated by erasure only.
    std::deque<std::reference_wrapper<power_set::value_type>> todo;

    // find the actual state associated to a given safra state
    // if the safra state has no actual state associated with it, create it and
    // update seen_, num2safra_ and todo accordingly
    auto get_state = [this, &todo](const safra_state& s) -> unsigned
      {
        auto it = seen_.find(s);
        if (it == seen_.end())
          {
            unsigned dst_num = res_->new_state();
            it = seen_.emplace(s, dst_num).first;
            num2safra_.emplace_back(s);
            todo.emplace_back(*it);
          }
        assert(it->second < res_->num_states());
        return it->second;
      };

    // add a safra state to todo, if the given safra state has not been deleted
    // (i.e. it is still in seen_)
    auto re_add = [this, &todo](const safra_state& s) -> void
      {
        auto it = seen_.find(s);
        if (it != seen_.end())
          todo.emplace_back(*it);
      };

    if (toadd_.empty())
      {
        // special case: unconditionnally build the whole automaton
        active_ = {};
        unsigned init_state = aut_->get_init_state_number();
        bool start_accepting =
          !use_scc_ || scc_.is_accepting_scc(scc_.scc_of(init_state));
        safra_state init(init_state, start_accepting);
        re_add(init);
      }
    else
      {
        // currently, incremental determinization does not support
        // stutter-invariance
        // stutter-invariance allows to accelerate paths when computing a
        // successor: there is no way currently to detect that some of the
        // intermediate (accelerated) states needs to be re-computed
        use_stutter_ = false;

        // re-add all safra states containing one activated state
        // also remove all edges from these
        std::set<safra_state> waiting;
        for (unsigned i = 0; i != toadd_.size(); ++i)
          {
            if (toadd_[i] && !active_[i])
              {
                active_[i] = 1; // activate state

                for (const auto& ss : seen_)
                  {
                    if (ss.first.contains(i))
                      {
                        unsigned num = ss.second;
                        // add the state to waiting
                        waiting.insert(ss.first);
                        // remove all outgoing edges from res_
                        for (auto t = res_->get_graph().out_iteraser(num); t;)
                          t.erase(); // erase() advances the iterator
                        // remove the state from non-deterministic states
                        nondet_states_.erase(num);
                      }
                  }
              }
          }

        void (*f)(const std::vector<unsigned>&, void*) =
          [](const std::vector<unsigned>& newst, void* d)
          {
            determinizer* det = static_cast<determinizer*>(d);
            for (auto it = det->seen_.begin(); it != det->seen_.end();)
              {
                unsigned dst = newst[it->second];
                if (dst == -1U)
                  it = det->seen_.erase(it);
                else
                  {
                    it->second = dst;
                    ++it;
                  }
              }
            unsigned j = 0;
            for (unsigned i = 0; i != newst.size(); ++i)
              if (newst[i] != -1U)
                det->num2safra_[j++] = det->num2safra_[i];
            det->num2safra_.resize(j);
          };

        res_->purge_unreachable_states(&f, this);
        for (const auto& s : waiting)
          re_add(s);

        // add the initial state if it is the first time that run() is called
        if (res_->num_edges() == 0)
          re_add(get_safra(res_->get_init_state_number()));
      }

    compute_succs succs(aut_, seen_, scc_, implies_, active_, use_scc_,
                        use_simulation_, use_stutter_);
    // The main loop
    while (!todo.empty())
      {
        const safra_state& curr = todo.front().get().first;
        unsigned src_num = todo.front().get().second;
        todo.pop_front();
        assert(src_num < res_->num_states());
        succs.set(curr, safra2letters_.get(curr));
        for (auto s = succs.begin(); s != succs.end(); ++s)
          {
            // Don't construct sink state as complete does a better job at this
            if (s->nodes_.empty())
              continue;
            unsigned dst_num = get_state(*s);
            if (s.color_ != -1U)
              {
                res_->new_edge(src_num, dst_num, s.cond(), {s.color_});
                sets_ = std::max(s.color_ + 1, sets_);
              }
            else
              res_->new_edge(src_num, dst_num, s.cond());
          }

        if (active_.empty())
          continue;

        // inactive nodes remain non-deterministic
        for (const auto& node : curr.nodes_)
          {
            if (active_[node.first] == 0)
              {
                nondet_states_.insert(src_num);
                bool is_acc = (node.second >= 0);

                for (const auto& e : aut_->out(node.first))
                  {
                    bool is_accepting =
                      !use_scc_ || scc_.is_accepting_scc(scc_.scc_of(e.dst));
                    unsigned dst_num =
                      get_state(safra_state(e.dst, is_accepting));
                    if (e.acc || (is_acc && !is_accepting))
                      res_->new_edge(src_num, dst_num, e.cond, {1});
                    else
                      res_->new_edge(src_num, dst_num, e.cond);
                  }
              }
          }
      }
    // Green and red colors work in pairs, so the number of parity conditions is
    // necessarily even.
    if (sets_ % 2 == 1)
      sets_ += 1;
    if (!nondet_states_.empty())
      sets_ = std::max(2U, sets_);

    // Acceptance is now min(odd) since we can emit Red on paths 0 with new opti
    res_->set_acceptance(sets_, acc_cond::acc_code::parity(false, true, sets_));
    res_->prop_universal(nondet_states_.empty() ? true : spot::trival::maybe());
    res_->prop_state_acc(false);

    if (pretty_print_)
      res_->set_named_prop("state-names", print_debug(aut_, seen_));
  }
}
