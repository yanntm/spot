// -*- coding: utf-8 -*-
// Copyright (C) 2017, 2018 Laboratoire de Recherche et Développement
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
#include <spot/misc/timer.hh>
#include <spot/misc/bddlt.hh>

#include <deque>

#include <fstream>
#include <string>

#include <sstream>
#include <string>

namespace{
  using namespace spot;
  
  bool is_arena(const twa_graph_ptr& arena)
  {
    if (arena->get_named_prop<std::vector<bool>>("state-player") == nullptr)
      throw std::runtime_error("States have no owners!");
    if ((arena->get_named_prop<std::vector<bool>>("winning-region") == nullptr)
        || (arena->get_named_prop<std::vector<unsigned>>("strategy") ==
            nullptr))
      throw std::runtime_error("Arena must have winning region and strategy!");
    return true;
  }
  
  bool is_solved_arena(const twa_graph_ptr& arena){
    is_arena(arena);
    unsigned n_states = arena->num_states();
    std::vector<bool>& owner =
        *arena->get_named_prop<std::vector<bool>>("state-player");
    std::vector<bool>& winner =
        *arena->get_named_prop<std::vector<bool>>("winning-region");
    std::vector<unsigned>& strategy =
        *arena->get_named_prop<std::vector<unsigned>>("strategy");
    if ((n_states!=owner.size())
        || (n_states!=winner.size())
        || (n_states!=strategy.size()))
      throw std::runtime_error("Arena has inconsistent sizes");
    // We need a strategy for all owned and winning states
    for (unsigned v=0; v<n_states; v++)
      if (owner[v] && winner[v])
        assert((strategy[v] > 0) && (strategy[v] <= arena->num_edges()));
    return true;
  }
  
  bool check_arena(const twa_graph_ptr& arena)
  {
    is_arena(arena);
    std::vector<bool>* owner_ptr =
        arena->get_named_prop<std::vector<bool>>("state-player");
    if (owner_ptr->size() != arena->num_states())
      throw std::runtime_error("More or less owners than states");
    std::vector<bool> &owner = *owner_ptr;
    bool max, odd;
    arena->acc().is_parity(max, odd, true);
    if (!(max && odd))
      throw std::runtime_error("arena must have max-odd acc. condition");
    for (const auto &e : arena->edges())
    {
      if (e.acc.max_set() == 0)
        throw std::runtime_error("arena must be colorized");
//      if ((owner[e.src] == owner[e.dst]) && (e.src!=e.dst))
      if ((owner[e.src] == owner[e.dst]))
      {
        std::cerr<<owner[e.src]<<" : "<<e.src<<", "<<e.dst<<
                 "; "<<arena->num_states()<<std::endl;
        throw std::runtime_error("Inconsistent arena");
      }
    }
    for (unsigned v = 0; v < arena->num_states(); v++)
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
        :e_num(num),
         e_dst(dst),
         e_acc(acc){};
    unsigned e_num, e_dst;
    acc_cond::mark_t e_acc;
  }; // edge_stash_t
  
  // Internal structs sued by parity_game
  // Struct to change recursive calls to stack
  struct work_t
  {
    work_t(unsigned wstep_, unsigned rd_, unsigned min_par_,
           unsigned max_par_) noexcept
        :wstep(wstep_),
         rd(rd_),
         min_par(min_par_),
         max_par(max_par_) {};
    unsigned wstep, rd, min_par, max_par;
  }; // work_t
  
  // Collects informations about an scc
  // Used to detect special cases
  struct subgame_info_t
  {
    typedef std::set<unsigned, std::greater<unsigned>> all_parities_t;
    subgame_info_t()noexcept{};
    subgame_info_t(bool empty, bool one_parity, bool one_player0,
                   bool one_player1, all_parities_t parities)noexcept
        : is_empty(empty),
          is_one_parity(one_parity),
          is_one_player0(one_player0),
          is_one_player1(one_player1),
          all_parities(parities){};
    bool is_empty; // empty subgame
    bool is_one_parity; // only one parity appears in the subgame
    // todo : Not used yet
    bool is_one_player0; // one player subgame for player0 <-> p==false
    bool is_one_player1; // one player subgame for player1 <-> p==true
    all_parities_t all_parities;
  }; // subgame_info_t
  
}

namespace spot
{
  namespace parity_game{
    typedef std::unordered_set<unsigned> region_old_t;
    // Map state number to index of the transition to take.
    typedef std::unordered_map<unsigned, unsigned> strategy_old_t;
    
