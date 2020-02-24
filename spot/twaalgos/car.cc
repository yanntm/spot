// -*- coding: utf-8 -*-
// Copyright (C) 2012-2019 Laboratoire de Recherche
// et Développement de l'Epita (LRDE).
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
#include <utility>
#include <spot/twa/twa.hh>
#include <spot/twaalgos/cleanacc.hh>
#include <spot/twaalgos/degen.hh>
#include <spot/twaalgos/car.hh>
#include <spot/twaalgos/dualize.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/parity.hh>
#include <spot/twaalgos/remfin.hh>
#include <spot/twaalgos/sccinfo.hh>
#include <spot/twaalgos/totgba.hh>
#include <unordered_map>

#include <unistd.h>

unsigned long max_choices = 0;
unsigned long nb_choices = 0;
unsigned long total_choices = 0;

namespace spot
{
  namespace
  {
// A simple tree. Each node stores a list of children and a color or a
// state of res_. We construct a tree to find an existing state and such that
// it's easy to have the most recent among a set of compatible states.
    struct node
    {
      // A color of the permutation or a state.
      unsigned label;
      std::vector<node*> children;
      // is_leaf is true if the label is a state of res_.
      bool is_leaf;

      node()
        : node(0, 0){ }

      node(int label_, bool is_leaf_)
        : label(label_)
        , children(0)
        , is_leaf(is_leaf_){ }

      ~node()
      {
        for (auto c : children)
          delete c;
      }

      // Add a permutation to the tree.
      void
      add_new_perm(std::vector<unsigned> permu, int pos, unsigned state)
      {
        if (pos == -1)
          children.push_back(new node(state, true));
        else
        {
          auto lab = permu[pos];
          auto child = std::find_if(children.begin(), children.end(),
              [lab](node* n){ return n->label == lab; });
          if (child == children.end())
          {
            node* new_child = new node(lab, false);
            children.push_back(new_child);
            new_child->add_new_perm(permu, pos - 1, state);
          }
          else
            (*child)->add_new_perm(permu, pos - 1, state);
        }
      }

      // Gives a state of res_ (if it exists) reachable from this node.
      // If use_last is true, we take the most recent, otherwise we take
      // the oldest.
      unsigned
      get_end(bool use_last)
      {
        if (children.empty())
        {
          if (!is_leaf)
            return -1U;
          return label;
        }
        if (use_last)
          return children[children.size() - 1]->get_end(use_last);
        return children[0]->get_end(use_last);
      }

      // Try to find a state compatible with the permu when seen_nb colors are
      // moved.
      unsigned
      get_existing(std::vector<unsigned> permu, unsigned seen_nb, int pos,
        bool use_last)
      {
        if (pos < (int) seen_nb)
          return get_end(use_last);
        else
        {
          auto lab = permu[pos];
          auto child = std::find_if(children.begin(), children.end(),
              [lab](node* n){ return n->label == lab; });
          if (child == children.end())
            return -1U;
          return (*child)->get_existing(permu, seen_nb, pos - 1, use_last);
        }
      }
    };

// Link between a state and a car_state. We construct this structure for
// each SCC so a state is the state of the SCC and not of aut_.
    class state_2_car_scc
    {
      // Link between a state of the SCC and a tree.
      std::vector<node> nodes;

public:
      state_2_car_scc(unsigned nb_states)
        : nodes(nb_states, node()){ }

      unsigned
      get_res_state(unsigned state, std::vector<unsigned> permu,
        unsigned seen_nb, bool use_last)
      {
        return nodes[state].get_existing(permu, seen_nb,
                 permu.size() - 1, use_last);
      }

      void
      add_res_state(unsigned initial, unsigned state,
        std::vector<unsigned> permu)
      {
        nodes[initial].add_new_perm(permu, ((int) permu.size()) - 1, state);
      }
    };

    class car_generator
    {
      enum algorithm {
        // Try to have a Büchi condition if we have Rabin.
        Rabin_to_Buchi,
        Streett_to_Buchi,
        // IAR
        IAR_Streett,
        IAR_Rabin,
        // CAR
        CAR,
        // Changing colors transforms acceptance to max even/odd copy.
        Copy_even,
        Copy_odd,
        // If a condition is "t" or "f", we just have to copy the automaton.
        False_clean,
        True_clean,
        None
      };

