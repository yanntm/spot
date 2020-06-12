// -*- coding: utf-8 -*-
// Copyright (C) 2017, 2018, 2020 Laboratoire de Recherche et Développement
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

#include "config.h"

#include <fstream>
#include <string>
#include <sstream>

#include <cmath>
#include <spot/misc/game.hh>
#include <spot/misc/bddlt.hh>
#include <spot/twaalgos/sbacc.hh>
#include <spot/twaalgos/parity.hh>
#include <spot/twaalgos/sccinfo.hh>

namespace
{
  using namespace spot;

  bool is_arena(const twa_graph_ptr &arena)
  {
    if (!arena->get_named_prop<std::vector<bool>>("state-player"))
      throw std::runtime_error("States have no owners!");
    if (!arena->get_named_prop<std::vector<bool>>("winning-region")
        || !arena->get_named_prop<std::vector<unsigned>>("strategy"))
      throw std::runtime_error("Arena must have winning region and strategy!");
    return true;
  }

  bool is_solved(const twa_graph_ptr &arena)
  {
    is_arena(arena);
    unsigned n_states = arena->num_states();
    std::vector<bool>& owner =
        *arena->get_named_prop<std::vector<bool>>("state-player");
    std::vector<bool>& winner =
        *arena->get_named_prop<std::vector<bool>>("winning-region");
    std::vector<unsigned>& strategy =
        *arena->get_named_prop<std::vector<unsigned>>("strategy");
    if ((n_states != owner.size())
        || (n_states != winner.size())
        || (n_states != strategy.size()))
      throw std::runtime_error("Arena has inconsistent sizes");
    // We need a strategy for all owned and winning states
    // If a local solver is used, states that have not been visited
    // should be marked as winning for the env
    for (unsigned v = 0; v < n_states; v++)
      if (owner[v] && winner[v])
        assert((strategy[v] > 0) && (strategy[v] <= arena->num_edges()));
    return true;
  }

  bool check_arena(const twa_graph_ptr &arena)
  {
    is_arena(arena);
    std::vector<bool>* owner_ptr =
        arena->get_named_prop < std::vector<bool>>("state-player");
    if (owner_ptr->size() != arena->num_states())
      throw std::runtime_error("More or less owners than states");
    std::vector<bool>& owner = *owner_ptr;
    bool max, odd;
    arena->acc().is_parity(max, odd, true);
    if (!(max && odd))
      throw std::runtime_error("arena must have max-odd acc. condition");
    for (const auto& e : arena->edges())
    {
      if (e.acc.max_set() == 0)
        throw std::runtime_error("arena must be colorized");
      if ((owner[e.src] == owner[e.dst]))
        {
          std::cerr << owner[e.src] <<" : " << e.src << ", " << e.dst <<
                   "; " << arena->num_states()<<std::endl;
          throw std::runtime_error("Inconsistent arena");
        }
    }
    for (unsigned v = 0; v < arena->num_states(); ++v)
      if (arena->out(v).begin() == arena->out(v).end())
        throw std::runtime_error("Not all states have successors!");
    return true;
  }

  // Internal structs
  // winning regions for env and player
  struct winner_t
  {
    std::vector<bool> has_winner_;
    std::vector<bool> winner_;

    inline bool operator()(unsigned v, bool p)
    {
      // returns true if player p wins v
      // false otherwise
      if (!has_winner_[v])
        return false;

      return winner_[v] == p;
    }

    inline void set(unsigned v, bool p)
    {
      has_winner_[v] = true;
      winner_[v] = p;
    }

    inline void unset(unsigned v)
    {
      has_winner_[v] = false;
    }

    inline bool winner(unsigned v)
    {
      assert(has_winner_.at(v));
      return winner_[v];
    }
  }; // winner_t

  // When using scc decomposition we need to track the
  // changes made to the graph
  struct edge_stash_t
  {
    edge_stash_t(unsigned num, unsigned dst, acc_cond::mark_t acc) noexcept
        : e_num(num),
          e_dst(dst),
          e_acc(acc)
    {
    }
    const unsigned e_num, e_dst;
    const acc_cond::mark_t e_acc;
  }; // edge_stash_t