    // false -> env, true -> player
    typedef std::vector<bool> region_t;
    // state idx -> global edge number
    typedef std::vector<unsigned> strategy_t;
    void solve_rec_old(const twa_graph_ptr& arena,
                       const std::vector<bool>& owner,
                       region_old_t& subgame, unsigned max_parity,
                       region_old_t (&w)[2], strategy_old_t (&s)[2]);
    strategy_old_t
    attractor_old(const twa_graph_ptr arena,
                  const std::vector<bool> owner,
                  const region_old_t& subgame, region_old_t& set,
                  unsigned max_parity, int p, bool attr_max=false);
    
  
    strategy_old_t
    attractor_old(const twa_graph_ptr arena,
                  const std::vector<bool> owner,
                  const region_old_t& subgame, region_old_t& set,
                  unsigned max_parity, int p, bool attr_max)
    {
      strategy_old_t strategy;
      std::set<unsigned> complement(subgame.begin(), subgame.end());
      for (unsigned s: set)
        complement.erase(s);
    
      acc_cond::mark_t max_acc({});
      for (unsigned i = 0; i <= max_parity; ++i)
        max_acc.set(i);
    
      bool once_more;
      do
      {
        once_more = false;
        for (auto it = complement.begin(); it != complement.end();)
        {
          unsigned s = *it;
          unsigned i = 0;
        
          bool is_owned = owner[s] == p;
          bool wins = !is_owned;
        
          for (const auto& e: arena->out(s))
          {
            if ((e.acc & max_acc) && subgame.count(e.dst))
            {
              if (set.count(e.dst)
                  || (attr_max && e.acc.max_set() - 1 == max_parity))
              {
                if (is_owned)
                {
                  strategy[s] = i;
                  wins = true;
                  break; // no need to check all edges
                }
              }
              else
              {
                if (!is_owned)
                {
                  wins = false;
                  break; // no need to check all edges
                }
              }
            }
            ++i;
          }
        
          if (wins)
          {
            // FIXME C++17 extract/insert could be useful here
            set.emplace(s);
            it = complement.erase(it);
            once_more = true;
          }
          else
            ++it;
        }
      } while (once_more);
      return strategy;
    } // attractor_old
  
    void solve_rec_old(const twa_graph_ptr& arena,
                       const std::vector<bool>& owner,
                       region_old_t& subgame, unsigned max_parity,
                       region_old_t (&w)[2], strategy_old_t (&s)[2])
    {
      assert(w[0].empty());
      assert(w[1].empty());
      assert(s[0].empty());
      assert(s[1].empty());
      // The algorithm works recursively on subgames. To avoid useless copies of
      // the game at each call, subgame and max_parity are used to filter states
      // and transitions.
      if (subgame.empty())
        return;
      int p = max_parity % 2;
    
      // Recursion on max_parity.
      region_old_t u;
      auto strat_u =
          attractor_old(arena, owner, subgame, u, max_parity, p, true);
    
      if (max_parity == 0)
      {
        s[p] = std::move(strat_u);
        w[p] = std::move(u);
        // FIXME what about w[!p]?
        return;
      }
    
      for (unsigned s: u)
        subgame.erase(s);
      // Player's winning region in the first recursive call.
      region_old_t w0[2];
      // Player's winning strategy in the first recursive call.
      strategy_old_t s0[2];
      solve_rec_old(arena, owner, subgame, max_parity - 1, w0, s0);
      if (w0[0].size() + w0[1].size() != subgame.size())
        throw std::runtime_error("size mismatch");
      //if (w0[p].size() != subgame.size())
      //  for (unsigned s: subgame)
      //    if (w0[p].find(s) == w0[p].end())
      //      w0[!p].insert(s);
      subgame.insert(u.begin(), u.end());
    
      if (w0[p].size() + u.size() == subgame.size())
      {
        s[p] = std::move(strat_u);
        s[p].insert(s0[p].begin(), s0[p].end());
        w[p].insert(subgame.begin(), subgame.end());
        return;
      }
    
      // Recursion on game size.
      auto strat_wnp =
          attractor_old(arena, owner, subgame, w0[!p], max_parity, !p);
    
      for (unsigned s: w0[!p])
        subgame.erase(s);
    
      region_old_t w1[2]; // Odd's winning region in the second recursive call.
      strategy_old_t s1[2]; // Odd's winning strategy in the second recursive
      // call.
      solve_rec_old(arena, owner, subgame, max_parity, w1, s1);
      if (w1[0].size() + w1[1].size() != subgame.size())
        throw std::runtime_error("size mismatch");
    
      w[p] = std::move(w1[p]);
      s[p] = std::move(s1[p]);
    
      w[!p] = std::move(w1[!p]);
      w[!p].insert(w0[!p].begin(), w0[!p].end());
      s[!p] = std::move(strat_wnp);
      s[!p].insert(s0[!p].begin(), s0[!p].end());
      s[!p].insert(s1[!p].begin(), s1[!p].end());
    
      subgame.insert(w0[!p].begin(), w0[!p].end());
    } // solve_rec_old
    