      static std::string
      algorithm_to_str(algorithm algo)
      {
        std::string algo_str;
        switch (algo)
        {
            case IAR_Streett:
              algo_str = "IAR (Streett)";
              break;
            case IAR_Rabin:
              algo_str = "IAR (Rabin)";
              break;
            case CAR:
              algo_str = "CAR";
              break;
            case Copy_even:
              algo_str = "Copy even";
              break;
            case Copy_odd:
              algo_str = "Copy odd";
              break;
            case False_clean:
              algo_str = "False clean";
              break;
            case True_clean:
              algo_str = "True clean";
              break;
            case Streett_to_Buchi:
              algo_str = "Streett to Büchi";
              break;
            case Rabin_to_Buchi:
              algo_str = "Rabin to Büchi";
              break;
            default:
              algo_str = "None";
              break;
        }
        return algo_str;
      }

      using perm_t = std::vector<unsigned>;
      struct car_state
      {
        // State of the original automaton
        unsigned state;
        // We create a new automaton for each SCC of the original automaton
        // so we keep a link between a car_state and the state of the
        // subautomaton.
        unsigned state_scc;
        // Permutation used by IAR and CAR.
        perm_t perm;

        bool
        operator<(const car_state &other) const
        {
          if (state < other.state)
            return true;
          if (state > other.state)
            return false;
          if (perm < other.perm)
            return true;
          if (perm > other.perm)
            return false;
          return state_scc < other.state_scc;
        }

        std::string
        to_string(algorithm algo) const
        {
          std::stringstream s;
          s << state;
          unsigned ps = perm.size();
          if (ps > 0)
          {
            s << " [";
            for (unsigned i = 0; i != ps; ++i)
            {
              if (i > 0)
                s << ',';
              s << perm[i];
            }
            s << ']';
          }
          s << ", ";
          s << algorithm_to_str(algo);
          return s.str();
        }
      };

      const acc_cond::mark_t &
      fin(std::vector<acc_cond::rs_pair> pairs, unsigned k,
        algorithm algo) const
      {
        if (algo == IAR_Rabin)
          return pairs[k].fin;
        else
          return pairs[k].inf;
      }

      acc_cond::mark_t
      inf(std::vector<acc_cond::rs_pair> pairs, unsigned k,
        algorithm algo) const
      {
        if (algo == IAR_Rabin)
          return pairs[k].inf;
        else
          return pairs[k].fin;
      }

      // Create a permutation for the first state of a SCC (IAR)
      void
      initial_perm_iar(std::set<unsigned> &perm_elem, perm_t &p0,
        algorithm algo, const acc_cond::mark_t &colors,
        const std::vector<acc_cond::rs_pair> &pairs)
      {
        for (unsigned k = 0; k != pairs.size(); ++k)
          if (!inf(pairs, k, algo) || (colors & (pairs[k].fin | pairs[k].inf)))
          {
            perm_elem.insert(k);
            p0.push_back(k);
          }
      }

      // Create a permutation for the first state of a SCC (CAR)
      void
      initial_perm_car(perm_t &p0, const acc_cond::mark_t &colors)
      {
        auto cont = colors.sets();
        p0.assign(cont.begin(), cont.end());
      }

      void
      find_new_perm_iar(perm_t &new_perm,
        const std::vector<acc_cond::rs_pair> &pairs,
        const acc_cond::mark_t &acc,
        algorithm algo, const std::set<unsigned> &perm_elem,
        unsigned &seen_nb)
      {
        for (unsigned k : perm_elem)
          if (acc & fin(pairs, k, algo))
          {
            ++seen_nb;
            auto it = std::find(new_perm.begin(), new_perm.end(), k);

            // move the pair in front of the permutation
            std::rotate(new_perm.begin(), it, it + 1);
          }
      }