  // Internal structs used by parity_game
  // Struct to change recursive calls to stack
  struct work_t
  {
    work_t(unsigned wstep_, unsigned rd_, unsigned min_par_,
           unsigned max_par_) noexcept
        : wstep(wstep_),
          rd(rd_),
          min_par(min_par_),
          max_par(max_par_)
    {
    }
    const unsigned wstep, rd, min_par, max_par;
  }; // work_t

  // Collects information about an scc
  // Used to detect special cases
  struct subgame_info_t
  {
    typedef std::set<unsigned, std::greater<unsigned>> all_parities_t;

    subgame_info_t() noexcept
    {
    }

    subgame_info_t(bool empty, bool one_parity, bool one_player0,
                   bool one_player1, all_parities_t parities) noexcept
        : is_empty(empty),
          is_one_parity(one_parity),
          is_one_player0(one_player0),
          is_one_player1(one_player1),
          all_parities(parities)
    {};
    bool is_empty; // empty subgame
    bool is_one_parity; // only one parity appears in the subgame
    // todo : Not used yet
    bool is_one_player0; // one player subgame for player0 <-> p==false
    bool is_one_player1; // one player subgame for player1 <-> p==true
    all_parities_t all_parities;
  }; // subgame_info_t

  // false -> env, true -> player
  typedef std::vector<bool> region_t;
  // state idx -> global edge number
  typedef std::vector<unsigned> strategy_t;

  // A class to solve parity games
  // The current implementation is inspired by
  // by oink however without multicore and adapted to transition based
  // acceptance
  class parity_game
  {
  public:

    bool solve(const twa_graph_ptr &arena)
    {
      assert(check_arena(arena));
      region_t* w_ptr = arena->get_named_prop<region_t>("winning-region");
      strategy_t* s_ptr = arena->get_named_prop<strategy_t>("strategy");
      if (!w_ptr || !s_ptr
          || !arena->get_named_prop<std::vector<bool>>("state-player"))
        throw std::runtime_error("Arena missing winning-region, strategy "
                                 "or state-player");
      // todo check if reordering states according to scc is worth it
      set_up(arena);
      // Start recursive zielonka in a bottom-up fashion on each scc
      subgame_info_t subgame_info;
      for (c_scc_idx_ = 0; c_scc_idx_ < info_->scc_count(); ++c_scc_idx_)
        {
          // Convert transitions leaving edges to self-loops
          // and check if trivially solvable
          subgame_info = fix_scc();
          // If empty, the scc was trivially solved
          if (!subgame_info.is_empty)
            {
              // Check for special cases
              if (subgame_info.is_one_parity)
                one_par_subgame_solver(subgame_info, max_abs_par_);
              else
                {
                  // "Regular" solver
                  max_abs_par_ = *subgame_info.all_parities.begin();
                  w_stack_.emplace_back(0, 0, 0, max_abs_par_);
                  zielonka();
                }
            }
        }
      // All done -> restore graph, i.e. undo self-looping
      restore();

      assert(std::all_of(w_.has_winner_.cbegin(), w_.has_winner_.cend(),
                         [](bool b)
                         { return b; }));
      assert(std::all_of(s_.cbegin(), s_.cend(),
                         [](unsigned e_idx)
                         { return e_idx > 0; }));

      // Put the solution as name variable
      region_t &w = *w_ptr;
      strategy_t &s = *s_ptr;
      w.swap(w_.winner_);
      s.resize(s_.size());
      std::copy(s_.begin(), s_.end(), s.begin());

      clean_up();
      return w[arena->get_init_state_number()];
    }

  private:

    // Returns the vector of states currently considered
    // i.e. the states of the current scc
    // c_scc_idx_ is set in the 'main' loop
    inline const std::vector<unsigned> &c_states()
    {
      assert(info_);
      return info_->states_of(c_scc_idx_);
    }

