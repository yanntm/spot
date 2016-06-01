// -*- coding: utf-8 -*-
// Copyright (C) 2016 Laboratoire de Recherche et Développement
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

#include <spot/twaalgos/parity.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/copy.hh>
#include <spot/twaalgos/product.hh>
#include <vector>
#include <utility>
#include <functional>
#include <queue>

namespace spot
{
  namespace
  {
    unsigned change_set(unsigned x, const unsigned num_sets,
                        const bool change_order, const bool change_style)
    {
      if (change_order)
        {
          // If the parity acceptance order is changed,
          // then the index of the sets are reversed
          x = num_sets - x - 1;
        }
      if (change_style)
        {
          // If the parity style is changed, then all the existing acceptance
          // sets are shifted
          ++x;
        }
      return x;
    }

    void
    change_acc(twa_graph_ptr& aut, unsigned num_sets, const bool change_order,
               const bool change_style, const bool output_max,
               const bool input_max)
    {
      for (auto& e: aut->edge_vector())
        if (e.acc)
          {
            unsigned msb = 0U;
            if (input_max)
              msb = e.acc.max_set() - 1;
            else
              for (auto i = 0U; i < num_sets; ++i)
                if (e.acc.has(i))
                  {
                    msb = i;
                    break;
                  }
            e.acc = acc_cond::mark_t();
            e.acc.set(change_set(msb, num_sets, change_order, change_style));
          }
        else if (output_max && change_style)
          {
            // If the parity is changed, a new set is introduced.
            // A parity max acceptance will mark the transitions which do not
            // belong to any set with this new set.
            // A parity min acceptance will introduce a unused acceptance set.
            e.acc.set(0);
          }
    }
  }

  twa_graph_ptr
  change_parity(const const_twa_graph_ptr& aut,
                parity_order order, parity_style style)
  {
    bool current_max;
    bool current_odd;
    if (!aut->acc().is_parity(current_max, current_odd))
      throw new std::invalid_argument("change_parity_acceptance: The first"
                                      "argument aut must have a parity "
                                      "acceptance.");
    auto result = copy(aut, twa::prop_set::all());
    auto old_num_sets = result->num_sets();

    bool output_max = true;
    switch (order)
      {
        case parity_order_max:
          output_max = true;
          break;
        case parity_order_min:
          output_max = false;
          break;
        case parity_order_same:
          output_max = current_max;
          break;
        case parity_order_dontcare:
          // If we need to change the style we may change the order not to
          // introduce new accset.
          output_max = (((style == parity_style_odd && !current_odd)
                         || (style == parity_style_even && current_odd))
                        && old_num_sets % 2 == 0) != current_max;
      }

    bool change_order = current_max != output_max;
    bool toggle_style = change_order && (old_num_sets % 2 == 0);

    bool output_odd = true;
    switch (style)
      {
        case parity_style_odd:
          output_odd = true;
          break;
        case parity_style_even:
          output_odd = false;
          break;
        case parity_style_same:
          output_odd = current_odd;
          break;
        case parity_style_dontcare:
          output_odd = current_odd != toggle_style;
          // If we need to change the order we may change the style not to
          // introduce new accset.
          break;
      }

    current_odd = current_odd != toggle_style;
    bool change_style = false;
    auto num_sets = old_num_sets;
    // If the parity neeeds to be changed, then a new acceptance set is created.
    // The old acceptance sets are shifted
    if (output_odd != current_odd)
      {
        change_style = true;
        ++num_sets;
      }

    if (change_order || change_style)
      {
        auto new_acc = acc_cond::acc_code::parity(output_max,
                                                  output_odd, num_sets);
        result->set_acceptance(num_sets, new_acc);
      }
    change_acc(result, old_num_sets, change_order,
               change_style, output_max, current_max);
    return result;
  }