      // Given the set acc of colors appearing on an edge, create a new
      // permutation new_perm, and give the number seen_nb of colors moved to
      // the head of the permutation.
      void
      find_new_perm_car(perm_t &new_perm, const acc_cond::mark_t &acc,
        unsigned &seen_nb, unsigned &h)
      {
        for (unsigned k : acc.sets())
        {
          auto it = std::find(new_perm.begin(), new_perm.end(), k);
          if (it != new_perm.end())
          {
            h = std::max(h, unsigned(it - new_perm.begin()) + 1);
            std::rotate(new_perm.begin(), it, it + 1);
            ++seen_nb;
          }
        }
      }

      void
      get_acceptance_iar(algorithm algo, const perm_t &current_perm,
        const std::vector<acc_cond::rs_pair> &pairs,
        const acc_cond::mark_t &e_acc, acc_cond::mark_t &acc)
      {
        unsigned delta_acc = (algo == IAR_Streett) && is_odd;

        // find the maximal index encountered by this transition
        unsigned maxint = -1U;

        for (int k = current_perm.size() - 1; k >= 0; --k)
        {
          unsigned pk = current_perm[k];

          if (!inf(pairs, pk,
            algo)
             || (e_acc & (pairs[pk].fin | pairs[pk].inf)))
          {
            maxint = k;
            break;
          }
        }
        unsigned value;

        if (maxint == -1U)
          value = delta_acc;
        else if (e_acc & fin(pairs, current_perm[maxint], algo))
          value = 2 * maxint + 2 + delta_acc;
        else
          value = 2 * maxint + 1 + delta_acc;
        acc = { value };
        max_color = std::max(max_color, value);
      }

      void
      get_acceptance_car(const acc_cond &sub_aut_cond, const perm_t &new_perm,
        unsigned h, acc_cond::mark_t &acc)
      {
        acc_cond::mark_t m(new_perm.begin(), new_perm.begin() + h);
        bool rej = !sub_aut_cond.accepting(m);
        unsigned value = 2 * h + rej + is_odd;
        acc = { value };
        max_color = std::max(max_color, value);
      }

      // Copy the given automaton and add 0 or 1 to all transitions.
      void
      apply_false_true_clean(const twa_graph_ptr &sub_automaton, bool is_true,
        std::vector<int> inf_fin_prefix, unsigned max_free_color)
      {
        std::vector<unsigned>* init_states = sub_automaton
          ->get_named_prop<std::vector<unsigned>>("original-states");

        for (unsigned state = 0; state < sub_automaton->num_states(); ++state)
        {
          unsigned s_aut = (*init_states)[state];

          car_state new_car = { s_aut, state, perm_t() };
          auto new_state = res_->new_state();
          car2num[new_car] = new_state;
          num2car.insert(num2car.begin() + new_state, new_car);
          algorithm algo = is_true ? True_clean : False_clean;
          if (pretty_print)
            names->push_back(new_car.to_string(algo));
          state2car[s_aut] = new_car;
        }
        for (unsigned state = 0; state < sub_automaton->num_states(); ++state)
        {
          auto col = is_true ^ !is_odd;
          if (((unsigned) col) > max_free_color)
            throw std::runtime_error("CAR needs more sets");
          unsigned s_aut = (*init_states)[state];
          car_state src = { s_aut, state, perm_t() };
          unsigned src_state = car2num[src];
          for (auto e : aut_->out(s_aut))
          {
            for (auto c : e.acc.sets())
            {
              if (inf_fin_prefix[c] + is_odd > col)
                col = inf_fin_prefix[c] + is_odd;
            }
            acc_cond::mark_t cond = { (unsigned) col };
            max_color = std::max(max_color, (unsigned) col);
            if (scc_.scc_of(s_aut) == scc_.scc_of(e.dst))
              res_->new_edge(src_state, car2num[state2car[e.dst]], e.cond,
                cond);
          }
        }
      }