    void set_up(const twa_graph_ptr &arena)
    {
      assert(is_arena(arena));

      owner_ptr_ =
          arena->get_named_prop<std::vector<bool>>("state-player");

      if (!arena->get_named_prop<region_t>("winning-region")
          || !arena->get_named_prop<strategy_t>("strategy")
          || !owner_ptr_)
        throw std::runtime_error("Arena missing winning-region, strategy "
                                 "or state-player");
      arena_ = arena;

      unsigned n_states = arena_->num_states();
      // Resize data structures
      subgame_.clear();
      subgame_.resize(n_states, unseen_mark);
      w_.has_winner_.clear();
      w_.has_winner_.resize(n_states, 0);
      w_.winner_.clear();
      w_.winner_.resize(n_states, 0);
      s_.clear();
      s_.resize(n_states, -1);
      // Init
      rd_ = 0;
      max_abs_par_ = arena_->get_acceptance().used_sets().max_set()-1;
      info_ = std::make_unique<scc_info>(arena_);
      // Every edge leaving an scc needs to be "fixed"
      // at some point.
      // We store: number of edge fixed, original dst, original acc
      change_stash_.clear();
      change_stash_.reserve(info_->scc_count() * 2);
    }

    // Checks if an scc is empty and reports the occurring parities
    // or special cases
    inline subgame_info_t
    inspect_scc(unsigned max_par)
    {
      subgame_info_t info;
      info.is_empty = true;
      info.is_one_player0 = true;
      info.is_one_player1 = true;
      for (unsigned v : c_states())
        {
          if (subgame_[v] != unseen_mark)
            continue;
          info.is_empty = false;

          bool multi_edge = false;
          for (const auto &e : arena_->out(v))
            if (subgame_[e.dst] == unseen_mark)
              {
                unsigned this_par = e.acc.max_set()-1;
                if (this_par <= max_par)
                  {
                    info.all_parities.insert(this_par);
                    multi_edge = true;
                  }
              }
          if (multi_edge)
            {
              // This state has multiple edges, so it is not
              // a one player subgame for !owner
              if ((*owner_ptr_)[v])
                info.is_one_player1 = false;
              else
                info.is_one_player0 = false;
            }
        } // v
      assert(info.all_parities.size()
             || (!info.all_parities.size() && info.is_empty));
      info.is_one_parity = info.all_parities.size() == 1;
      // Done
      return info;
    }

    // Checks if an scc can be trivially solved,
    // that is, all vertices of the scc belong to the
    // attractor of a transition leaving the scc
    inline subgame_info_t
    fix_scc()
    {
      auto scc_acc = info_->acc_sets_of(c_scc_idx_);
      // We will override all parities of edges leaving the scc
      bool added[] = {false, false};
      unsigned par_pair[2];
      unsigned scc_new_par = std::max(scc_acc.max_set(), 1u);
      if (scc_new_par&1)
        {
          par_pair[1] = scc_new_par;
          par_pair[0] = scc_new_par+1;
        }
      else
        {
          par_pair[1] = scc_new_par+1;
          par_pair[0] = scc_new_par;
        }
      acc_cond::mark_t even_mark({par_pair[0]});
      acc_cond::mark_t odd_mark({par_pair[1]});

      // Only necessary to pass tests
      max_abs_par_ = std::max(par_pair[0], par_pair[1]);

      for (unsigned v : c_states())
        {
          assert(subgame_[v] == unseen_mark);
          for (auto &e : arena_->out(v))
            {
              // The outgoing edges are taken finitely often
              // -> disregard parity
              if (subgame_[e.dst] != unseen_mark)
                {
                  // Edge leaving the scc
                  change_stash_.emplace_back(arena_->edge_number(e),
                                             e.dst, e.acc);
                  if (w_.winner(e.dst))
                    {
                      // Winning region of player -> odd
                      e.acc = odd_mark;
                      added[1] = true;
                    }
                  else
                    {
                      // Winning region of env -> even
                      e.acc = even_mark;
                      added[0] = true;
                    }
                  // Replace with self-loop
                  e.dst = e.src;
                }
            } // e
        } // v

      // Compute the attractors of the self-loops/transitions leaving scc
      // These can be directly added to the winning states
      // Note: attractors can not intersect therefore the order in which
      // they are computed does not matter
      unsigned dummy_rd;

      for (bool p : {false, true})
        if (added[p])
          attr(dummy_rd, p, par_pair[p], true, par_pair[p]);

      if (added[0] || added[1])
        // Fix "negative" strategy
        for (unsigned v : c_states())
          if (subgame_[v] != unseen_mark)
            s_[v] = std::abs(s_[v]);

      return inspect_scc(unseen_mark);
    } // fix_scc

