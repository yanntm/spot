// -*- coding: utf-8 -*-
// Copyright (C) 2015, 2016 Laboratoire de Recherche et
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

#include <spot/twa/acc.hh>
#include <spot/mc/unionfind.hh>
#include <spot/mc/intersect.hh>

namespace spot
{
  /// \brief This class implements the sequential emptiness check as
  /// presented in "Three SCC-based Emptiness Checks for Generalized
  /// B\¨uchi Automata" (Renault et al, LPAR 2013). Among the three
  /// emptiness check that has been proposed we opted to implement
  /// the Gabow's one.
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class ec_renault13lpar  : public intersect<State, SuccIterator,
                                       StateHash, StateEqual,
                                       ec_renault13lpar<State, SuccIterator,
                                                        StateHash, StateEqual>>
  {
    // Ease the manipulation
    using typename intersect<State, SuccIterator, StateHash, StateEqual,
                             ec_renault13lpar<State, SuccIterator,
                                              StateHash,
                                              StateEqual>>::product_state;

  public:
    ec_renault13lpar(kripkecube<State, SuccIterator>& sys,
                     twacube_ptr twa, unsigned tid, bool stop)
      : intersect<State, SuccIterator, StateHash, StateEqual,
                  ec_renault13lpar<State, SuccIterator,
                                   StateHash, StateEqual>>(sys, twa, tid, stop),
      acc_(twa->acc()), sccs_(0U)
      {
      }

    virtual ~ec_renault13lpar()
    {

    }

    /// \brief This method is called at the begining of the exploration.
    /// here we do not need to setup any information.
    void setup()
    {
    }

    /// \brief This method is called to notify the emptiness checks
    /// that a new state has been discovered. If this method return
    /// false, the state will not be explored. The parameter \a dfsnum
    /// specify an unique id for the state \a s. Parameter \a cond represents
    /// The value on the ingoing edge to \a s.
    bool push_state(product_state, unsigned dfsnum, acc_cond::mark_t cond)
    {
      uf_.makeset(dfsnum);
      roots_.push_back({dfsnum, cond, 0U});
      return true;
    }

    /// \brief This method is called to notify the emptiness checks
    /// that a state will be popped. If the method return false, then
    /// the state will be popped. Otherwise the state \a newtop will
    /// become the new top of the DFS stack. If the state \a top is
    /// the only one in the DFS stak, the parameter \a is_initial is set
    /// to true and both \a newtop and \a  newtop_dfsnum have inconsistency
    /// values.
    bool pop_state(product_state, unsigned top_dfsnum, bool,
                   product_state, unsigned)
    {
      if (top_dfsnum == roots_.back().dfsnum)
        {
          roots_.pop_back();
          ++sccs_;
          uf_.markdead(top_dfsnum);
        }
      dfs_ = this->todo.size()  > dfs_ ? this->todo.size() : dfs_;
      return true;
    }

    /// \brief This method is called for every closing, back, or forward edge.
    /// Return true if a counterexample has been found.
    bool update(product_state, unsigned,
                product_state, unsigned dst_dfsnum,
                acc_cond::mark_t cond)
    {
      if (uf_.isdead(dst_dfsnum))
        return false;

      while (!uf_.sameset(dst_dfsnum, roots_.back().dfsnum))
        {
          auto& el = roots_.back();
          roots_.pop_back();
          uf_.unite(dst_dfsnum, el.dfsnum);
          cond |= el.acc | el.ingoing;
        }
      roots_.back().acc |= cond;
      found_ = acc_.accepting(roots_.back().acc);
      if (SPOT_UNLIKELY(found_))
        this->stop_ = true;
      return found_;
    }

    bool counterexample_found()
    {
      return found_;
    }

    std::string trace()
    {
      SPOT_ASSERT(counterexample_found());
      std::string res = "Prefix:\n";

       // Compute the prefix of the accepting run
      for (auto& s : this->todo)
        res += "  " + std::to_string(s.st.st_prop) +
          + "*" + this->sys_.to_string(s.st.st_kripke) + "\n";

      // Compute the accepting cycle
      res += "Cycle:\n";

      struct ctrx_element
      {
        const product_state* prod_st;
        ctrx_element* parent_st;
        SuccIterator* it_kripke;
        std::shared_ptr<trans_index> it_prop;
      };
      std::queue<ctrx_element*> bfs;

      acc_cond::mark_t acc = 0U;

      bfs.push(new ctrx_element({&this->todo.back().st, nullptr,
              this->sys_.succ(this->todo.back().st.st_kripke, this->tid_),
              this->twa_->succ(this->todo.back().st.st_prop)}));
      while (true)
        {
        here:
          auto* front = bfs.front();
          bfs.pop();
          // PUSH all successors of the state.
          while (!front->it_kripke->done())
            {
              while (!front->it_prop->done())
                {
                  if (this->twa_->get_cubeset().intersect
                      (this->twa_->trans_data(front->it_prop, this->tid_).cube_,
                       front->it_kripke->condition()))
                    {
                      const product_state dst = {
                        front->it_kripke->state(),
                        this->twa_->trans_storage(front->it_prop).dst
                      };

                      // Skip Unknown states or not same SCC
                      auto it  = this->map.find(dst);
                      if (it == this->map.end() ||
                          !uf_.sameset(it->second,
                                       this->map[this->todo.back().st]))
                        {
                          front->it_prop->next();
                          continue;
                        }

                      // This is a valid transition. If this transition
                      // is the one we are looking for, update the counter-
                      // -example and flush the bfs queue.
                      auto mark = this->twa_->trans_data(front->it_prop,
                                                         this->tid_).acc_;
                      if (!acc.has(mark.id))
                        {
                          ctrx_element* current = front;
                          while (current != nullptr)
                            {
                              // FIXME also display acc?
                              res = res + "  " +
                                std::to_string(current->prod_st->st_prop) +
                                + "*" +
                                this->sys_. to_string(current->prod_st
                                                      ->st_kripke) +
                                "\n";
                              current = current->parent_st;
                            }

                          // empty the queue
                          while (!bfs.empty())
                            {
                              auto* e = bfs.front();
                              bfs.pop();
                              delete e;
                            }

                          // update acceptance
                          acc |= mark;
                          if (this->twa_->acc().accepting(acc))
                            return res;

                          const product_state* q = &(it->first);
                          ctrx_element* root = new ctrx_element({
                              q , nullptr,
                              this->sys_.succ(q->st_kripke, this->tid_),
                              this->twa_->succ(q->st_prop)
                          });
                          bfs.push(root);
                          goto here;
                        }

                      // Otherwise increment iterator and push successor.
                      const product_state* q = &(it->first);
                      ctrx_element* root = new ctrx_element({
                          q , nullptr,
                          this->sys_.succ(q->st_kripke, this->tid_),
                          this->twa_->succ(q->st_prop)
                      });
                      bfs.push(root);
                    }
                  front->it_prop->next();
                }
              front->it_prop->reset();
              front->it_kripke->next();
            }
        }

      // never reach here;
      return res;
    }

    virtual istats stats() override
    {
      return {this->states(), this->trans(), sccs_,
          (unsigned) roots_.size(), dfs_, found_};
    }

  private:

    bool found_ = false;        ///< \brief A counterexample is detected?

    struct root_element {
      unsigned dfsnum;
      acc_cond::mark_t ingoing;
      acc_cond::mark_t acc;
    };

    /// \brief the root stack.
    std::vector<root_element> roots_;
    int_unionfind uf_;
    acc_cond acc_;
    unsigned sccs_;
    unsigned dfs_;
  };
}