      void
      apply_lar(const twa_graph_ptr &sub_automaton, unsigned init,
        const std::vector<acc_cond::rs_pair> &pairs,
        algorithm algo, unsigned scc_num, std::vector<int> inf_fin_prefix,
        unsigned max_free_color)
      {
        auto state_2_car = state_2_car_scc(sub_automaton->num_states());
        std::vector<unsigned>* init_states = sub_automaton->
          get_named_prop<std::vector<unsigned>>("original-states");
        std::deque<car_state> todo;
        auto get_state =
          [&](const car_state &s){
            auto it = car2num.find(s);

            if (it == car2num.end())
            {
              unsigned nb = res_->new_state();
              if (search_ex)
                state_2_car.add_res_state(s.state_scc, nb, s.perm);
              car2num[s] = nb;
              num2car.insert(num2car.begin() + nb, s);

              todo.push_back(s);
              if (pretty_print)
                names->push_back(s.to_string(algo));
              return nb;
            }
            return it->second;
          };

        auto colors = sub_automaton->acc().get_acceptance().used_sets();
        std::set<unsigned> perm_elem;

        perm_t p0 = { };
        switch (algo)
        {
            case IAR_Streett:
            case IAR_Rabin:
              initial_perm_iar(perm_elem, p0, algo, colors, pairs);
              break;
            case CAR:
              initial_perm_car(p0, colors);
              break;
            default:
              assert(false);
              break;
        }

        car_state s0{ (*init_states)[init], init, p0 };
        get_state(s0); // put s0 in todo

        // the main loop
        while (!todo.empty())
        {
          car_state current = todo.front();
          todo.pop_front();

          unsigned src_num = get_state(current);

          for (const auto &e : sub_automaton->out(current.state_scc))
          {
            perm_t new_perm = current.perm;

            // Count pairs whose fin-part is seen on this transition
            unsigned seen_nb = 0;

            // consider the pairs for this SCC only
            unsigned h = 0;

            switch (algo)
            {
                case IAR_Rabin:
                case IAR_Streett:
                  find_new_perm_iar(new_perm, pairs, e.acc, algo,
                    perm_elem, seen_nb);
                  break;
                case CAR:
                  find_new_perm_car(new_perm, e.acc, seen_nb, h);
                  break;
                default:
                  assert(false);
            }

            // Optimization: when several indices are seen in the
            // transition, they move at the front of new_perm in any
            // order. Check whether there already exists an car_state
            // that matches this condition.

            car_state dst;
            unsigned dst_num = -1U;

            if (search_ex)
            {
              dst_num = state_2_car.get_res_state(e.dst, new_perm, seen_nb,
                  use_last);
            }

            if (dst_num == -1U)
            {
              auto dst = car_state{ (*init_states)[e.dst], e.dst, new_perm };
              dst_num = get_state(dst);
            }

            acc_cond::mark_t acc = { };

            switch (algo)
            {
                case IAR_Rabin:
                case IAR_Streett:
                  get_acceptance_iar(algo, current.perm, pairs, e.acc, acc);
                  break;
                case CAR:
                  get_acceptance_car(sub_automaton->acc(), new_perm, h, acc);
                  break;
                default:
                  assert(false);
            }

            unsigned acc_col = acc.min_set() - 1;
            if (parity_prefix)
            {
              if (acc_col > max_free_color)
                throw std::runtime_error("CAR needs more sets");
              // parity prefix
              for (auto col : e.acc.sets())
              {
                if (inf_fin_prefix[col] + is_odd > (int) acc_col)
                  {
                    max_color = std::max(max_color,
                                      (unsigned) inf_fin_prefix[col] + is_odd);
                    acc_col = (unsigned) inf_fin_prefix[col] + is_odd;
                  }
              }
            }
            res_->new_edge(src_num, dst_num, e.cond, { acc_col });
          }
        }
        auto leaving_edge =
          [&](unsigned d){
            return scc_.scc_of(num2car.at(d).state) != scc_num;
          };
        auto filter_edge =
          [](const twa_graph::edge_storage_t &,
            unsigned dst,
            void* filter_data){
            decltype(leaving_edge) *data =
              static_cast<decltype(leaving_edge)*>(filter_data);

            if ((*data)(dst))
              return scc_info::edge_filter_choice::ignore;

            return scc_info::edge_filter_choice::keep;
          };
        scc_info sub_scc(res_, get_state(s0), filter_edge, &leaving_edge);

        // SCCs are numbered in reverse topological order, so the bottom SCC has
        // index 0.
        const unsigned bscc = 0;
        assert(sub_scc.scc_count() != 0);
        assert(sub_scc.succ(0).empty());
        assert(
          [&](){
          for (unsigned s = 1; s != sub_scc.scc_count(); ++s)
            if (sub_scc.succ(s).empty())
              return false;

          return true;
        } ());

        assert(sub_scc.states_of(bscc).size() >= sub_automaton->num_states());

        // update state2car
        for (unsigned scc_state : sub_scc.states_of(bscc))
        {
          car_state &car = num2car.at(scc_state);

          if (state2car.find(car.state) == state2car.end())
            state2car[car.state] = car;
        }
      }