    class parity_game
    {
    public:
      
      bool solve(const twa_graph_ptr& arena)
      {
        assert(check_arena(arena));
        if (!arena->get_named_prop<region_t>("winning-region")
            || !arena->get_named_prop<strategy_t>("strategy")
            || !arena->get_named_prop<std::vector<bool>>("state-player"))
          throw std::runtime_error("Arena missing winning-region, strategy "
                                   "or state-player");
        // todo check if reordering states according to scc is worth it
        set_up(arena);
        // Start recursive zielonka on each scc
        subgame_info_t subgame_info;
        c_scc_idx_ = 0;
        for (c_scc_ = info_->begin(); c_scc_ != info_->end();
             c_scc_++, c_scc_idx_++)
        {
          subgame_info = fix_scc();
          if (!subgame_info.is_empty) // If empty, the scc was trivially solved
          {
            if (subgame_info.is_one_parity){
              one_par_subgame_solver(subgame_info, max_abs_par_);
            }else {
              max_abs_par_ = *subgame_info.all_parities.begin();
              w_stack_.emplace_back(0, 0, 0, max_abs_par_);
              zielonka();
            }
          }
        }
        // All done -> restore graph, i.e. undone self-looping
        restore();
  
        assert(std::all_of(w_.has_winner_.cbegin(), w_.has_winner_.cend(),
                                [](bool b){return b;}));
        assert(std::all_of(s_.cbegin(), s_.cend(),
                                [](unsigned e_idx){return e_idx>0;}));
        
        // Put the solution as name variable
        region_t& w = *arena->get_named_prop<region_t>("winning-region");
        strategy_t& s = *arena->get_named_prop<strategy_t>("strategy");
        w.swap(w_.winner_);
        s.resize(s_.size());
        std::copy(s_.begin(), s_.end(), s.begin());
  
        free(info_);
  
//        std::cerr << "callcount " << call_count_ << " direct solves " <<
//        direct_solve_count_ << " one parities " << one_parity_count_ << "\n";
        
        clean_up();
        return w[arena->get_init_state_number()];
      }
      
    private:
      void set_up(const twa_graph_ptr& arena)
      {
        if (!arena->get_named_prop<region_t>("winning-region")
            || !arena->get_named_prop<strategy_t>("strategy")
            || !arena->get_named_prop<std::vector<bool>>("state-player"))
          throw std::runtime_error("Arena missing winning-region, strategy "
                                   "or state-player");
        arena_ = arena;
        owner_ptr_ = arena_->get_named_prop<std::vector<bool>>("state-player");
        
        unsigned n_states=arena_->num_states();
        // Alloc
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
        info_ = new scc_info(arena_);
        // Every edge leaving an scc needs to be "fixed"
        // at some point.
        // We store: number of edge fixed, original dst, original acc
        change_stash_.clear();
        change_stash_.reserve(info_->scc_count()*2);
    
        call_count_=0;
        direct_solve_count_=0;
        one_parity_count_=0;
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
        for (unsigned v : c_scc_->states())
        {
          if (subgame_[v] != unseen_mark)
            continue;
          info.is_empty = false;
      
          bool multi_edge = false;
          for (const auto &e : arena_->out(v))
            if (subgame_[e.dst] == unseen_mark)
            {
              unsigned this_par = e.acc.max_set() - 1;
              if (this_par <= max_par) {
                info.all_parities.insert(this_par);
                multi_edge = true;
              }
            }
          if (multi_edge)
          {
            // This state has multiple edges, so it is not
            // a one player subgame for !owner
            if ((*owner_ptr_)[v])
            {
              info.is_one_player1 = false;
            }else {
              info.is_one_player0 = false;
            }
          }
        } // v
        assert(info.all_parities.size()
                    || (!info.all_parities.size() && info.is_empty));
        info.is_one_parity = info.all_parities.size()==1;
        // Done
        return info;
      }
  