  twa_graph_ptr
  cleanup_parity_acceptance(const const_twa_graph_ptr& aut, bool keep_style)
  {
    auto result = copy(aut, twa::prop_set::all());
    return cleanup_parity_acceptance_here(result, keep_style);
  }

  twa_graph_ptr
  cleanup_parity_acceptance_here(twa_graph_ptr aut, bool keep_style)
  {
    bool current_max;
    bool current_odd;
    if (!aut->acc().is_parity(current_max, current_odd))
      throw new std::invalid_argument("colorize_parity: The first argument aut "
                                      "must have a parity acceptance.");
    auto used_in_aut = acc_cond::mark_t();
    // Compute all the used sets
    if (aut->num_sets() > 0)
      {
        for (auto& t: aut->edges())
        {
          if (current_max)
            {
              auto maxset = t.acc.max_set();
              if (maxset)
                {
                  t.acc = acc_cond::mark_t();
                  t.acc.set(maxset - 1);
                }
            }
          else
            t.acc = t.acc.lowest();
          used_in_aut |= t.acc;
        }
      }
    if (used_in_aut)
    {
      // Never remove the least significant acceptance set, and mark the
      // acceptance set 0 to keep the style if needed.
      if (aut->num_sets() > 0)
        {
          if (current_max || keep_style)
            used_in_aut.set(0);
          if (!current_max)
            used_in_aut.set(aut->num_sets() - 1);
        }

      // Fill the vector shift with the new acceptance sets
      std::vector<unsigned> shift(aut->acc().num_sets());
      int prev_used = -1;
      bool change_style = false;
      unsigned new_index = 0;
      for (auto i = 0U; i < shift.size(); ++i)
        if (used_in_aut.has(i))
          {
            if (prev_used == -1)
              change_style = i % 2 != 0;
            else if ((i + prev_used) % 2 != 0)
              ++new_index;
            shift[i] = new_index;
            prev_used = i;
          }

      // Update all the transitions with the vector shift
      for (auto& t: aut->edges())
        {
          auto maxset = t.acc.max_set();
          if (maxset)
            {
              t.acc = acc_cond::mark_t();
              t.acc.set(shift[maxset - 1]);
            }
        }
      auto new_num_sets = new_index + 1;
      if (new_num_sets < aut->num_sets())
        {
          auto new_acc = acc_cond::acc_code::parity(current_max,
                                                    current_odd != change_style,
                                                    new_num_sets);
          aut->set_acceptance(new_num_sets, new_acc);
        }
    }
    else if (aut->num_sets() > 0U)
      {
        if ((current_max && current_odd)
           || (!current_max && current_odd != (aut->num_sets() % 2 == 0)))
          aut->set_acceptance(0, acc_cond::acc_code::t());
        else
          aut->set_acceptance(0, acc_cond::acc_code::f());
      }
    return aut;
  }

  twa_graph_ptr
  colorize_parity(const const_twa_graph_ptr& aut, bool keep_style)
  {
    return colorize_parity_here(copy(aut, twa::prop_set::all()), keep_style);
  }

  twa_graph_ptr
  colorize_parity_here(twa_graph_ptr aut, bool keep_style)
  {
    bool current_max;
    bool current_odd;
    if (!aut->acc().is_parity(current_max, current_odd))
      throw new std::invalid_argument("colorize_parity: The first argument aut "
                                      "must have a parity acceptance.");

    bool has_empty = false;
    for (const auto& e: aut->edges())
      if (!e.acc)
        {
          has_empty = true;
          break;
        }
    auto num_sets = aut->num_sets();
    unsigned incr = 0U;
    if (has_empty)
      {
        // If the automaton has a transition that belong to any set, we need to
        // introduce a new acceptance set.
        if (keep_style && current_max)
          {
            // We may want to add a second acceptance set to keep the style of
            // the parity acceptance
            incr = 2;
          }
        else
          incr = 1;
        num_sets += incr;
        bool new_style = current_odd == (keep_style || !current_max);
        auto new_acc = acc_cond::acc_code::parity(current_max,
                                                  new_style, num_sets);
        aut->set_acceptance(num_sets, new_acc);
      }
    if (current_max)
      for (auto& e: aut->edges())
        {
          auto maxset = e.acc.max_set();
          e.acc = acc_cond::mark_t();
          if (maxset == 0)
            e.acc.set(incr - 1);
          else
            e.acc.set(maxset + incr - 1);
        }
    else
      for (auto& e: aut->edges())
        {
          if (e.acc)
            e.acc = e.acc.lowest();
          else
            {
              e.acc = acc_cond::mark_t();
              e.acc.set(num_sets - incr);
            }
        }
    return aut;
  }