      // Copy a given automaton replacing i by permut[i].
      void
      apply_copy(const twa_graph_ptr &sub_automaton,
        const std::vector<unsigned> &permut,
        bool copy_odd, std::vector<int> inf_fin_prefix)
      {
        std::vector<unsigned>* init_states = sub_automaton
          ->get_named_prop<std::vector<unsigned>>("original-states");
        for (unsigned state = 0; state < sub_automaton->num_states(); ++state)
        {
          car_state new_car = { (*init_states)[state], state, perm_t() };
          auto new_state = res_->new_state();
          car2num[new_car] = new_state;
          num2car.insert(num2car.begin() + new_state, new_car);
          state2car[(*init_states)[state]] = new_car;
          if (pretty_print)
            names->push_back(
              new_car.to_string(copy_odd ? Copy_odd : Copy_even));
        }
        auto cond_col = sub_automaton->acc().get_acceptance().used_sets();
        for (unsigned s = 0; s < sub_automaton->num_states(); ++s)
        {
          for (auto e : sub_automaton->out(s))
          {
            acc_cond::mark_t mark = { };
            int max_edge = -1;
            for (auto col : e.acc.sets())
            {
              if (cond_col.has(col))
                max_edge = std::max(max_edge, (int) permut[col]);
              if (inf_fin_prefix[col] + is_odd > max_edge)
                max_edge = inf_fin_prefix[col] + is_odd;
            }
            if (max_edge != -1)
            {
              mark.set((unsigned) max_edge);
              max_color = std::max(max_color, (unsigned) max_edge);
            }
            car_state src = { (*init_states)[s], s, perm_t() },
              dst = { (*init_states)[e.dst], e.dst, perm_t() };
            unsigned src_state = car2num[src],
                     dst_state = car2num[dst];
            res_->new_edge(src_state, dst_state, e.cond, mark);
          }
        }
      }

      void apply_to_Buchi(twa_graph_ptr sub_automaton,
         bool is_streett_to_buchi, std::vector<int> inf_fin_prefix,
         unsigned max_free_color)
      {
        std::vector<unsigned>* init_states = sub_automaton
          ->get_named_prop<std::vector<unsigned>>("original-states");
        auto sub_cond = sub_automaton->get_acceptance();
        if (is_streett_to_buchi)
          sub_automaton->set_acceptance(sub_cond.complement());
        auto buchi = rabin_to_buchi_if_realizable(sub_automaton);
        if (is_streett_to_buchi)
          {
            max_color = std::max(max_color, 2U);
            sub_automaton->set_acceptance(sub_cond);
          }

        for (unsigned state = 0; state < buchi->num_states(); ++state)
        {
          car_state new_car = { (*init_states)[state], state, perm_t() };
          auto new_state = res_->new_state();
          car2num[new_car] = new_state;
          num2car.insert(num2car.begin() + new_state, new_car);
          state2car[(*init_states)[state]] = new_car;
          if (pretty_print)
            names->push_back(new_car.to_string(
              is_streett_to_buchi ? Streett_to_Buchi : Rabin_to_Buchi));
        }
        auto g = sub_automaton->get_graph();
        for (unsigned s = 0; s < buchi->num_states(); ++s)
        {
          unsigned b = g.state_storage(s).succ;
          while (b)
          {
            auto& e = g.edge_storage(b);
            auto acc = e.acc;
            acc <<= (is_odd + is_streett_to_buchi);
            if ((is_odd || is_streett_to_buchi) && acc == acc_cond::mark_t{ })
              acc = { (is_streett_to_buchi && is_odd) };
            car_state src = { (*init_states)[s], s, perm_t() },
                      dst = { (*init_states)[e.dst], e.dst, perm_t() };
            unsigned src_state = car2num[src],
                     dst_state = car2num[dst];
            auto col = ((int) acc.min_set()) - 1;
            if (col > (int) max_free_color)
              throw std::runtime_error("CAR needs more sets");
            auto& e2 = sub_automaton->get_graph().edge_storage(b);
            for (auto c : e2.acc.sets())
            {
              if (inf_fin_prefix[c] + is_odd > col)
                col = inf_fin_prefix[c] + is_odd;
            }
            if (col != -1)
              acc = { (unsigned) col };
            else
              acc = {};
            res_->new_edge(src_state, dst_state, e.cond, acc);
            b = e.next_succ;
          }
        }
      }