    inline bool
    attr(unsigned &rd, bool p, unsigned max_par,
         bool acc_par, unsigned min_win_par)
    {
      // Computes the attractor of the winning set of player p within a
      // subgame given as rd.
      // If acc_par is true, max_par transitions are also accepting and
      // the subgame count will be increased
      // The attracted vertices are directly added to the set

      // Increase rd meaning we create a new subgame
      if (acc_par)
        rd = ++rd_;
      // todo replace with a variant of topo sort ?
      // As proposed in Oink! / PGSolver
      // Needs the transposed graph however

      assert((!acc_par) || (acc_par && (max_par&1) == p));
      assert(!acc_par || (0 < min_win_par));
      assert((min_win_par <= max_par) && (max_par <= max_abs_par_));

      bool grown = false;
      // We could also directly mark states as owned,
      // instead of adding them to to_add first,
      // possibly reducing the number of iterations.
      // However by making the algorithm complete a loop
      // before adding it is like a backward bfs and (generally) reduces
      // the size of the strategy
      static std::vector<unsigned> to_add;

      assert(to_add.empty());

      do
        {
          if (!to_add.empty())
            {
              grown = true;
              for (unsigned v : to_add)
                {
                  // v is winning
                  w_.set(v, p);
                  // Mark if demanded
                  if (acc_par)
                    {
                      assert(subgame_[v] == unseen_mark);
                      subgame_[v] = rd;
                    }
                }
            }
          to_add.clear();

          for (unsigned v : c_states())
            {
              if ((subgame_[v] < rd) || (w_(v, p)))
                // Not in subgame or winning
                continue;

              bool is_owned = (*owner_ptr_)[v] == p;
              bool wins = !is_owned;
              // Loop over out-going

              // Optim: If given the choice,
              // we seek to go to the "oldest" subgame
              // That is the subgame with the lowest rd value
              unsigned min_subgame_idx = -1u;
              for (const auto &e: arena_->out(v))
                {
                  unsigned this_par = e.acc.max_set()-1;
                  if ((subgame_[e.dst] >= rd) && (this_par <= max_par))
                    {
                      // Check if winning
                      if (w_(e.dst, p)
                          || (acc_par && (min_win_par <= this_par)))
                        {
                          assert(!acc_par || (this_par < min_win_par) ||
                                 (acc_par && (min_win_par <= this_par) &&
                                  ((this_par&1) == p)));
                          if (is_owned)
                            {
                              wins = true;
                              if (acc_par)
                                {
                                  s_[v] = arena_->edge_number(e);
                                  if (min_win_par <= this_par)
                                    // max par edge
                                    // change sign -> mark as needs
                                    // to be possibly fixed
                                    s_[v] = -s_[v];
                                  break;
                                }
                              else
                                {
                                  //snapping
                                  if (subgame_[e.dst] < min_subgame_idx)
                                    {
                                      s_[v] = arena_->edge_number(e);
                                      min_subgame_idx = subgame_[e.dst];
                                      if (!p)
                                        // No optim for env
                                        break;
                                    }
                                }
                            }// owned
                        }
                      else
                        {
                          if (!is_owned)
                            {
                              wins = false;
                              break;
                            }
                        } // winning
                    } // subgame
                }// for edges
              if (wins)
                to_add.push_back(v);
            } // for v
        } while (!to_add.empty());
      // done

      assert(to_add.empty());
      return grown;
    }

