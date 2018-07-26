// -*- coding: utf-8 -*-
// Copyright (C) 2018 Laboratoire de Recherche et Développement
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

#include "config.h"
#include <spot/twaalgos/toparity.hh>

#include <deque>
#include <map>

#include <sstream>

namespace spot
{
  namespace
  {
    // a class to store xor normal forms
    // a formula is in XNF if it is a xor of positive (Fin-less) formulas
    class xnf final
    {
      std::set<acc_cond::acc_code> terms;

      void insert(const acc_cond::acc_code& f)
      {
        if (terms.count(f))
          terms.erase(f);
        else
          terms.insert(f);
      }
    public:
      xnf() = default;
      xnf(const acc_cond::acc_code& f): xnf() { insert(f); }

      unsigned size() const { return terms.size(); }

      unsigned nb_sat(const acc_cond::mark_t& m) const
      {
        unsigned r = 0;
        for (const auto& phi : terms)
          if (phi.accepting(m))
            ++r;
        return r;
      }

      static xnf ff() { return xnf(); }
      static xnf tt() { return !ff(); }

      // not
      void neg()
      {
        insert(acc_cond::acc_code::t());
      }
      xnf operator!() const
      {
        auto res = *this;
        res.neg();
        return res;
      }

      // xor
      xnf operator+(const xnf& o) const
      {
        xnf res = *this;
        res += o;
        return res;
      }
      xnf& operator+=(const xnf& o)
      {
        for (const auto& f : o.terms)
          insert(f);
        return *this;
      }

      // and
      xnf operator&(const xnf& o) const
      {
        xnf res = *this;
        res &= o;
        return res;
      }
      xnf& operator&=(const xnf& o)
      {
        decltype(terms) tmp;
        std::swap(terms, tmp);

        for (const auto& f1 : tmp)
          for (const auto& f2 : o.terms)
            insert(f1 & f2);
        return *this;
      }

      // or
      xnf operator|(const xnf& o) const
      {
        xnf res = *this;
        res |= o;
        return res;
      }
      xnf& operator|=(const xnf& o)
      {
        *this += o & *this;
        *this += o;
        return *this;
      }

      std::string to_string() const
      {
        std::stringstream s;
        for (const auto& f : terms)
          s << f << " + ";
        return s.str();
      }
    };

    static xnf acc2xnf(const acc_cond::acc_code& acc)
    {
      auto marks = acc.used_inf_fin_sets();
      if (marks.second.count())
        {
          unsigned k = *marks.second.sets().begin();
          acc_cond::mark_t m({k});
          auto neg = acc2xnf(acc.remove(m, true));
          auto pos = acc2xnf(acc.remove(m, false));
          auto v = xnf(acc_cond::acc_code::inf(m));
          pos &= v;
          neg &= !v;
          return pos + neg;
        }
      return xnf(acc);
    }

    struct lar_state
    {
      unsigned state;
      std::vector<unsigned> perm;
      std::vector<unsigned> count;

      bool operator<(const lar_state& s) const
      {
        if (state == s.state)
          {
            if (perm == s.perm)
              return count < s.count;
            else
              return perm < s.perm;
          }
        else
          return state < s.state;
      }

      std::string to_string() const
      {
        std::stringstream s;
        s << state << " [";
        for (unsigned i = 0; i != perm.size(); ++i)
          {
            s << perm[i] << '@' << count[i];
            if (i < perm.size() - 1)
              s << ", ";
          }
        s << ']';

        return s.str();
      }
    };

    class lar_generator
    {
      const const_twa_graph_ptr& aut_;
      twa_graph_ptr res_;
      const bool pretty_print;

      // map lar_state to states in result automaton
      std::map<lar_state, unsigned> lar2num;
      // symmetry informations
      // we need to know for each symmetry class:
      //  - its id
      //  - its elements in order (any arbitrary order will do)
      //  - its size
      std::vector<acc_cond::mark_t> classes;
      std::vector<unsigned> acc2class;
      std::vector<unsigned> accpos;

      // algebraic normal form (xor of and's of positive litterals)
      const xnf accxnf;