      // Given an automaton, determine the algorithm that we will apply
      algorithm
      chooseAlgo(twa_graph_ptr &sub_automaton,
        std::vector<acc_cond::rs_pair> &pairs,
        std::vector<unsigned> &permut)
      {
        auto scc_condition = sub_automaton->acc();
        if (parity_equiv)
        {
          if (scc_condition.is_f())
            return False_clean;
          if (scc_condition.is_t())
            return True_clean;
          auto permut_tmp = std::vector<int>(
            scc_condition.all_sets().max_set(), -1);

          if (!is_odd && scc_condition.is_parity_max_equiv(permut_tmp, true))
          {
            for (auto c : permut_tmp)
              permut.push_back((unsigned) c);

            scc_condition.apply_permutation(permut);
            sub_automaton->apply_permutation(permut);
            return Copy_even;
          }
          std::fill(permut_tmp.begin(), permut_tmp.end(), -1);
          if (scc_condition.is_parity_max_equiv(permut_tmp, false))
          {
            for (auto c : permut_tmp)
              permut.push_back((unsigned) c);
            scc_condition.apply_permutation(permut);
            sub_automaton->apply_permutation(permut);
            return Copy_odd;
          }
        }

        if (rabin_to_buchi)
        {
          if (rabin_to_buchi_if_realizable(sub_automaton) != nullptr)
          {
            return Rabin_to_Buchi;
          }
          else
          {
            bool streett_buchi = false;
            auto sub_cond = sub_automaton->get_acceptance();
            sub_automaton->set_acceptance(sub_cond.complement());
            streett_buchi =
              (rabin_to_buchi_if_realizable(sub_automaton) != nullptr);
            sub_automaton->set_acceptance(sub_cond);
            if (streett_buchi)
            {
              return Streett_to_Buchi;
            }
          }
        }

        auto pairs1 = std::vector<acc_cond::rs_pair>();
        auto pairs2 = std::vector<acc_cond::rs_pair>();
        std::sort(pairs1.begin(), pairs1.end());
        pairs1.erase(std::unique(pairs1.begin(), pairs1.end()), pairs1.end());
        std::sort(pairs2.begin(), pairs2.end());
        pairs2.erase(std::unique(pairs2.begin(), pairs2.end()), pairs2.end());
        bool is_r_like = scc_condition.is_rabin_like(pairs1);
        bool is_s_like = scc_condition.is_streett_like(pairs2);
        unsigned num_cols = scc_condition.get_acceptance().used_sets().count();
        if (is_r_like)
        {
          if ((is_s_like && pairs1.size() < pairs2.size()) || !is_s_like)
          {
            if (pairs1.size() > num_cols)
              return CAR;
            pairs = pairs1;
            return IAR_Rabin;
          }
          else if (is_s_like)
          {
            if (pairs2.size() > num_cols)
              return CAR;
            pairs = pairs2;
            return IAR_Streett;
          }
        }
        else
        {
          if (is_s_like)
          {
            if (pairs2.size() > num_cols)
              return CAR;
            pairs = pairs2;
            return IAR_Streett;
          }
        }
        return CAR;
      }