  namespace
  {
    class state_history : public std::vector<bool>
    {
    public:
      state_history(unsigned left_num_sets, unsigned right_num_sets) :
        left_num_sets_(left_num_sets),
        right_num_sets_(right_num_sets)
      {
        resize(left_num_sets * right_num_sets * 2, false);
      }

      bool get_e(unsigned left, unsigned right) const
      {
        return get(left, right, true);
      }

      bool get_f(unsigned left, unsigned right) const
      {
        return get(left, right, false);
      }

      void set_e(unsigned left, unsigned right, bool val)
      {
        set(left, right, true, val);
      }

      void set_f(unsigned left, unsigned right, bool val)
      {
        set(left, right, false, val);
      }

      unsigned get_left_num_sets() const
      {
        return left_num_sets_;
      }

      unsigned get_left_num_sets() const
      {
        return right_num_sets_;
      }

    private:
      unsigned left_num_sets_;
      unsigned right_num_sets_;

      bool get(unsigned left, unsigned right, bool first) const
      {
        return at(left * right_num_sets_ * 2 + right * 2 + (first ? 1 : 0));
      }

      void set(unsigned left, unsigned right, bool first, bool val)
      {
        at(left * right_num_sets_ * 2 + right * 2 + (first ? 1 : 0)) = val;
      }
    };

    typedef std::tuple<unsigned, unsigned, state_history>
      product_state;

    struct product_state_hash
    {
      size_t
      operator()(product_state s) const
      {
        auto result = wang32_hash(std::get<0>(s) ^ wang32_hash(std::get<1>(s)));
        return result ^ (std::hash<std::vector<bool>>()(std::get<2>(s)) << 1);
      }
    };