      struct stack_elem
      {
        unsigned num;
        lar_state s;
      };
    public:
      explicit lar_generator(const const_twa_graph_ptr& a, bool pretty_print)
      : aut_(a)
      , res_(nullptr)
      , pretty_print(pretty_print)
      , acc2class(aut_->num_sets(), -1U)
      , accpos(aut_->num_sets(), -1U)
      , accxnf(acc2xnf(aut_->acc().get_acceptance()))
      {
        const auto symm = aut_->acc().get_acceptance().symmetries();
        for (unsigned k : aut_->acc().all_sets().sets())
          {
            unsigned r = symm[k];
            if (k == r) // new class
              {
                acc2class[k] = classes.size();
                accpos[k] = 0;
                classes.push_back({k});
              }
            else
              {
                unsigned c = acc2class[r];
                acc2class[k] = c;
                accpos[k] = classes[c].count();
                classes[c].set(k);
              }
          }
      }

      twa_graph_ptr run()
      {
        const bool even = aut_->acc().accepting(acc_cond::mark_t({}));
        // create resulting automaton
        res_ = make_twa_graph(aut_->get_dict());
        res_->copy_ap_of(aut_);

        std::deque<stack_elem> todo;
        auto get_state = [this, &todo](const lar_state& s)
          {
            auto it = lar2num.emplace(s, -1U);
            if (it.second) // insertion took place
              {
                unsigned nb = res_->new_state();
                it.first->second = nb;
                todo.push_back({nb, s});
              }
            return it.first->second;
          };

        // initial state
        {
          std::vector<unsigned> p0;
          std::vector<unsigned> c0;
          for (unsigned i = 0; i < classes.size(); ++i)
            {
              p0.push_back(i);
              c0.push_back(0);
            }
          lar_state s0{aut_->get_init_state_number(), p0, c0};
          unsigned init = get_state(s0); // put s0 in todo
          res_->set_init_state(init);
        }

        // main loop
        while (!todo.empty())
          {
            lar_state current = std::move(todo.front().s);
            unsigned src_num = todo.front().num;
            todo.pop_front();

            for (const auto& e : aut_->out(current.state))
              {
                // find the new permutation
                std::vector<unsigned> new_perm = current.perm;
                std::vector<unsigned> new_count = current.count;
                std::vector<unsigned> hits(new_count.size(), 0);
                unsigned h = 0;

                for (unsigned k : e.acc.sets())
                  {
                    unsigned c = acc2class[k];
                    if (accpos[k] == new_count[c])
                      new_count[c] += 1;
                    else
                      ++hits[c];
                    if (new_count[c] == classes[c].count())
                      {
                        new_count[c] = 0;
                        auto it =
                          std::find(new_perm.begin(), new_perm.end(), c);
                        assert(it != new_perm.end());
                        h = std::max(h, unsigned(new_perm.end() - it));
                        std::rotate(it, it+1, new_perm.end());
                      }
                  }

                lar_state dst{e.dst, new_perm, new_count};
                unsigned dst_num = get_state(dst);

                // do the h last elements satisfy the acceptance condition?
                acc_cond::mark_t m({});
                for (auto c = new_perm.end() - h; c != new_perm.end(); ++c)
                  m |= classes[*c];
                for (auto c = new_perm.begin(); c != new_perm.end() - h; ++c)
                  {
                    for (unsigned k : classes[*c].sets())
                      {
                        if (hits[*c] == 0)
                          break;
                        m.set(k);
                        --hits[*c];
                      }
                  }

                res_->new_edge(src_num, dst_num, e.cond,
                               {accxnf.nb_sat(m) - even});
              }
          }

        // parity max odd
        unsigned nb_colors = accxnf.size() + 1 - even;
        res_->set_acceptance(nb_colors,
            acc_cond::acc_code::parity(true, !even, nb_colors));

        // inherit properties of the input automaton
        res_->prop_copy(aut_, { false, false, true, false, true, true });

        if (pretty_print)
          {
            auto names = new std::vector<std::string>(res_->num_states());
            for (const auto& p : lar2num)
              (*names)[p.second] = p.first.to_string();
            res_->set_named_prop("state-names", names);
          }

        return res_;
      }

    };
  }

  twa_graph_ptr
  to_parity(const const_twa_graph_ptr& aut, bool pretty_print)
  {
    if (!aut->is_existential())
      throw std::runtime_error("LAR does not handle alternation");
    // if aut is already parity return it as is
    if (aut->acc().is_parity())
      return std::const_pointer_cast<twa_graph>(aut);

    lar_generator gen(aut, pretty_print);
    return gen.run();
  }

}