      void
      build_scc(twa_graph_ptr &sub_automaton, unsigned scc_num,
        std::vector<int> inf_fin_prefix, unsigned max_free_color)
      {
        unsigned init = 0;

        std::vector<acc_cond::rs_pair> pairs = { };
        auto permut = std::vector<unsigned>();
        auto algo = chooseAlgo(sub_automaton, pairs, permut);
        if ((algo == IAR_Rabin || algo == Copy_odd) && !is_odd)
        {
          is_odd = true;
          ++max_color;
          for (auto &edge : res_->edges())
            {
              if (edge.acc == acc_cond::mark_t{})
                edge.acc = {0};
              else
                edge.acc <<= 1;
            }
        }
        switch (algo)
        {
            case False_clean:
            case True_clean:
              apply_false_true_clean(sub_automaton, (algo == True_clean),
                                      inf_fin_prefix, max_free_color);
              break;
            case IAR_Streett:
            case IAR_Rabin:
            case CAR:
              apply_lar(sub_automaton, init, pairs, algo, scc_num,
                        inf_fin_prefix, max_free_color);
              break;
            case Copy_odd:
            case Copy_even:
              apply_copy(sub_automaton, permut, algo == Copy_odd,
                          inf_fin_prefix);
              break;
            case Rabin_to_Buchi:
            case Streett_to_Buchi:
              apply_to_Buchi(sub_automaton, (algo == Streett_to_Buchi),
                              inf_fin_prefix, max_free_color);
              break;
            default:
              break;
        }
      }

public:
      twa_graph_ptr
      run()
      {
        res_ = make_twa_graph(aut_->get_dict());
        res_->copy_ap_of(aut_);
        for (unsigned scc = 0; scc < scc_.scc_count(); ++scc)
        {
          if (!scc_.is_useful_scc(scc))
            continue;
          auto sub_automata = scc_.split_on_sets(scc, { }, true);
          if (sub_automata.empty())
          {
            for (auto state : scc_.states_of(scc))
            {
              auto new_state = res_->new_state();
              car_state new_car = { state, state, perm_t() };
              car2num[new_car] = new_state;
              num2car.insert(num2car.begin() + new_state, new_car);
              if (pretty_print)
                names->push_back(new_car.to_string(None));
              state2car[state] = new_car;
            }
            continue;
          }
          auto sub_automaton = sub_automata[0];
          if (scc_acc_clean)
          {
            simplify_acceptance_here(sub_automaton);
          }
          if (partial_degen)
          {
            sub_automaton = partial_degeneralize(sub_automaton);
            simplify_acceptance_here(sub_automaton);
          }
          std::vector<int> parity_prefix_colors (SPOT_MAX_ACCSETS, -1);
          unsigned min_prefix_color = SPOT_MAX_ACCSETS + 1;
          if (parity_prefix)
          {
            auto new_acc = sub_automaton->acc();
            auto colors = std::vector<unsigned>();
            bool inf_start =
              sub_automaton->acc().has_parity_prefix(new_acc, colors);
            sub_automaton->set_acceptance(new_acc);
            for (unsigned i = 0; i < colors.size(); ++i)
              parity_prefix_colors[colors[i]] =
                SPOT_MAX_ACCSETS - 2 - i - !inf_start;
            if (colors.size() > 0)
              min_prefix_color =
                SPOT_MAX_ACCSETS - 2 - colors.size() - 1 - !inf_start;
          }
          build_scc(sub_automaton, scc, parity_prefix_colors,
            min_prefix_color - 1);
        }

        for (unsigned state = 0; state < res_->num_states(); ++state)
        {
          unsigned original_state = num2car.at(state).state;
          auto state_scc = scc_.scc_of(original_state);
          for (auto edge : aut_->out(original_state))
          {
            if (scc_.scc_of(edge.dst) != state_scc)
            {
              auto car = state2car.find(edge.dst);
              if (car != state2car.end())
              {
                unsigned res_dst = car2num.at(car->second);
                res_->new_edge(state, res_dst, edge.cond, { });
              }
            }
          }
        }
        unsigned initial_state = aut_->get_init_state_number();
        auto initial_car = state2car.find(initial_state);
        auto initial_state_res = car2num.find(initial_car->second);
        if (initial_state_res != car2num.end())
          res_->set_init_state(initial_state_res->second);
        else
          res_->new_state();
        res_->set_acceptance(
          acc_cond::acc_code::parity_max(is_odd, max_color + 1));
        if (pretty_print)
          res_->set_named_prop("state-names", names);
        reduce_parity_here(res_);
        res_->purge_unreachable_states();
        return res_;
      }