    // We need to check if transitions that are accepted due
    // to their parity remain in the winning region of p
    inline bool
    fix_strat_acc(unsigned rd, bool p, unsigned min_win_par, unsigned max_par)
    {
      for (unsigned v : c_states())
        {
          // Only current attractor and owned
          // and winning vertices are concerned
          if ((subgame_[v] != rd) || !w_(v, p)
              || ((*owner_ptr_)[v] != p) || (s_[v] > 0))
            continue;

          // owned winning vertex of attractor
          // Get the strategy edge
          s_[v] = -s_[v];
          const auto &e_s = arena_->edge_storage(s_[v]);
          // Optimization only for player
          if (!p && w_(e_s.dst, p))
            // If current strat is admissible -> nothing to do
            // for env
            continue;

          // This is an accepting edge that is no longer admissible
          // or we seek a more desirable edge (for player)
          assert(min_win_par <= e_s.acc.max_set()-1);
          assert(e_s.acc.max_set()-1 <= max_par);

          // Strategy heuristic : go to the oldest subgame
          unsigned min_subgame_idx = -1u;

          s_[v] = -1;
          for (const auto &e_fix : arena_->out(v))
            {
              if (subgame_[e_fix.dst] >= rd)
                {
                  unsigned this_par = e_fix.acc.max_set()-1;
                  // This edge must have less than max_par,
                  // otherwise it would have already been attracted
                  assert((this_par <= max_par) || ((this_par&1) != (max_par&1)));
                  // if it is accepting and leads to the winning region
                  // -> valid fix
                  if ((min_win_par <= this_par)
                      && (this_par <= max_par)
                      && w_(e_fix.dst, p)
                      && (subgame_[e_fix.dst] < min_subgame_idx))
                    {
                      // Max par edge to older subgame found
                      s_[v] = arena_->edge_number(e_fix);
                      min_subgame_idx = subgame_[e_fix.dst];
                    }
                }
            }
          if (s_[v] == -1)
            // NO fix found
            // This state is NOT won by p due to any accepting edges
            return true; // true -> grown
        }
      // Nothing to fix or all fixed
      return false; // false -> not grown == all good
    }

    inline void zielonka()
    {
      while (!w_stack_.empty())
        {
          auto this_work = w_stack_.back();
          w_stack_.pop_back();

          switch (this_work.wstep)
            {
              case (0):
                {
                  assert(this_work.rd == 0);
                  assert(this_work.min_par == 0);

                  unsigned rd;
                  assert(this_work.max_par <= max_abs_par_);

                  // Check if empty and get parities
                  subgame_info_t subgame_info =
                      inspect_scc(this_work.max_par);

                  if (subgame_info.is_empty)
                    // Nothing to do
                    break;
                  if (subgame_info.is_one_parity)
                    {
                      // Can be trivially solved
                      one_par_subgame_solver(subgame_info, this_work.max_par);
                      break;
                    }

                  // Compute the winning parity boundaries
                  // -> Priority compression
                  // Optional, improves performance
                  // Highest actually occurring
                  unsigned max_par = *subgame_info.all_parities.begin();
                  unsigned min_win_par = max_par;
                  while ((min_win_par > 2) &&
                         (!subgame_info.all_parities.count(min_win_par-1)))
                    min_win_par -= 2;
                  assert(max_par > 0);
                  assert(!subgame_info.all_parities.empty());
                  assert(min_win_par > 0);

                  // Get the player
                  bool p = min_win_par&1;
                  assert((max_par&1) == (min_win_par&1));
                  // Attraction to highest par
                  // This increases rd_ and passes it to rd
                  attr(rd, p, max_par, true, min_win_par);
                  // All those attracted get subgame_[v] <- rd

                  // Continuation
                  w_stack_.emplace_back(1, rd, min_win_par, max_par);
                  // Recursion
                  w_stack_.emplace_back(0, 0, 0, min_win_par-1);
                  // Others attracted will have higher counts in subgame
                  break;
                }
              case (1):
                {
                  unsigned rd = this_work.rd;
                  unsigned min_win_par = this_work.min_par;
                  unsigned max_par = this_work.max_par;
                  assert((min_win_par&1) == (max_par&1));
                  bool p = min_win_par&1;
                  // Check if the attractor of w_[!p] is equal to w_[!p]
                  // if so, player wins if there remain accepting transitions
                  // for max_par (see fix_strat_acc)
                  // This does not increase but reuse rd
                  bool grown = attr(rd, !p, max_par, false, min_win_par);
                  // todo investigate: A is an attractor, so the only way that
                  // attr(w[!p]) != w[!p] is if the max par "exit" edges lead
                  // to a trap for player/ exit the winning region of the
                  // player so we can do a fast check instead of the
                  // generic attr computation which only needs to be done
                  // if the fast check is positive

                  // Check if strategy needs to be fixed / is fixable
                  if (!grown)
                    // this only concerns parity accepting edges
                    grown = fix_strat_acc(rd, p, min_win_par, max_par);
                  // If !grown we are done, and the partitions are valid

                  if (grown)
                    {
                      // Reset current game without !p attractor
                      for (unsigned v : c_states())
                        if (!w_(v, !p) && (subgame_[v] >= rd))
                          {
                            // delete ownership
                            w_.unset(v);
                            // Mark as unseen
                            subgame_[v] = unseen_mark;
                            // Unset strat for testing
                            s_[v] = -1;
                          }
                      w_stack_.emplace_back(0, 0, 0, max_par);
                      // No need to do anything else
                      // the attractor of !p of this level is not changed
                    }
                  break;
                }
              default:
                throw std::runtime_error("No valid workstep");
            } // switch
        } // while
    } // zielonka

