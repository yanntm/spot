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

#include <cmath>
#include <spot/misc/game.hh>
#include <spot/misc/bddlt.hh>
#include <spot/twaalgos/sccinfo.hh>

namespace spot
{
  namespace
  {
    constexpr unsigned unseen_mark = std::numeric_limits<unsigned>::max();
    using par_t = int;
    constexpr par_t limit_par_even =
        std::numeric_limits<par_t>::max() & 1
        ? std::numeric_limits<par_t>::max()-3
        : std::numeric_limits<par_t>::max()-2;
    using strat_t = long long;
    constexpr strat_t no_strat_mark = std::numeric_limits<strat_t>::min();


    static const std::vector<bool>*
    ensure_game(const const_twa_graph_ptr& arena, const char* fnname)
    {
      auto owner = arena->get_named_prop<std::vector<bool>>("state-player");
      if (!owner)
        throw std::runtime_error
          (std::string(fnname) + ": automaton should define \"state-player\"");
      if (owner->size() != arena->num_states())
        throw std::runtime_error
          (std::string(fnname) + ": \"state-player\" should have "
           "as many states as the automaton");
      return owner;
    }

    static const std::vector<bool>*
    ensure_parity_game(const const_twa_graph_ptr& arena, const char* fnname)
    {
      bool max, odd;
      bool is_par = arena->acc().is_parity(max, odd, true);
      if (!is_par)
        throw std::runtime_error
          (std::string(fnname) +
           ": arena must have one of the four parity acceptance conditions");
//      for (const auto& e : arena->edges())
//        if (!e.acc)
//          throw std::runtime_error
//            (std::string(fnname) + ": arena must be colorized");
      return ensure_game(arena, fnname);
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
        return has_winner_[v] ? winner_[v] == p : false;
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

    // Internal structs used by parity_game
    // Struct to change recursive calls to stack
    struct work_t
    {
      work_t(unsigned wstep_, unsigned rd_, par_t min_par_,
             par_t max_par_) noexcept
        : wstep(wstep_),
          rd(rd_),
          min_par(min_par_),
          max_par(max_par_)
      {
      }
      const unsigned wstep, rd;
      const par_t min_par, max_par;
    }; // work_t

    // Collects information about an scc
    // Used to detect special cases
    struct subgame_info_t
    {
      typedef std::set<par_t, std::greater<par_t>> all_parities_t;

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


    // A class to solve parity games
    // The current implementation is inspired by
    // by oink however without multicore and adapted to transition based
    // acceptance
    class parity_game
    {
    public:

      bool solve(const twa_graph_ptr& arena)
      {
        // todo check if reordering states according to scc is worth it
        set_up(arena);
        // Start recursive zielonka in a bottom-up fashion on each scc
        subgame_info_t subgame_info;
        for (c_scc_idx_ = 0; c_scc_idx_ < info_->scc_count(); ++c_scc_idx_)
          {
            // Testing
            // Make sure that every state that has a winner also
            // belongs to a subgame
            assert([&]()
                    {
                      for (unsigned i = 0; i < arena_->num_states(); ++i)
                        if (w_.has_winner_[i]
                            && (subgame_[i] == unseen_mark))
                          return false;
                        return true;
                    }());
            // Useless SCCs are winning for player 0.
            if (!info_->is_useful_scc(c_scc_idx_))
              {
                // This scc also gets its own subgame
                ++rd_;
                for (unsigned v: c_states())
                  {
                    subgame_[v] = rd_;
                    w_.set(v, false);
                    // The strategy for player 0 is to take the first
                    // available edge.
                    if ((*owner_ptr_)[v] == false)
                      for (const auto &e : arena_->out(v))
                        {
                          s_[v] = arena_->edge_number(e);
                          break;
                        }
                  }
                continue;
              }
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
                    w_stack_.emplace_back(0, 0,
                                          min_par_graph_, max_abs_par_);
                    zielonka();
                  }
              }
          }
        // Every state needs a winner
        assert(std::all_of(w_.has_winner_.cbegin(), w_.has_winner_.cend(),
                           [](bool b)
                           { return b; }));
        // Only the states owned by the winner need a strategy
        assert([&]()
               {
                  for (unsigned v = 0; v < arena_->num_states(); ++v)
                    {
                      if (((*owner_ptr_)[v] == w_.winner(v))
                          && ((s_[v] <= 0) || (s_[v] > arena_->num_edges())))
                        return false;
                    }
                  return true;
               }());

        // Put the solution as named property
        region_t &w = *arena->get_or_set_named_prop<region_t>("state-winner");
        strategy_t &s = *arena->get_or_set_named_prop<strategy_t>("strategy");
        w.swap(w_.winner_);
        s.reserve(s_.size());
        for (auto as : s_)
          s.push_back(as == no_strat_mark ? 0 : (unsigned) as);

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

      void set_up(const twa_graph_ptr& arena)
      {
        owner_ptr_ = ensure_parity_game(arena, "solve_parity_game()");
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
        s_.resize(n_states, no_strat_mark);
        // Init
        rd_ = 0;
        info_ = std::make_unique<scc_info>(arena_);
        // Create all the parities
        bool max, odd;
        arena_->acc().is_parity(max, odd, true);
        max_abs_par_ = arena_->acc().all_sets().max_set()-1;
        // Make it the next larger odd
        par_t next_max_par_odd = max_abs_par_&1 ? max_abs_par_+2
                                                : max_abs_par_+1;
        all_edge_par_.resize(arena_->num_edges()+1);
        std::function<par_t(spot::acc_cond::mark_t m)> to_par_;
        if (max && odd)
          {
            min_par_graph_ = -1;
            max_par_graph_ = max_abs_par_;
            to_par_ = [next_max_par_odd](spot::acc_cond::mark_t m)
                        {
                          return (par_t)m.max_set() - 1;
                        };
          }
        else if (max && !odd)
          {
            // "Add 1" so even becomes odd
            max_abs_par_ += 1;
            min_par_graph_ = 0;
            max_par_graph_ = max_abs_par_;
            to_par_ = [](spot::acc_cond::mark_t m)
                        {
                          return (par_t)m.max_set();
                        };
          }
        else if (!max && odd)
          {
            max_abs_par_ = 1;
            min_par_graph_ = -next_max_par_odd;
            max_par_graph_ = 0;
            // Make sure no-color is the weakest
            to_par_ = [next_max_par_odd](spot::acc_cond::mark_t m)
                        {
                          par_t p = (par_t)m.min_set() - 1;
                          return p == -1 ? -next_max_par_odd : -p;
                        };
          }
        else if (!max && !odd)
          {
            max_abs_par_ = 0;
            min_par_graph_ = -next_max_par_odd;
            max_par_graph_ = -1;
            // Make sure no-color is the weakest
            to_par_ = [next_max_par_odd](spot::acc_cond::mark_t m)
                        {
                          par_t p = (par_t)m.min_set();
                          return p == 0 ? -next_max_par_odd : -p;
                        };
          }
        else
          throw std::runtime_error("Unreachable!");
        for (unsigned i = 1; i < arena_->num_edges() + 1; ++i)
          {
            all_edge_par_[i] = to_par_(arena_->edge_storage(i).acc);
            assert((min_par_graph_ <= all_edge_par_[i])
                    && (all_edge_par_[i] <= max_par_graph_));
          }
      }

      // Checks if an scc is empty and reports the occurring parities
      // or special cases
      inline subgame_info_t
      inspect_scc(par_t max_par)
      {
        subgame_info_t info;
        info.is_empty = true;
        info.is_one_player0 = true;
        info.is_one_player1 = true;
        for (unsigned v : c_states())
          {
            if (subgame_[v] != unseen_mark)
              continue;

            bool multi_edge = false;
            for (const auto &e : arena_->out(v))
              if (subgame_[e.dst] == unseen_mark)
                {
                  info.is_empty = false;
                  par_t this_par = to_par(e);
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
        // todo is this always true?
        assert(info.all_parities.size() || info.is_empty);
        info.is_one_parity = info.all_parities.size() == 1;
        // Done
        return info;
      }

      // Computes the trivially solvable part of the scc
      // That is the states that can be attracted to an
      // outgoing transition
      inline subgame_info_t
      fix_scc()
      {
        // Note that the winner of the transitions
        // leaving the scc are already determined
        // attr(...) will only modify the
        // states within the current scc
        // but we have to "trick" it into
        // not disregarding the transitions leaving the scc
        // dummy needed to pass asserts
        max_abs_par_ = limit_par_even+2;
        // The attractors should define their own subgame
        // but as we want to compute the attractors of the
        // leaving transitions, we need to make
        // sure that
        // a) no transition is excluded due to its parity
        // b) no transition is considered accepting/winning
        //    due to its parity
        // Final note: Attractors cannot intersect by definition
        //             therefore the order in which they are computed
        //             is irrelevant
        unsigned dummy_rd = 0;
        // Attractor of outgoing transitions winning for env
        attr(dummy_rd, false, limit_par_even, true, limit_par_even, false);
        // Attractor of outgoing transitions winning for player
        attr(dummy_rd, true, limit_par_even+1, true, limit_par_even+1, false);

        // No strategy fix need
        // assert if all winning states of the current scc have a valid strategy

        assert([&]()
               {
                 for (unsigned v : c_states())
                   {
                     if (!w_.has_winner_[v])
                       continue;
                     // We only need a strategy if the winner
                     // of the state is also the owner
                     if (w_.winner(v) != (*owner_ptr_)[v])
                       continue;
                     if (s_[v] <= 0)
                       {
                         std::cerr << "state " << v << " has a winner "
                                   << w_.winner(v) << " and owner "
                                   << (*owner_ptr_)[v]
                                   << " but no strategy "
                                   << s_[v] << '\n';
                         return false;
                       }
                     const auto& e = arena_->edge_storage(s_[v]);
                     if (!w_.has_winner_[e.dst]
                         || (w_.winner(e.src) != w_.winner(e.dst)))
                       {
                         std::cerr << "state " << v << " has a winner "
                                   << w_.winner(v)
                                   << " but no valid strategy\n";
                         return false;
                       }
                   }
                 return true;
               }());

        auto ins = inspect_scc(limit_par_even);
        max_abs_par_ = *ins.all_parities.begin();
        return ins;
      } // fix_scc

      inline bool
      attr(unsigned &rd, bool p, par_t max_par,
           bool acc_par, par_t min_win_par, bool respect_sg=true)
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

        assert((!acc_par) || (acc_par && to_player(max_par) == p));
        assert(!acc_par || (min_par_graph_ <= min_win_par));
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
            grown |= !to_add.empty();
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
            to_add.clear();

            for (unsigned v : c_states())
              {
                if ((subgame_[v] < rd) || (w_(v, p)))
                  // Not in subgame or winning for p
                  continue;

                bool is_owned = (*owner_ptr_)[v] == p;
                bool wins = !is_owned;
                // Loop over out-going

                // Optim: If given the choice,
                // we seek to go to the "oldest" subgame
                // That is the subgame with the lowest rd value
                unsigned min_subgame_idx = unseen_mark;
                for (const auto &e: arena_->out(v))
                  {
                    par_t this_par = to_par(e);
                    if ((!respect_sg || (subgame_[e.dst] >= rd))
                         && (this_par <= max_par))
                      {
                        // Check if winning
                        if (w_(e.dst, p)
                            || (acc_par && (min_win_par <= this_par)))
                          {
                            assert(!acc_par || (this_par < min_win_par) ||
                                   (acc_par && (min_win_par <= this_par) &&
                                    (to_player(this_par) == p)));
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
      fix_strat_acc(unsigned rd, bool p, par_t min_win_par, par_t max_par)
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
              // If current strat is admissible ->
              // nothing to do for env
              continue;

            // This is an accepting edge that is no longer admissible
            // or we seek a more desirable edge (for player)
            assert(min_win_par <= to_par(e_s));
            assert(to_par(e_s) <= max_par);

            // Strategy heuristic : go to the oldest subgame
            unsigned min_subgame_idx = unseen_mark;

            s_[v] = no_strat_mark;
            for (const auto &e_fix : arena_->out(v))
              {
                if (subgame_[e_fix.dst] >= rd)
                  {
                    par_t this_par = to_par(e_fix);
                    // This edge must have less than max_par,
                    // otherwise it would have already been attracted
                    assert((this_par <= max_par)
                           || (to_player(this_par) != (max_par&1)));
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
            if (s_[v] == no_strat_mark)
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
                  assert(this_work.min_par == min_par_graph_);

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
                  // Attention in partially colored graphs
                  // the parity -1 and 0 appear
                  par_t max_par = *subgame_info.all_parities.begin();
                  par_t min_win_par = max_par;
                  while ((min_win_par >= (min_par_graph_+2)) &&
                         (!subgame_info.all_parities.count(min_win_par - 1)))
                    min_win_par -= 2;
                  assert(min_win_par >= min_par_graph_);
                  assert(max_par >= min_win_par);
                  assert((max_par&1) == (min_win_par&1));
                  assert(!subgame_info.all_parities.empty());

                  // Get the player
                  bool p = to_player(min_win_par);
                  // Attraction to highest par
                  // This increases rd_ and passes it to rd
                  attr(rd, p, max_par, true, min_win_par);
                  // All those attracted get subgame_[v] <- rd

                  // Continuation
                  w_stack_.emplace_back(1, rd, min_win_par, max_par);
                  // Recursion
                  w_stack_.emplace_back(0, 0, min_par_graph_, min_win_par - 1);
                  // Others attracted will have higher counts in subgame
                  break;
                }
              case (1):
                {
                  unsigned rd = this_work.rd;
                  par_t min_win_par = this_work.min_par;
                  par_t max_par = this_work.max_par;
                  assert(to_player(min_win_par) == to_player(max_par));
                  bool p = to_player(min_win_par);
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
                            s_[v] = no_strat_mark;
                          }
                      w_stack_.emplace_back(0, 0, min_par_graph_, max_par);
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
      }

      // Dedicated solver for special cases
      inline void one_par_subgame_solver(const subgame_info_t &info,
                                         par_t max_par)
      {
        assert(info.all_parities.size() == 1);
        // The entire subgame is won by the player of the only parity
        // Any edge will do
        // todo optim for smaller circuit
        // This subgame gets its own counter
        ++rd_;
        unsigned rd = rd_;
        par_t one_par = *info.all_parities.begin();
        bool winner = to_player(one_par);
        assert(one_par <= max_par);

        for (unsigned v : c_states())
          {
            if (subgame_[v] != unseen_mark)
              continue;
            // State of the subgame
            subgame_[v] = rd;
            w_.set(v, winner);
            // Get the strategy
            assert(s_[v] == no_strat_mark);
            for (const auto &e : arena_->out(v))
              {
                par_t this_par = to_par(e);
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

      template <class EDGE>
      inline par_t
      to_par(const EDGE& e)
      {
        return all_edge_par_[arena_->edge_number(e)];
      }

      inline bool
      to_player(par_t par)
      {
        return par & 1;
      }

      twa_graph_ptr arena_;
      const std::vector<bool> *owner_ptr_;
      unsigned rd_;
      winner_t w_;
      // Subgame array similar to the one from oink!
      std::vector<unsigned> subgame_;
      // strategies for env and player; For synthesis only player is needed
      // We need a signed value here in order to "fix" the strategy
      // during construction
      std::vector<strat_t> s_;

      // Informations about sccs andthe current scc
      std::unique_ptr<scc_info> info_;
      par_t max_abs_par_; // Max parity occurring in the current scc
      // Minimal and maximal parity occurring in the entire graph
      par_t min_par_graph_, max_par_graph_;
      // Info on the current scc
      unsigned c_scc_idx_;
      // Change recursive calls to stack
      std::vector<work_t> w_stack_;
      // Directly store a vector of parities
      // This vector will be created such
      // that it takes care of the actual parity condition
      // and after that zielonka can be called as if max odd
      std::vector<par_t> all_edge_par_;
    };

  } // anonymous

  bool solve_parity_game(const twa_graph_ptr& arena)
  {
    parity_game pg;
    return pg.solve(arena);
  }

  void pg_print(std::ostream& os, const const_twa_graph_ptr& arena)
  {
    auto owner = ensure_parity_game(arena, "pg_print");
    // Ensure coloring
    assert([&]()
            {
              bool max;
              bool odd;
              arena->acc().is_parity(max, odd, true);
              return max && odd;
            }() && "pg_printer needs max-odd parity");
    assert([&]()
           {
             for (unsigned ie = 0; ie < arena->num_edges(); ++ie)
               {
                  const auto& es = arena->edge_storage(ie+1);
                  if (!es.acc)
                    return false;
               }
             return true;
           }() && "Arena must be colorized");

    unsigned ns = arena->num_states();
    unsigned init = arena->get_init_state_number();
    os << "parity " << ns - 1 << ";\n";
    std::vector<bool> seen(ns, false);
    std::vector<unsigned> todo({init});
    while (!todo.empty())
      {
        unsigned src = todo.back();
        todo.pop_back();
        if (seen[src])
          continue;
        seen[src] = true;
        os << src << ' ';
        os << arena->out(src).begin()->acc.max_set() - 1 << ' ';
        os << (*owner)[src] << ' ';
        bool first = true;
        for (auto& e: arena->out(src))
          {
            if (!first)
              os << ',';
            first = false;
            os << e.dst;
            if (!seen[e.dst])
              todo.push_back(e.dst);
          }
        if (src == init)
          os << " \"INIT\"";
        os << ";\n";
      }
  }

  void alternate_players(spot::twa_graph_ptr& arena,
                         bool first_player, bool complete0)
  {
    auto um = arena->acc().unsat_mark();
    if (!um.first)
      throw std::runtime_error
        ("alternate_players(): game winning condition is a tautology");

    unsigned sink_env = 0;
    unsigned sink_con = 0;

    std::vector<bool> seen(arena->num_states(), false);
    unsigned init = arena->get_init_state_number();
    std::vector<unsigned> todo({init});
    auto owner = new std::vector<bool>(arena->num_states(), false);
    (*owner)[init] = first_player;
    while (!todo.empty())
      {
        unsigned src = todo.back();
        todo.pop_back();
        seen[src] = true;
        bdd missing = bddtrue;
        for (const auto& e: arena->out(src))
          {
            bool osrc = (*owner)[src];
            if (complete0 && !osrc)
              missing -= e.cond;

            if (!seen[e.dst])
              {
                (*owner)[e.dst] = !osrc;
                todo.push_back(e.dst);
              }
            else if ((*owner)[e.dst] == osrc)
              {
                delete owner;
                throw
                  std::runtime_error("alternate_players(): odd cycle detected");
              }
          }
        if (complete0 && !(*owner)[src] && missing != bddfalse)
          {
            if (sink_env == 0)
              {
                sink_env = arena->new_state();
                sink_con = arena->new_state();
                owner->push_back(true);
                owner->push_back(false);
                arena->new_edge(sink_con, sink_env, bddtrue, um.second);
                arena->new_edge(sink_env, sink_con, bddtrue, um.second);
              }
            arena->new_edge(src, sink_con, missing, um.second);
          }
      }

    arena->set_named_prop("state-player", owner);
  }

  twa_graph_ptr
  highlight_strategy(twa_graph_ptr& aut,
                     int player0_color,
                     int player1_color)
  {
    auto owner = ensure_game(aut, "highlight_strategy()");
    region_t* w = aut->get_named_prop<region_t>("state-winner");
    strategy_t* s = aut->get_named_prop<strategy_t>("strategy");
    if (!w)
      throw std::runtime_error
        ("highlight_strategy(): "
         "winning region unavailable, solve the game first");
    if (!s)
      throw std::runtime_error
        ("highlight_strategy(): strategy unavailable, solve the game first");

    unsigned ns = aut->num_states();
    auto* hl_edges = aut->get_or_set_named_prop<std::map<unsigned, unsigned>>
      ("highlight-edges");
    auto* hl_states = aut->get_or_set_named_prop<std::map<unsigned, unsigned>>
      ("highlight-states");

    if (unsigned sz = std::min(w->size(), s->size()); sz < ns)
      ns = sz;

    for (unsigned n = 0; n < ns; ++n)
      {
        int color = (*w)[n] ? player1_color : player0_color;
        if (color == -1)
          continue;
        (*hl_states)[n] = color;
        if ((*w)[n] == (*owner)[n])
          (*hl_edges)[(*s)[n]] = color;
      }

    return aut;
  }

  void set_state_players(twa_graph_ptr arena, const std::vector<bool>& owners)
  {
    std::vector<bool>* owners_ptr = new std::vector<bool>(owners);
    set_state_players(arena, owners_ptr);
  }

  void set_state_players(twa_graph_ptr arena, std::vector<bool>* owners)
  {
    if (owners->size() != arena->num_states())
      throw std::runtime_error
        ("set_state_players(): There must be as many owners as states");

    arena->set_named_prop<std::vector<bool>>("state-player", owners);
  }

  void set_state_player(twa_graph_ptr arena, unsigned state, unsigned owner)
  {
    if (state >= arena->num_states())
      throw std::runtime_error("set_state_player(): invalid state number");

    std::vector<bool> *owners = arena->get_named_prop<std::vector<bool>>
      ("state-player");
    if (!owners)
      {
        owners = new std::vector<bool>(arena->num_states(), false);
        arena->set_named_prop<std::vector<bool>>("state-player", owners);
      }

    (*owners)[state] = owner;
  }

  const std::vector<bool>& get_state_players(const_twa_graph_ptr arena)
  {
    const std::vector<bool>* owners = arena->get_named_prop<std::vector<bool>>
      ("state-player");
    if (!owners)
      throw std::runtime_error
        ("get_state_players(): state-player property not defined, not a game");

    return *owners;
  }

  unsigned get_state_player(const_twa_graph_ptr arena, unsigned state)
  {
    if (state >= arena->num_states())
      throw std::runtime_error("get_state_player(): invalid state number");

    const std::vector<bool>* onwers = arena->get_named_prop<std::vector<bool>>
      ("state-player");
    if (!onwers)
      throw std::runtime_error
        ("get_state_player(): state-player property not defined, not a game");

    return (*onwers)[state] ? 1 : 0;
  }
}