      explicit car_generator(const const_twa_graph_ptr &a, bool search_ex,
        bool partial_degen, bool scc_acc_clean,
        bool parity_equiv, bool use_last, bool parity_prefix,
        bool rabin_to_buchi, bool pretty_print)
        : aut_(a)
        , scc_(scc_info(a))
        , is_odd(false)
        , max_color(0)
        , search_ex(search_ex)
        , partial_degen(partial_degen)
        , scc_acc_clean(scc_acc_clean)
        , parity_equiv(parity_equiv)
        , use_last(use_last)
        , parity_prefix(parity_prefix)
        , rabin_to_buchi(rabin_to_buchi)
        , pretty_print(pretty_print)
      {
        if (pretty_print)
          names = new std::vector<std::string>();
        else
          names = nullptr;
      }

private:
      // Automaton to transform
      const const_twa_graph_ptr &aut_;
      // scc_info of aut
      const scc_info scc_;
      // Result
      twa_graph_ptr res_;
      // true if the acceptance condition of the result is max odd,
      // false otherwise
      bool is_odd;
      // The maximum color appearing in the result
      unsigned max_color;

      // Link between a state of res_ and a car_state
      std::vector<car_state> num2car;
      // Link between a state of aut_ and a car_state
      std::map<unsigned, car_state> state2car;
      // Link between a car_state and a state of aut_
      std::map<car_state, unsigned> car2num;

      // Options :
      // When we move several elements, we can try to find an order
      // such that the new permutation already exists.
      const bool search_ex;
      // Apply a partial degeneralization to remove occurences
      // of Fin | Fin and Inf & Inf.
      const bool partial_degen;
      // Remove for each SCC the colors that don't appear. It implies that we
      // can use different algorithms for each SCC.
      const bool scc_acc_clean;
      // Check if there exists a permutations of colors such that the acceptance
      // condition is a partity condition.
      const bool parity_equiv;
      // Use the most recent state when looking for an existing state.
      const bool use_last;
      // Try to "copy" the part of the acceptance condition similar to parity.
      const bool parity_prefix;

      const bool rabin_to_buchi;

      // Give a name to the states describing the state of the aut_ and the
      // permutation.
      const bool pretty_print;

      // Names of the states.
      std::vector<std::string>* names;
    };
  } // namespace

  twa_graph_ptr
  remove_false_transitions(const twa_graph_ptr a)
  {
    auto res_ = make_twa_graph(a->get_dict());
    res_->copy_ap_of(a);
    for (unsigned state = 0; state < a->num_states(); ++state)
      res_->new_state();
    for (unsigned state = 0; state < a->num_states(); ++state)
      for (auto edge : a->out(state))
        if (edge.cond != bddfalse)
          res_->new_edge(state, edge.dst, edge.cond, edge.acc);
    res_->set_init_state(a->get_init_state_number());
    res_->set_acceptance(a->get_acceptance());
    return res_;
  }

  twa_graph_ptr
  car(const twa_graph_ptr &aut,
    bool search_ex,
    bool partial_degen,
    bool scc_acc_clean, bool parity_equiv, bool use_last, bool parity_prefix,
    bool rabin_to_buchi, bool pretty_print)
  {
    auto new_aut = remove_false_transitions(aut);
    return car_generator(new_aut, search_ex, partial_degen, scc_acc_clean,
             parity_equiv, use_last, parity_prefix,
             rabin_to_buchi, pretty_print)
           .run();
  }
} // namespace spot