    // Undo change to the graph made along the way
    inline void restore()
    {
      // "Unfix" the edges leaving the sccs
      // This is called once the game has been solved
      for (auto &e_stash : change_stash_)
        {
          auto &e = arena_->edge_storage(e_stash.e_num);
          e.dst = e_stash.e_dst;
          e.acc = e_stash.e_acc;
        }
      // Done
    }

    // Empty all internal variables
    inline void clean_up()
    {
      info_ = nullptr;
      subgame_.clear();
      w_.has_winner_.clear();
      w_.winner_.clear();
      s_.clear();
      rd_ = 0;
      max_abs_par_ = 0;
      change_stash_.clear();
    }

    // Dedicated solver for special cases
    inline void one_par_subgame_solver(const subgame_info_t &info,
                                       unsigned max_par)
    {
      assert(info.all_parities.size() == 1);
      // The entire subgame is won by the player of the only parity
      // Any edge will do
      // todo optim for smaller circuit
      // This subgame gets its own counter
      ++rd_;
      unsigned rd = rd_;
      unsigned one_par = *info.all_parities.begin();
      bool winner = one_par & 1;
      assert(one_par <= max_par);

      for (unsigned v : c_states())
      {
        if (subgame_[v] != unseen_mark)
          continue;
        // State of the subgame
        subgame_[v] = rd;
        w_.set(v, winner);
        // Get the strategy
        assert(s_[v] == -1);
        for (const auto &e : arena_->out(v))
        {
          unsigned this_par = e.acc.max_set()-1;
          if ((subgame_[e.dst] >= rd) && (this_par <= max_par))
          {
            assert(this_par == one_par);
            // Ok for strat
            s_[v] = arena_->edge_number(e);
            break;
          }
        }
        assert((0 < s_[v]) && (s_[v] < unseen_mark));
      }
      // Done
    }
    // todo one player arena

    const unsigned unseen_mark = std::numeric_limits<unsigned>::max();

    twa_graph_ptr arena_;
    const std::vector<bool> *owner_ptr_;
    unsigned rd_;
    winner_t w_;
    // Subgame array similar to the one from oink!
    std::vector<unsigned> subgame_;
    // strategies for env and player; For synthesis only player is needed
    // We need a signed value here in order to "fix" the strategy
    // during construction
    std::vector<long long> s_;

    // Informations about sccs andthe current scc
    std::unique_ptr<scc_info> info_;
    unsigned max_abs_par_; // Max parity occurring in the current scc
    // Info on the current scc
    unsigned c_scc_idx_;
    // Fixes made to the sccs that have to be undone
    // before returning
    std::vector<edge_stash_t> change_stash_;
    // Change recursive calls to stack
    std::vector<work_t> w_stack_;
  };

}

namespace spot
{