    twa_graph_ptr
    parity_product_aux(twa_graph_ptr& left, twa_graph_ptr& right)
    {
      std::unordered_map<product_state, std::pair<unsigned, unsigned>,
                         product_state_hash> s2n;
      std::queue<std::pair<product_state, unsigned>> todo;
      auto res = make_twa_graph(left->get_dict());
      res->copy_ap_of(left);
      res->copy_ap_of(right);
      unsigned left_num_sets = left->num_sets();
      unsigned right_num_sets = right->num_sets();
      unsigned z_size = left_num_sets + right_num_sets - 1;
      auto z = acc_cond::acc_code::parity(true, false, z_size);
      res->set_acceptance(z_size, z);

      auto v = new product_states;
      res->set_named_prop("product-states", v);

      auto new_state =
        [&](const state_history& current_history,
            unsigned left_state, unsigned right_state,
            unsigned left_acc_set, unsigned right_acc_set)
        -> std::pair<unsigned, unsigned>
        {
          product_state x(left_state, right_state, current_history);
          auto& mat = std::get<2>(x);
          for (unsigned i = 0; i < left_num_sets; ++i)
            for (unsigned j = 0; j < right_num_sets; ++j)
            {
              auto e_ij = current_history.get_e(i, j);
              auto f_ij = current_history.get_f(i, j);
              auto left_in_i = left_acc_set >= i;
              auto right_in_j = right_acc_set >= j;
              if (e_ij && f_ij)
              {
                mat.set_e(i, j, left_in_i);
                mat.set_f(i, j, right_in_j);
              }
              else
              {
                mat.set_e(i, j, e_ij || left_in_i);
                mat.set_f(i, j, f_ij || right_in_j);
              }
            }
          auto p = s2n.emplace(x, std::make_pair(0, 0));
          if (p.second)                 // This is a new state
          {
            p.first->second.first = res->new_state();
            p.first->second.second = 0;
            for (unsigned i = z_size - 1; i > 0
                 && p.first->second.second == 0; --i)
            {
              // i is the index of the resulting automaton acceptance set
              // If i is even, it means that the according set is a set with
              // transitions that need to be infinitly often as the acceptance
              // is a parity even. Then k, the index of the first automaton must
              // be even too.
              unsigned k = 0;
              if (i >= right_num_sets)
                k = i - right_num_sets + 1;
              unsigned var = 2 - i % 2;
              k += k & ~var & 1;
              unsigned max_k = std::min(i + 1, left_num_sets);
              while (k < max_k)
              {
                unsigned l = i - k;
                if (mat.get_e(k, l) && mat.get_f(k, l))
                {
                  p.first->second.second = i;
                  break;
                }
                k += var;
              }
              v->push_back(std::make_pair(left_state, right_state));
            }
            todo.emplace(x, p.first->second.first);
          }
          return p.first->second;
        };

      state_history init_state_history(left_num_sets, right_num_sets);
      product_state init_state(left->get_init_state_number(),
                               right->get_init_state_number(),
                               init_state_history);
      auto init_state_index = res->new_state();
      s2n.emplace(init_state, std::make_pair(init_state_index, 0));
      todo.emplace(init_state, init_state_index);
      res->set_init_state(init_state_index);

      while (!todo.empty())
      {
        auto& top = todo.front();
        for (auto& l: left->out(std::get<0>(top.first)))
          for (auto& r: right->out(std::get<1>(top.first)))
          {
            auto cond = l.cond & r.cond;
            if (cond == bddfalse)
              continue;
            auto left_acc = l.acc.max_set() - 1;
            auto right_acc = r.acc.max_set() - 1;
            auto dst = new_state(std::get<2>(top.first), l.dst, r.dst,
                                 left_acc, right_acc);
            auto acc = acc_cond::mark_t();
            acc.set(dst.second);
            res->new_edge(top.second, dst.first, cond, acc);
          }
        todo.pop();
      }

      res->prop_deterministic(left->prop_deterministic()
                              && right->prop_deterministic());
      res->prop_stutter_invariant(left->prop_stutter_invariant()
                                  && right->prop_stutter_invariant());
      // The product of X!a and Xa, two stutter-sentive formulas,
      // is stutter-invariant.
      //res->prop_stutter_sensitive(left->prop_stutter_sensitive()
      //                            && right->prop_stutter_sensitive());
      res->prop_inherently_weak(left->prop_inherently_weak()
                                && right->prop_inherently_weak());
      res->prop_weak(left->prop_weak()
                     && right->prop_weak());
      res->prop_terminal(left->prop_terminal()
                         && right->prop_terminal());
      res->prop_state_acc(left->prop_state_acc()
                          && right->prop_state_acc());
      return res;
    }
  }

  twa_graph_ptr
  parity_product(const const_twa_graph_ptr& left,
                 const const_twa_graph_ptr& right)
  {
    if (left->get_dict() != right->get_dict())
      throw std::runtime_error("parity_product: left and right automata "
                               "should share their bdd_dict");
    auto first = change_parity(left, parity_order_max, parity_style_even);
    auto second = change_parity(right, parity_order_max, parity_style_even);
    cleanup_parity_acceptance_here(first, true);
    cleanup_parity_acceptance_here(second, true);
    colorize_parity_here(first, true);
    colorize_parity_here(second, true);
    return parity_product_aux(first, second);
  }
}
