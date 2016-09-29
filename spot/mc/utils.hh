// -*- coding: utf-8 -*-
// Copyright (C) 2016 Laboratoire de Recherche et
// Developpement de l'Epita
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

#include <spot/mc/reachability.hh>
#include <spot/mc/intersect.hh>
#include <spot/twa/twa.hh>
#include <spot/twacube_algos/convert.hh>

namespace spot
{
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class SPOT_API kripke_to_twa :
    public seq_reach_kripke<State, SuccIterator,
                            StateHash, StateEqual,
                            kripke_to_twa<State, SuccIterator,
                                          StateHash, StateEqual>>
  {
  public:
    kripke_to_twa(kripkecube<State, SuccIterator>& sys, bdd_dict_ptr dict)
      : seq_reach_kripke<State, SuccIterator, StateHash, StateEqual,
                         kripke_to_twa<State, SuccIterator,
                                       StateHash, StateEqual>>(sys),
      dict_(dict)
      {}

    ~kripke_to_twa()
      {
      }

    void setup()
    {
      res_ = make_twa_graph(dict_);
      names_ = new std::vector<std::string>();

      // padding to simplify computation.
      res_->new_state();

      // Compute the reverse binder.
      auto aps = this->sys_.get_ap();
      for (unsigned i = 0; i < aps.size(); ++i)
        {
          auto k = res_->register_ap(aps[i]);
          reverse_binder_.insert({i, k});
        }
    }

    void push(State s, unsigned i)
    {
      unsigned st = res_->new_state();
      names_->push_back(this->sys_.to_string(s));
      SPOT_ASSERT(st == i);
    }

    void edge(unsigned src, unsigned dst)
    {
      cubeset cs(this->sys_.get_ap().size());
      bdd cond = cube_to_bdd(this->todo.back().it->condition(),
                             cs, reverse_binder_);
      res_->new_edge(src, dst, cond);
    }

    void finalize()
    {
      res_->set_init_state(1);
      res_->purge_unreachable_states();
      res_->set_named_prop<std::vector<std::string>>("state-names", names_);
    }

    twa_graph_ptr twa()
    {
      return res_;
    }

  private:
    spot::twa_graph_ptr res_;
    std::vector<std::string>* names_;
    bdd_dict_ptr dict_;
    std::unordered_map<int, int> reverse_binder_;
  };



  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class SPOT_API product_to_twa :
    public intersect<State, SuccIterator,
                     StateHash, StateEqual,
                     product_to_twa<State, SuccIterator,
                                    StateHash, StateEqual>>
  {
    // Ease the manipulation
    using typename intersect<State, SuccIterator, StateHash, StateEqual,
                             product_to_twa<State, SuccIterator,
                                            StateHash,
                                            StateEqual>>::product_state;

  public:
    product_to_twa(kripkecube<State, SuccIterator>& sys,
                   twacube_ptr twa)
      : intersect<State, SuccIterator, StateHash, StateEqual,
                  product_to_twa<State, SuccIterator,
                                 StateHash, StateEqual>>(sys, twa)
      {
      }

    ~product_to_twa()
      {
      }

    twa_graph_ptr twa()
    {
      res_->set_named_prop<std::vector<std::string>>("state-names", names_);
      return res_;
    }

    void setup()
    {
      auto d = spot::make_bdd_dict();
      res_ = make_twa_graph(d);
      names_ = new std::vector<std::string>();

      int i = 0;
      for (auto ap : this->twa_->get_ap())
        {
          auto idx = res_->register_ap(ap);
          reverse_binder_[i++] = idx;
        }
    }

    bool push_state(product_state s, unsigned i, acc_cond::mark_t)
    {
      // push also implies edge (when it's not the initial state)
      if (this->todo.size())
        {
          auto c = this->twa_->get_cubeset()
            .intersection(this->twa_->trans_data
                          (this->todo.back().it_prop).cube_,
                          this->todo.back().it_kripke->condition());

          bdd x = spot::cube_to_bdd(c, this->twa_->get_cubeset(),
                                    reverse_binder_);
          this->twa_->get_cubeset().release(c);
          res_->new_edge(this->map[this->todo.back().st]-1, i-1, x,
                         this->twa_->trans_data
                         (this->todo.back().it_prop).acc_);
        }

      unsigned st = res_->new_state();
      names_->push_back(this->sys_.to_string(s.st_kripke) +
                        ('*' + std::to_string(s.st_prop)));
      SPOT_ASSERT(st+1 == i);
      return true;
    }


    bool update(product_state, unsigned src,
                product_state, unsigned dst,
                acc_cond::mark_t cond)
    {
      auto c = this->twa_->get_cubeset()
        .intersection(this->twa_->trans_data
                      (this->todo.back().it_prop).cube_,
                      this->todo.back().it_kripke->condition());

      bdd x = spot::cube_to_bdd(c, this->twa_->get_cubeset(),
                                reverse_binder_);
      this->twa_->get_cubeset().release(c);
      res_->new_edge(src-1, dst-1, x, cond);
      return false;
    }

    // These callbacks are useless here
    bool counterexample_found()
    {
      return false;
    }

    std::string trace()
    {
      return "";
    }

    bool pop_state(product_state, unsigned, bool, product_state, unsigned)
    {
      return true;
    }

  private:
    spot::twa_graph_ptr res_;
    std::vector<std::string>* names_;
    std::unordered_map<int, int> reverse_binder_;
  };
}