      // Checks if an scc can be trivially solved
      inline subgame_info_t
      fix_scc()
      {
        auto scc_acc = info_->acc_sets_of(c_scc_idx_);
        // We will override all parities of edges leaving the scc
        bool added[] = {false, false};
        acc_cond::mark_t odd_mark, even_mark;
        unsigned par_pair[2];
        unsigned scc_new_par = std::max(scc_acc.max_set(), 1u);
        if (scc_new_par & 1)
        {
          par_pair[1] = scc_new_par;
          par_pair[0] = scc_new_par + 1;
        } else {
          par_pair[1] = scc_new_par + 1;
          par_pair[0] = scc_new_par;
        }
        even_mark = acc_cond::mark_t({par_pair[0]});
        odd_mark = acc_cond::mark_t({par_pair[1]});
  
        // Only necessary to pass tests
        max_abs_par_ = std::max(par_pair[0], par_pair[1]);
  
        for (unsigned v : c_scc_->states())
        {
          assert(subgame_[v] == unseen_mark);
          for (auto &e : arena_->out(v)) {
            // The outgoing edges are taken finitely often -> disregard parity
            if (subgame_[e.dst] != unseen_mark)
            {
              // Edge leaving the scc
              change_stash_.emplace_back(arena_->edge_number(e), e.dst, e.acc);
              if (w_.winner(e.dst))
              {
                // Winning region of player -> odd
                e.acc = odd_mark;
                added[1] = true;
              } else {
                // Winning region of env -> even
                e.acc = even_mark;
                added[0] = true;
              }
              // Replace with self-loop
              e.dst = e.src;
            }
          } // e
        } // v
    
        // Compute the attractors of the self-loops
        // These can be directly added to the winning states
        // Note: attractors can not intersect therefore the order in which
        // they are computed does not matter
        unsigned dummy_rd;
    
        for (bool p : {false, true})
          if (added[p])
            attr(dummy_rd, p, par_pair[p], true, par_pair[p]);
    
        if (added[0] || added[1])
          // Fix "negative" strategy
          for (unsigned v : c_scc_->states())
            if (subgame_[v]!=unseen_mark)
              s_[v] = std::abs(s_[v]);
    
        subgame_info_t info = inspect_scc(unseen_mark);
        direct_solve_count_ += info.is_empty;
        return info;
      } // fix_scc
  