  void
  make_arena(twa_graph_ptr& arena, bdd outs)
  {
    auto um = arena->acc().unsat_mark();
    if (!um.first)
      throw std::runtime_error("game winning condition is a tautology");

    bdd* outs_ptr = arena->get_named_prop<bdd>("synthesis-output");

    if (outs_ptr && (outs != *outs_ptr))
      throw std::runtime_error("Inconsistent definition of output ap");

    if (!outs_ptr)
      arena->set_named_prop("synthesis-output", new bdd(outs));


    std::vector<bool> seen(arena->num_states(), false);
    std::vector<unsigned> todo({arena->get_init_state_number()});
    std::vector<bool> owner(arena->num_states()+10, false);
    // +10 Is to avoid reallocation and takes into account the
    // two sinks and the first 8 self-loops (self-loops only occur
    // in sd and they are "rare"
    // Note that sinks and intermediate states are never pushed back,
    // so "seen" only needs to account for the original states
    owner[arena->get_init_state_number()] = false;

    unsigned sink_env=0, sink_con=0;

    auto create_sink = [&]()
      {
        if (sink_env == 0)
        {
          sink_env = arena->new_state();
          sink_con = arena->new_state();
          arena->new_edge(sink_con, sink_env, bddtrue, um.second);
          arena->new_edge(sink_env, sink_con, bddtrue, um.second);
          owner.at(sink_env) = false;
          owner.at(sink_con) = true;
        }
      };

    while (!todo.empty())
      {
        unsigned src = todo.back();
        todo.pop_back();
        seen[src] = true;
        bdd missing = bddtrue;
        for (auto &e: arena->out(src))
          {
            // Owner is only required to have one successor
            // env must be complete
            // todo what is the exact reason for this?
            // Zielonka does not depend on completeness (there is no notion
            // of the bdd ) but without completeness one gets incorrect results
            // even for ds in the case of unreal specifications
            if (owner[src])
              missing = bddfalse;
            else
              missing -= e.cond;
            if (!seen[e.dst])
              {
                owner[e.dst] = !owner[src];
                todo.push_back(e.dst);
              }
            else if (e.dst == e.src)
              {
                // Self-loops can occur when using sd
                // Split into new state and edge
                if (arena->num_states() == owner.size())
                  owner.resize(owner.size()+owner.size()/5+1, false);
                unsigned intermed = arena->new_state();
                e.dst = intermed;
                arena->new_edge(intermed, e.src, bddtrue, e.acc);
                owner.at(intermed) = !owner[e.src];
              }
            else
              assert((owner[e.dst] != owner[src])
                          && "Illformed arena!");
          } // edges
        if (owner[src] && (missing == bddtrue))
          {
            // The owned state has no successors
            // Create edge to sink_env
            create_sink();
            arena->new_edge(src, sink_env, bddtrue, um.second);
          }
        if (!owner[src] && (missing != bddfalse))
          {
            create_sink();
            arena->new_edge(src, sink_con, missing, um.second);
          }
      }
    owner.resize(arena->num_states());
    // set them in the aut
    arena->set_named_prop("state-player",
                          new std::vector<bool>(std::move(owner)));
    // Set also the strategy and the winning region without filling them
    arena->set_named_prop("winning-region",
                          new std::vector<bool>());
    arena->set_named_prop("strategy",
                          new std::vector<unsigned>());
    // Empty winning region and strategy stand for unsolved
  }

  bool solve_parity_game(const twa_graph_ptr& arena)
  {
    parity_game pg;
    return pg.solve(arena);
  }

