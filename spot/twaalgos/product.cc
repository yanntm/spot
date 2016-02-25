// -*- coding: utf-8 -*-
// Copyright (C) 2014, 2015, 2016 Laboratoire de Recherche et
// Développement de l'Epita (LRDE).
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

#include <spot/twaalgos/product.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/complete.hh>
#include <deque>
#include <queue>
#include <unordered_map>
#include <spot/misc/hash.hh>

namespace spot
{
  namespace
  {
    typedef std::pair<unsigned, unsigned> product_state;

    struct product_state_hash
    {
      size_t
      operator()(product_state s) const
      {
        return wang32_hash(s.first ^ wang32_hash(s.second));
      }
    };

    static
    twa_graph_ptr product_aux(const const_twa_graph_ptr& left,
			      const const_twa_graph_ptr& right,
			      unsigned left_state,
			      unsigned right_state,
			      bool and_acc)
    {
      std::unordered_map<product_state, unsigned, product_state_hash> s2n;
      std::deque<std::pair<product_state, unsigned>> todo;

      if (left->get_dict() != right->get_dict())
	throw std::runtime_error("product: left and right automata should "
				 "share their bdd_dict");
      auto res = make_twa_graph(left->get_dict());
      res->copy_ap_of(left);
      res->copy_ap_of(right);
      auto left_num = left->num_sets();
      auto right_acc = right->get_acceptance() << left_num;
      if (and_acc)
	right_acc &= left->get_acceptance();
      else
	right_acc |= left->get_acceptance();
      res->set_acceptance(left_num + right->num_sets(), right_acc);

      auto v = new product_states;
      res->set_named_prop("product-states", v);

      auto new_state =
	[&](unsigned left_state, unsigned right_state) -> unsigned
	{
	  product_state x(left_state, right_state);
	  auto p = s2n.emplace(x, 0);
	  if (p.second)		// This is a new state
	    {
	      p.first->second = res->new_state();
	      todo.emplace_back(x, p.first->second);
	      assert(p.first->second == v->size());
	      v->push_back(x);
	    }
	  return p.first->second;
	};

      res->set_init_state(new_state(left_state, right_state));
      if (right_acc.is_f())
	// Do not bother doing any work if the resulting acceptance is
	// false.
	return res;
      while (!todo.empty())
	{
	  auto top = todo.front();
	  todo.pop_front();
	  for (auto& l: left->out(top.first.first))
	    for (auto& r: right->out(top.first.second))
	      {
		auto cond = l.cond & r.cond;
		if (cond == bddfalse)
		  continue;
		auto dst = new_state(l.dst, r.dst);
		res->new_edge(top.second, dst, cond,
			      l.acc | (r.acc << left_num));
		// If right is deterministic, we can abort immediately!
	      }
	}

      res->prop_deterministic(left->prop_deterministic()
			      && right->prop_deterministic());
      res->prop_stutter_invariant(left->prop_stutter_invariant()
				  && right->prop_stutter_invariant());
      // The product of X!a and Xa, two stutter-sentive formulas,
      // is stutter-invariant.
      //res->prop_stutter_sensitive(left->prop_stutter_sensitive()
      //			    && right->prop_stutter_sensitive());
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

    typedef std::vector<unsigned> product_n_state;

    struct product_n_state_hash
    {
      size_t
      operator()(const product_n_state& s) const
      {
        size_t result = 0;
        for (auto i: s)
          result = wang32_hash(result ^ wang32_hash(i));
        return result;
      }
    };

    template <class I>
    static twa_graph_ptr product_n_aux(I begin, I end)
    {
      if (begin == end)
        // Empty product
        throw std::invalid_argument("product_n(): cannot compute an empty "
                                    "product");
      if (std::next(begin) == end)
        // One factor in the product
        return copy(*begin, twa::prop_set::all());

      std::unordered_map<product_n_state, unsigned, product_n_state_hash> s2n;
      std::queue<std::pair<product_n_state, unsigned>> todo;

      // Count he numebr of automata
      unsigned num_automata = 0;
      for (auto i = begin; i != end; i++)
        ++num_automata;

      // Create the resulting automaton and initialize its AP
      auto res = make_twa_graph((*begin)->get_dict());
      for (auto i = std::next(begin); i != end; ++i)
        {
          if (res->get_dict() != (*i)->get_dict())
            throw std::runtime_error("product_n: automata should share "
                                     "their bdd_dict");
          res->copy_ap_of(*i);
        }

      // Number to shift for each acceptation condition
      // Compute the resulting acceptation condition
      std::vector<unsigned> acc_shift;
      acc_shift.reserve(num_automata - 1);
      auto right_acc = (*begin)->get_acceptance();
      auto j = begin;
      unsigned prev = 0;
      for (auto i = std::next(begin); i != end; i++)
      {
        prev += (*j)->num_sets();
        acc_shift.push_back(prev);
        j++;
        right_acc &= (*j)->get_acceptance() << acc_shift.back();
      }
      res->set_acceptance(acc_shift.back() + (*j)->num_sets(), right_acc);

      // Create a new state if it doesn't exist yet
      auto new_state =
        [&](const product_n_state& states) -> unsigned
        {
          auto p = s2n.emplace(states, 0);
          if (p.second)
            {
              p.first->second = res->new_state();
              todo.emplace(states, p.first->second);
            }
          return p.first->second;
        };

      // Compute the new initial state
      product_n_state init_state;
      init_state.reserve(num_automata);
      for (auto i = begin; i != end; i++)
        init_state.push_back((*i)->get_init_state_number());
      res->set_init_state(new_state(init_state));

      if (right_acc.is_f())
        // Do not bother doing any work if the resulting acceptance is
        // false.
        return res;

      while (!todo.empty())
        {
          auto top = todo.front();
          todo.pop();
          for (auto& first_edge: (*begin)->out(*top.first.begin()))
            {
              auto cond = first_edge.cond;
              auto acc = first_edge.acc;
              product_n_state dst;
              dst.reserve(num_automata);
              dst.push_back(first_edge.dst);
              std::function<void(bdd, acc_cond::mark_t,
                                 std::vector<unsigned>::iterator,
                                 product_n_state, I,
                                 std::vector<unsigned>::iterator)> traversal =
                [&](bdd cond, acc_cond::mark_t acc,
                    std::vector<unsigned>::iterator current_state,
                    product_n_state dst, I current_automaton,
                    std::vector<unsigned>::iterator current_acc_shift) -> void
                {
                  auto cur_edges = (*current_automaton)->out(*current_state);
                  for (auto& edge: cur_edges)
                    {
                      auto current_cond = cond & edge.cond;
                      if (current_cond == bddfalse)
                        continue;
                      auto current_acc = acc | (edge.acc << *current_acc_shift);
                      dst.push_back(edge.dst);
                      if (std::next(current_automaton) == end)
                      {
                        auto dst_state = new_state(dst);
                        res->new_edge(top.second, dst_state,
                                      current_cond, current_acc);
                      }
                      else
                        traversal(current_cond, current_acc,
                                  std::next(current_state), dst,
                                  std::next(current_automaton),
                                  std::next(current_acc_shift));
                      dst.pop_back();
                    }
                };
              traversal(cond, acc, std::next(top.first.begin()), dst,
                        std::next(begin), acc_shift.begin());
            }
        }
      res->prop_deterministic(std::all_of(begin, end, [](twa_graph_ptr i)
                              {
                                return i->prop_deterministic();
                              }));

      res->prop_stutter_invariant(std::all_of(begin, end, [](twa_graph_ptr i)
                                  {
                                    return i->prop_stutter_invariant();
                                  }));

      res->prop_inherently_weak(std::all_of(begin, end, [](twa_graph_ptr i)
                                {
                                  return i->prop_inherently_weak();
                                }));

      res->prop_weak(std::all_of(begin, end, [](twa_graph_ptr i)
                     {
                       return i->prop_weak();
                     }));

      res->prop_terminal(std::all_of(begin, end, [](twa_graph_ptr i)
                         {
                           return i->prop_terminal();
                         }));

      res->prop_state_acc(std::all_of(begin, end, [](twa_graph_ptr i)
                          {
                            return i->prop_state_acc();
                          }));
      return res;
    }
  }

  twa_graph_ptr product(const const_twa_graph_ptr& left,
			const const_twa_graph_ptr& right,
			unsigned left_state,
			unsigned right_state)
  {
    return product_aux(left, right, left_state, right_state, true);
  }

  twa_graph_ptr product(const const_twa_graph_ptr& left,
			const const_twa_graph_ptr& right)
  {
    return product(left, right,
		   left->get_init_state_number(),
		   right->get_init_state_number());
  }

  twa_graph_ptr product_or(const const_twa_graph_ptr& left,
			   const const_twa_graph_ptr& right,
			   unsigned left_state,
			   unsigned right_state)
  {
    return product_aux(complete(left),
		       complete(right),
		       left_state, right_state, false);
  }

  twa_graph_ptr product_n(std::initializer_list<twa_graph_ptr> automata)
  {
    return product_n_aux(automata.begin(), automata.end());
  }

  twa_graph_ptr product_n(const std::vector<twa_graph_ptr>& automata)
  {
    return product_n_aux(automata.begin(), automata.end());
  }

  twa_graph_ptr product_or(const const_twa_graph_ptr& left,
			   const const_twa_graph_ptr& right)
  {
    return product_or(left, right,
		      left->get_init_state_number(),
		      right->get_init_state_number());
  }
}