      inline bool
      attr(unsigned& rd, bool p, unsigned max_par,
           bool acc_par, unsigned min_win_par)
      {
        // Computes the attractor of the winning set of player p within a
        // subgame given as rd.
        // If acc_par is true, max_par transitions are also accepting and
        // the subgame count will be increased
        // The attracted vertices are directly added to the set
    
        // Increase rd meaning we create a new subgame
        if (acc_par)
        {
          ++rd_;
          rd = rd_;
        }
        // todo replace with a variant of topo sort ?
        // As proposed in Oink! / PGSolver
        // Needs the transposed graph however
    
        assert((!acc_par) || (acc_par && (max_par&1)==p));
        assert(!acc_par || (0<min_win_par));
        assert((min_win_par<=max_par) && (max_par<=max_abs_par_));
    
        bool grown = false;
        // We could also directly mark states as owned,
        // instead of adding them to to_add first,
        // possibly reducing the number of iterations.
        // However by making the algorithm complete a loop
        // before adding it is like a backward bfs and (generally) reduces
        // the size of the strategy
        static std::vector<unsigned> to_add;
        // This function is called a lot, so keeping the vector around helps
    
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
      
          for (unsigned v : c_scc_->states())
          {
            if ((subgame_[v]<rd) || (w_(v,p)))
              // Not in subgame or winning
              continue;
        
            bool is_owned = (*owner_ptr_)[v] == p;
            bool wins = !is_owned;
            // Loop over out-going
        
            // Optim: If given the choice,
            // we seek to go to the "oldest" subgame
            // That is the subgame with the lowest rd value
            unsigned min_subgame_idx = -1u;
            for (const auto& e: arena_->out(v))
            {
              unsigned this_par = e.acc.max_set() - 1;
              if ((subgame_[e.dst]>=rd) && (this_par<=max_par))
              {
                // Check if winning
                if (w_(e.dst, p) || (acc_par && (min_win_par <= this_par)))
                {
                  assert(!acc_par || (this_par < min_win_par) ||
                              (acc_par && (min_win_par <= this_par) &&
                               ((this_par & 1) == p)));
                  if (is_owned)
                  {
                    wins = true;
                    if (acc_par)
                    {
                      s_[v] = arena_->edge_number(e);
                      if (min_win_par <= this_par)
                        // max par edge
                        // change sign -> mark as needs to be possibly fixed
                        s_[v] = -s_[v];
                      break;
                    }else{
                      //snapping
                      if (subgame_[e.dst]<min_subgame_idx)
                      {
                        s_[v] = arena_->edge_number(e);
                        min_subgame_idx = subgame_[e.dst];
                        if (!p)
                          // No optim for env
                          break;
                      }
                    }
                  }// owned
                } else {
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
        }while(!to_add.empty());
        // done
    
        assert(to_add.empty());
        return grown;
      }
  
      // We need to check if transitions that are accepted due
      // to their parity remain in the winning region of p
      inline bool
      fix_strat_acc(unsigned rd, bool p, unsigned min_win_par, unsigned max_par)
      {
        for (unsigned v : c_scc_->states()){
          // Only current attractor and owned and winning vertices are concerned
          if ((subgame_[v]!=rd) || !w_(v,p)
              || ((*owner_ptr_)[v]!=p) || (s_[v]>0))
            continue;
          
          // owned winning vertex of attractor
          // If current strat is admissible -> nothing to do
          // Get the strategy edge
          s_[v] = -s_[v];
          const auto& e_s = arena_->edge_storage(s_[v]);
          // Optimization only for player
          if (!p && w_(e_s.dst, p))
            continue;
      
          // This is an accepting edge that is no longer admissible
          // or we seek a more desirable edge
          assert(min_win_par <= e_s.acc.max_set() - 1);
          assert(e_s.acc.max_set() - 1 <= max_par);
      
          // Strategy heuristic : go to the oldest subgame
          unsigned min_subgame_idx = -1u;
      
          s_[v] = -1;
          for (const auto & e_fix : arena_->out(v))
          {
            if (subgame_[e_fix.dst]>=rd)
            {
              unsigned this_par = e_fix.acc.max_set() - 1;
              // This edge must have less than max_par, otherwise it would have already been attracted
              assert((this_par<=max_par) || ((this_par&1)!=(max_par&1)));
              // if it is accepting and leads to the winning region
              // -> valid fix
              if ((min_win_par<=this_par)
                  && (this_par<=max_par)
                  && w_(e_fix.dst, p)
                  && (subgame_[e_fix.dst] < min_subgame_idx))
              {
                // Max par edge to older subgame found
                s_[v] = arena_->edge_number(e_fix);
                min_subgame_idx = subgame_[e_fix.dst];
              }
            }
          }
          if (s_[v] == -1){
            // NO fix found
            // This state is NOT won by p due to any accepting edges
            return true; // true -> grown
          }
        }
        // Nothing to fix or all fixed
        return false; // false -> not grown == all good
      }
  
      inline void zielonka()
      {
        while(!w_stack_.empty())
        {
          auto this_work = w_stack_.back();
          w_stack_.pop_back();
      
          switch(this_work.wstep)
          {
            case(0):
            {
              assert(this_work.rd==0);
              assert(this_work.min_par==0);
          
              unsigned rd;
              assert(this_work.max_par <= max_abs_par_);
              call_count_++;
          
              // Check if empty and get parities
              subgame_info_t subgame_info = inspect_scc(this_work.max_par);
          
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
              while ((min_win_par > 2)
                     && (!subgame_info.all_parities.count(min_win_par - 1)))
                min_win_par -= 2;
              assert(max_par > 0);
              assert(!subgame_info.all_parities.empty());
              assert(min_win_par > 0);
          
              // Get the player
              bool p = min_win_par & 1;
              assert((max_par & 1) == (min_win_par & 1));
              // Attraction to highest par
              // This increases rd_ and passes it to rd
              attr(rd, p, max_par, true, min_win_par);
              // All those attracted get subgame_[v] <- rd
              
              // Continuation
              w_stack_.emplace_back(1,rd,min_win_par,max_par);
              // Recursion
              w_stack_.emplace_back(0,0,0,min_win_par-1);
              // Others attracted will have higher counts in subgame
              break;
            }
            case(1):
            {
              unsigned rd = this_work.rd;
              unsigned min_win_par = this_work.min_par;
              unsigned max_par = this_work.max_par;
              assert((min_win_par&1)==(max_par&1));
              bool p = min_win_par&1;
              // Check if the attractor of w_[!p] is equal to w_[!p]
              // if so, player wins if there remain accepting transitions
              // for max_par (see fix_strat_acc)
              // This does not increase but reuse rd
              bool grown = attr(rd, !p, max_par, false, min_win_par);
              // todo investigate: A is an attractor, so the only way that
              // attr(w[!p]) != w[!p] is if the max par "exit" edges lead
              // to a trap for player/ exit the winning region of the player
              // so we can do a fast check instead of the generic attr computation
              // which only needs to be done if the fast check is positive
          
              // Check if strategy needs to be fixed / is fixable
              if (!grown)
                // this only concerns parity accepting edges
                grown = fix_strat_acc(rd, p, min_win_par, max_par);
              // If !grown we are done, and the partitions are valid
              
              if (grown)
              {
                // Reset current game without !p attractor
                for (unsigned v : c_scc_->states())
                  if (!w_(v, !p) && (subgame_[v] >= rd))
                  {
                    // delete ownership
                    w_.unset(v);
                    // Mark as unseen
                    subgame_[v] = unseen_mark;
                    // Unset strat for testing
                    s_[v] = -1;
                  }
                w_stack_.emplace_back(0,0,0,max_par);
                // No need to do anything else
                // the attractor of !p of this level is not changed
              }
              break;
            }
            default:{
              throw std::runtime_error("No valid workstep");
            }
          }
          //Done
        }
      } // zielonka
      
      // Undo change to the graph made along the way
      inline void restore()
      {
        // "Unfix" the edges leaving the sccs
        for (auto& e_stash : change_stash_)
        {
          auto& e = arena_->edge_storage(e_stash.e_num);
          e.dst = e_stash.e_dst;
          e.acc = e_stash.e_acc;
        }
        // Done
      }
      
      // Empty all internal variables
      inline void clean_up()
      {
        subgame_.clear();
        w_.has_winner_.clear();
        w_.winner_.clear();
        s_.clear();
        rd_ = 0;
        max_abs_par_ = 0;
        change_stash_.clear();
      }
      
      // Dedicated solver for special cases
      inline void one_par_subgame_solver(const subgame_info_t& info,
                                         unsigned max_par)
      {
        assert(info.all_parities.size()==1);
        // The entire subgame is won by the player of the only parity
        // Any edge will do
        // todo optim for smaller circuit
        one_parity_count_++;
        // This subgame gets its own counter
        ++rd_;
        unsigned rd = rd_;
        unsigned one_par = *info.all_parities.begin();
        bool winner = one_par&1;
//        std::cout << one_par << ", " << max_par << std::endl;
        assert(one_par<=max_par);
  
        for (unsigned v : c_scc_->states())
        {
          if (subgame_[v] != unseen_mark)
            continue;
          // State of the subgame
          subgame_[v] = rd;
          w_.set(v,winner);
          // Get the strategy
          assert(s_[v] == -1);
          for (const auto& e : arena_->out(v))
          {
            unsigned this_par = e.acc.max_set() - 1;
            if ((subgame_[e.dst] >= rd) && (this_par<=max_par))
            {
              assert( this_par == one_par);
              // Ok for strat
              s_[v] = arena_->edge_number(e);
              break;
            }
          }
          assert((0<s_[v]) && (s_[v]<unseen_mark));
        }
        // Done
      }
      // todo one player arena
      
      // Member variables
      const unsigned unseen_mark = std::numeric_limits<unsigned>::max();
      
      // "Global" variables
      twa_graph_ptr arena_;
      const std::vector<bool>* owner_ptr_;
      unsigned rd_;
      winner_t w_;
      // Subgame array similar to the one from oink!
      std::vector<unsigned> subgame_;
      // strategies for env and player; For synthesis only player is needed
      // We need a signed value here in order to "fix" the strategy
      // during construction
      std::vector<long long> s_;
      
      // Informations about sccs andthe current scc
      scc_info* info_;
      unsigned max_abs_par_; // Max parity occuring in the current scc
      // Info on the current scc
      std::vector<scc_info_node>::const_iterator c_scc_;
      unsigned c_scc_idx_;
      // Fixes made to the sccs that have to be undone
      // before returning
      std::vector<edge_stash_t> change_stash_;
      size_t call_count_, direct_solve_count_, one_parity_count_;
      // Change recursive calls to stack
      std::vector<work_t> w_stack_;
    } pg_solver;
    
  } // parity_game
  
  namespace pg = parity_game;
  
  std::pair<twa_graph_ptr, std::vector<bool>>
  parse_pg_file(const std::string& fname)
  {
    std::ifstream pgfile(fname);
    std::string this_line;
    const char* substr_s;
    char* substr_e;
    
    // First line expected to contain max-parity
    std::getline(pgfile, this_line);
    if (this_line.find_first_of("parity ")!=0)
      throw std::runtime_error
          ("First line expected to contain parity");
    
    // pgsolver accepts color 0, we do not, so shift all parities by 2
//    std::cout << this_line << std::endl;
    unsigned max_par = (unsigned) std::strtoll(this_line.c_str()+7, &substr_e,
                                               10)+2;
    
    // Dummy dict, all transition get cond=true
    bdd_dict_ptr bdd_dict = make_bdd_dict();
    std::pair<twa_graph_ptr, std::vector<bool>> res;
    res.first = make_twa_graph(bdd_dict);
    std::vector<bool>& owner = res.second;
    
    res.first->set_acceptance(max_par,
                                 acc_cond::acc_code::parity_max_odd(max_par));
    // Loop once to get the number of states
    unsigned n_states=0;
    while (std::getline(pgfile, this_line))
      n_states++;
    
    res.first->new_states(n_states);
    owner.resize(n_states);
  
    // Loop again
    pgfile.close();
    pgfile = std::ifstream(fname);
    std::getline(pgfile, this_line);
    
    while (std::getline(pgfile, this_line))
    {
      substr_s = this_line.c_str();
      unsigned src = (unsigned) std::strtoll(substr_s, &substr_e, 10);
      substr_s = substr_e;
      unsigned parity = (unsigned) std::strtoll(substr_s, &substr_e, 10)+2;
      substr_s = substr_e;
      owner[src] = (bool) std::strtol(substr_s, &substr_e, 10);
      substr_s = substr_e;
      // Loop over all succ
      // Note that in order to be a game, there has to be
      // at least one successor
      while (true)
      {
        unsigned dst = (unsigned) std::strtoll(substr_s, &substr_e, 10);
        substr_s = substr_e;
        res.first->new_edge(src, dst, bddtrue,
                            acc_cond::mark_t({parity}));
        if (*substr_s==','){
          substr_s++;
        }else{
          // Next is symbolic (pgsolver format)
          // Or "nextline" (oink! format)
          break;
        }
      }
    }
    pgfile.close();
    res.first->set_named_prop("state-player",
                           new std::vector<bool>(std::move(owner)));
    res.first->set_named_prop("winning-region",
                          new std::vector<bool>());
    res.first->set_named_prop("strategy",
                          new std::vector<unsigned>());
  
    return res;
  }
  
  void
  make_arena(twa_graph_ptr& arena)
  {
    auto um = arena->acc().unsat_mark();
    if (!um.first)
      throw std::runtime_error("game winning condition is a tautology");
    
    std::vector<bool> seen(arena->num_states(), false);
    std::vector<unsigned> todo({arena->get_init_state_number()});
    std::vector<bool> owner(arena->num_states()+10, false);
    owner[arena->get_init_state_number()] = false;
    
    unsigned sink_env=0, sink_con=0;
    
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
        if (owner[src])
          missing = bddfalse;
        else
          missing -= e.cond;
        if (!seen[e.dst]) {
          owner[e.dst] = !owner[src];
          todo.push_back(e.dst);
        }else if(e.dst==e.src){
          // Self-loops can occur when using sd
          // Split into new state and edge
          if (arena->num_states() == owner.size())
            owner.resize(owner.size()+20, false);
          unsigned intermed = arena->new_state();
//          std::cout << "create " << intermed << std::endl;
          e.dst = intermed;
          arena->new_edge(intermed, e.src, bddtrue, e.acc);
          owner.at(intermed) = !owner[e.src];
        }else{
//          std::cout << e.src << " ; " << e.dst << " ; " << e.cond << " ; "
//                    << e.acc << " ; " << owner[e.src] << ","
//                    << owner[e.dst] << " ; " << arena->num_states()
//                    << std::endl;
          assert((owner[e.dst] != owner[src])
                      && "Illformed arena!");
        }
      }
      if (owner[src] && (missing == bddtrue))
      {
        // The owned state has no successors
        // Create edge to sink_env
        if (sink_env == 0)
        {
          sink_env = arena->new_state();
          sink_con = arena->new_state();
//          std::cout << "ss " << sink_env << ", " << sink_con << std::endl;
          arena->new_edge(sink_con, sink_env, bddtrue, um.second);
          arena->new_edge(sink_env, sink_con, bddtrue, um.second);
          owner.at(sink_env) = false;
          owner.at(sink_con) = true;
        }
        arena->new_edge(src, sink_env, bddtrue, um.second);
      }
      if (!owner[src] && (missing != bddfalse))
      {
        if (sink_env == 0)
        {
          sink_env = arena->new_state();
          sink_con = arena->new_state();
//          std::cout << "ss " << sink_env << ", " << sink_con << std::endl;
          arena->new_edge(sink_con, sink_env, bddtrue, um.second);
          arena->new_edge(sink_env, sink_con, bddtrue, um.second);
          owner.at(sink_env) = false;
          owner.at(sink_con) = true;
        }
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
  
  bool
  solve_parity_game_old(const twa_graph_ptr& arena,
                        const std::vector<bool>& owner,
                        pg::region_old_t (&w)[2], pg::strategy_old_t (&s)[2])
  {
    pg::region_old_t states;
    for (unsigned i = 0; i < arena->num_states(); ++i)
      states.insert(i);
    
    unsigned max_parity = 0;
    for (const auto& e: arena->edges())
      max_parity = std::max(max_parity, e.acc.max_set());
    assert(max_parity);
    max_parity -= 1;
    
    pg::solve_rec_old(arena, owner, states, max_parity, w, s);
    
    return w[1].count(arena->get_init_state_number());
  }
  
  bool solve_parity_game(const twa_graph_ptr& arena)
  {
    return pg::pg_solver.solve(arena);
  }
  
  twa_graph_ptr
  apply_strategy(const twa_graph_ptr& arena, bdd all_outputs,
                 bool do_purge, bool unsplit, bool keep_acc, bool leave_choice)
  {
    assert(is_solved_arena(arena));
    namespace pg = spot::parity_game;
  
    if (!arena->get_named_prop<pg::region_t>("winning-region")
        || !arena->get_named_prop<pg::strategy_t>("strategy")
        || !arena->get_named_prop<std::vector<bool>>("state-player"))
      throw std::runtime_error("Arena missing winning-region, strategy "
                               "or state-player");
    
    pg::region_t& w =
        *arena->get_named_prop<pg::region_t>("winning-region");
    pg::strategy_t& s =
        *arena->get_named_prop<pg::strategy_t>("strategy");
    
    // If we purge, unsplit but not keep_acc we store the distinct
    // parts of the condition as chances are we are using ltlsynt
//    bool store_e = !do_purge && unsplit && !keep_acc;
//    std::vector<std::array<bdd,2>> e_conds;
//    if (store_e)
//      e_conds.push_back({bddfalse, bddfalse}); //Zero edge
    
    auto aut = spot::make_twa_graph(arena->get_dict());
    aut->copy_ap_of(arena);
    if (keep_acc){
      aut->copy_acceptance_of(arena);
    }else{
      aut->set_acceptance(0, acc_cond::acc_code());
    }
  
    const unsigned unseen_mark = std::numeric_limits<unsigned>::max();
    std::vector<unsigned> todo{arena->get_init_state_number()};
    std::vector<unsigned> pg2aut(arena->num_states(), unseen_mark);
    aut->set_init_state(aut->new_state());
    pg2aut[arena->get_init_state_number()] = aut->get_init_state_number();
  
    std::unordered_set<bdd, bdd_hash> n_minterms;
    while (!todo.empty()) {
      unsigned v = todo.back();
      todo.pop_back();
      // Env edge -> keep all
      for (auto &e1: arena->out(v)) {
        assert(w.at(e1.dst));
        if (!unsplit)
        {
          if(pg2aut[e1.dst]==unseen_mark)
            pg2aut[e1.dst] = aut->new_state();
          aut->new_edge(pg2aut[v], pg2aut[e1.dst],
                        e1.cond, keep_acc ? e1.acc : acc_cond::mark_t({}));
        }
        // Player strat
        // Only colors on player edge are important,
        // as env edges have been colorized afterwards
        // todo leave choice to aiger in order to minimize and-gates
        auto &e2 = arena->edge_storage(s[e1.dst]);
        if (pg2aut[e2.dst] == unseen_mark) {
          pg2aut[e2.dst] = aut->new_state();
          todo.push_back(e2.dst);
        }
        bdd out;
        if (leave_choice)
          // Leave the choice
          out = e2.cond;
        else
          // Save only one letter
          out = bdd_satoneset(e2.cond, all_outputs, bddfalse);
        n_minterms.insert(out);
        
        aut->new_edge(unsplit ? pg2aut[v] : pg2aut[e1.dst],
                      pg2aut[e2.dst],
                      unsplit ? (e1.cond & out):out,
                      keep_acc ? e2.acc : acc_cond::mark_t({}));
      }
    }
    // When doing the normal synthesis, do not purge states
    // to keep the graph as is
    // the translation to aiger will run a DFS to only acount
    // for reachable states
    if (do_purge) {
      aut->purge_dead_states();
    }
    aut->set_named_prop("synthesis-outputs", new bdd(all_outputs));
    return aut;
  }
  
}//spot