  twa_graph_ptr
  apply_strategy(const twa_graph_ptr& arena,
                 bool unsplit, bool keep_acc, bool leave_choice)
  {
    assert(is_solved(arena));

    bdd* all_outputs_ptr = arena->get_named_prop<bdd>("synthesis-output");
    bdd all_outputs = all_outputs_ptr ? *all_outputs_ptr : bdd(bddfalse);

    region_t* w_ptr =
        arena->get_named_prop<region_t>("winning-region");
    strategy_t* s_ptr =
        arena->get_named_prop<strategy_t>("strategy");
    std::vector<bool>* sp_ptr =
        arena->get_named_prop<std::vector<bool>>("state-player");

    if (!w_ptr || !s_ptr || !sp_ptr)
      throw std::runtime_error("Arena missing winning-region, strategy "
                               "or state-player");

    if (!(w_ptr->at(arena->get_init_state_number())))
      throw std::runtime_error("Player does not win initial state, strategy "
                               "is not applicable");

    region_t& w = *w_ptr;
    strategy_t& s = *s_ptr;

    auto aut = spot::make_twa_graph(arena->get_dict());
    aut->copy_ap_of(arena);
    if (keep_acc)
      aut->copy_acceptance_of(arena);
    else
      aut->set_acceptance(0, acc_cond::acc_code());

    const unsigned unseen_mark = std::numeric_limits<unsigned>::max();
    std::vector<unsigned> todo{arena->get_init_state_number()};
    std::vector<unsigned> pg2aut(arena->num_states(), unseen_mark);
    aut->set_init_state(aut->new_state());
    pg2aut[arena->get_init_state_number()] = aut->get_init_state_number();
    bdd out;
    while (!todo.empty())
      {
        unsigned v = todo.back();
        todo.pop_back();
        // Env edge -> keep all
        for (auto &e1: arena->out(v))
          {
            assert(w.at(e1.dst));
            if (!unsplit)
              {
                if (pg2aut[e1.dst] == unseen_mark)
                  pg2aut[e1.dst] = aut->new_state();
                aut->new_edge(pg2aut[v], pg2aut[e1.dst], e1.cond,
                              keep_acc ? e1.acc : acc_cond::mark_t({}));
              }
            // Player strat
            auto &e2 = arena->edge_storage(s[e1.dst]);
            if (pg2aut[e2.dst] == unseen_mark)
              {
                pg2aut[e2.dst] = aut->new_state();
                todo.push_back(e2.dst);
              }
            if (leave_choice)
              // Leave the choice
              out = e2.cond;
            else
              // Save only one letter
              out = bdd_satoneset(e2.cond, all_outputs, bddfalse);

            aut->new_edge(unsplit ? pg2aut[v] : pg2aut[e1.dst],
                          pg2aut[e2.dst],
                          unsplit ? (e1.cond & out):out,
                          keep_acc ? e2.acc : acc_cond::mark_t({}));
          }
      }

    aut->set_named_prop("synthesis-outputs", new bdd(all_outputs));
    // If no unsplitting is demanded, it remains a two-player arena
    // We do not need to track winner as this is a
    // strategy automaton
    if (!unsplit)
      {
        std::vector<bool>& sp_pg = * sp_ptr;
        std::vector<bool> sp_aut(aut->num_states());
        strategy_t str_aut(aut->num_states());
        for (unsigned i = 0; i < arena->num_states(); ++i)
          {
            if (pg2aut[i] == unseen_mark)
              // Does not appear in strategy
              continue;
            sp_aut[pg2aut[i]] = sp_pg[i];
            str_aut[pg2aut[i]] = s[i];
          }
        aut->set_named_prop("state-player",
                            new std::vector<bool>(std::move(sp_aut)));
        aut->set_named_prop("winning-region",
                            new region_t(aut->num_states(), true));
        aut->set_named_prop("strategy",
                            new strategy_t(std::move(str_aut)));
      }
    return aut;
  }

  std::ostream&
  print_pg(std::ostream& os, const twa_ptr& aut)
  {
    auto a = down_cast<twa_graph_ptr>(aut);
    if (!a)
      throw std::runtime_error("parity game output is only for twa_graph");
    // Parity games are state based
    twa_graph_ptr aut_sbacc_i = sbacc(a);
    twa_graph_ptr aut_sbacc = spot::colorize_parity(aut_sbacc_i, true);

    unsigned init = aut_sbacc->get_init_state_number();

    os << "parity " << aut_sbacc->num_states() - 1 << ";\n";
    std::vector<bool> seen(aut_sbacc->num_states(), false);
    std::vector<bool> owner(aut_sbacc->num_states(), false);
    std::vector<unsigned> todo({init});

    while (!todo.empty())
      {
        unsigned src = todo.back();
        todo.pop_back();
        if (seen[src])
          continue;
        seen[src] = true;
        os << src << ' ';
        os << aut_sbacc->state_acc_sets(src).max_set() - 1 << ' ';
        os << owner[src] << ' ';
        bool first = true;
        for (const auto& e: aut_sbacc->out(src))
          {
            if (!first)
              os << ',';
            first = false;
            os << e.dst;
            if (!seen[e.dst])
              {
                owner[e.dst] = !owner[src];
                todo.push_back(e.dst);
              }
          }
        if (src == init)
          os << " \"INIT\"";
        os << ";\n";
      }
    return os;
  }

}//spot
