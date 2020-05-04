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

#include <deque>

namespace spot
{
  
  parity_game::parity_game(const twa_graph_ptr& arena,
                           const std::vector<bool>& owner)
      : arena_(arena)
      , owner_(owner)
  {
    bool max, odd;
    arena_->acc().is_parity(max, odd, true);
    if (!(max && odd))
      throw std::runtime_error("arena must have max-odd acceptance condition");
    
    auto check_colorized = [&]()
    {
      for (const auto& e : arena_->edges())
        if (e.acc.max_set() == 0)
          throw std::runtime_error("arena must be colorized");
      return true;
    };
    SPOT_ASSERT( check_colorized() );
    
    assert(owner_.size() == arena_->num_states());
  }
  
  void parity_game::print(std::ostream& os)
  {
    os << "parity " << num_states() - 1 << ";\n";
    std::vector<bool> seen(num_states(), false);
    std::vector<unsigned> todo({get_init_state_number()});
    while (!todo.empty())
    {
      unsigned src = todo.back();
      todo.pop_back();
      if (seen[src])
        continue;
      seen[src] = true;
      os << src << ' ';
      os << out(src).begin()->acc.max_set() - 1 << ' ';
      os << owner(src) << ' ';
      bool first = true;
      for (auto& e: out(src))
      {
        if (!first)
          os << ',';
        first = false;
        os << e.dst;
        if (!seen[e.dst])
          todo.push_back(e.dst);
      }
      if (src == get_init_state_number())
        os << " \"INIT\"";
      os << ";\n";
    }
  }
  
  bool parity_game::solve(region_t (&w)[2], strategy_t (&s)[2]) const
  {
    region_t states_;
    for (unsigned i = 0; i < num_states(); ++i)
      states_.insert(i);
    unsigned m = max_parity();
    solve_rec(states_, m, w, s);
    return w[1].count(get_init_state_number());
  }
  
  parity_game::strategy_t
  parity_game::attractor(const region_t& subgame, region_t& set,
                         unsigned max_parity, int p, bool attr_max) const
  {
    strategy_t strategy;
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
        
        bool is_owned = owner_[s] == p;
        bool wins = !is_owned;
        
        for (const auto& e: out(s))
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
  }
  
  void parity_game::solve_rec(region_t& subgame, unsigned max_parity,
                              region_t (&w)[2], strategy_t (&s)[2]) const
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
    region_t u;
    auto strat_u = attractor(subgame, u, max_parity, p, true);
    
    if (max_parity == 0)
    {
      s[p] = std::move(strat_u);
      w[p] = std::move(u);
      // FIXME what about w[!p]?
      return;
    }
    
    for (unsigned s: u)
      subgame.erase(s);
    region_t w0[2]; // Player's winning region in the first recursive call.
    strategy_t s0[2]; // Player's winning strategy in the first recursive call.
    solve_rec(subgame, max_parity - 1, w0, s0);
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
    auto strat_wnp = attractor(subgame, w0[!p], max_parity, !p);
    
    for (unsigned s: w0[!p])
      subgame.erase(s);
    
    region_t w1[2]; // Odd's winning region in the second recursive call.
    strategy_t s1[2]; // Odd's winning strategy in the second recursive call.
    solve_rec(subgame, max_parity, w1, s1);
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
  }
  
  void
  parity_game::set_up()
  {
    // Alloc
    subgame_.clear();
    subgame_.resize(num_states(), unseen_mark);
    w_.has_winner_.clear();
    w_.has_winner_.resize(num_states(), 0);
    w_.winner_.clear();
    w_.winner_.resize(num_states(), 0);
    s_.clear();
    s_.resize(num_states(), -1);
    
    // Init
    rd_ = 0;
    
    max_abs_par_ = arena_->get_acceptance().used_sets().max_set()-1;
    info_ = new scc_info(arena_);
    // Every edge leaving an scc needs to be "fixed"
    // at some point.
    // We store: number of edge fixed, original dst, original acc
    change_stash_.reserve(info_->scc_count()*2);
    
    call_count_=0;
    direct_solve_count_=0;
    one_parity_count_=0;
  }
  
  bool parity_game::solve_alt(region_alt_t& w, strategy_alt_t& s)
  {
    // todo check if reordering states according to scc is worth it
    set_up();
    // Start recursive zielonka
    // Is this scc empty
//    bool is_empty;
    // All parities appearing in the scc
//    std::set<unsigned, std::greater<unsigned>> all_parities;
    subgame_info_t subgame_info;
    c_scc_idx_ = 0;
    for (c_scc_ = info_->begin(); c_scc_ != info_->end();
         c_scc_++, c_scc_idx_++)
    {
//      std::tie(is_empty, all_parities) = fix_scc();
      subgame_info = fix_scc();
      if (!subgame_info.is_empty) // If empty, the scc was trivially solved
      {
        if (subgame_info.is_one_parity){
          one_par_subgame_solver(subgame_info);
        }else {
          // The scc is empty if it can be trivially solved
          max_abs_par_ = *subgame_info.all_parities.begin();
          w_stack_.emplace_back(0, 0, 0, max_abs_par_);
          zielonka();
        }
      }
    }
    // All done -> restore graph, i.e. undone self-looping
    restore();
    
    SPOT_ASSERT(std::all_of(w_.has_winner_.cbegin(), w_.has_winner_.cend(),
                            [](bool b){return b;}));
    SPOT_ASSERT(std::all_of(s_.cbegin(), s_.cend(),
                            [](unsigned e_idx){return e_idx>0;}));
    
    w.swap(w_.winner_);
    s.resize(s_.size());
    std::copy(s_.begin(), s_.end(), s.begin());
    
    free(info_);
    
    std::cerr << "cc " << call_count_ << " ds " << direct_solve_count_
        << " op " << one_parity_count_ << "\n";
    return w[get_init_state_number()];
  }
  
  inline bool parity_game::attr(unsigned& rd, bool p, unsigned max_par,
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
    
    SPOT_ASSERT((!acc_par) || (acc_par && (max_par&1)==p));
    SPOT_ASSERT(!acc_par || (0<min_win_par));
    SPOT_ASSERT((min_win_par<=max_par) && (max_par<=max_abs_par_));
    
    bool grown = false;
    // We could also directly mark states as owned,
    // instead of adding them to to_add first,
    // possibly reducing the number of iterations.
    // However by making the algorithm complete a loop
    // before adding it is like a backward bfs and (generally) reduces
    // the size of the strategy
    static std::vector<unsigned> to_add;
    
    SPOT_ASSERT(to_add.empty());
    
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
            SPOT_ASSERT(subgame_[v] == unseen_mark);
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
        
        bool is_owned = owner_[v] == p;
        bool wins = !is_owned;
        // Loop over out-going
        
        // Optim: If given the choice,
        // we seek to go to the "oldest" subgame
        // That is the subgame with the lowest rd value
        unsigned min_subgame_idx = -1u;
        for (const auto& e: out(v))
        {
          unsigned this_par = e.acc.max_set() - 1;
          if ((subgame_[e.dst]>=rd) && (this_par<=max_par))
          {
            // Check if winning
            if (w_(e.dst, p) || (acc_par && (min_win_par <= this_par)))
            {
              SPOT_ASSERT(!acc_par || (this_par < min_win_par) ||
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
    
    SPOT_ASSERT(to_add.empty());
    return grown;
  }
  
  // We need to check if transitions that are accepted due
  // to their parity remain in the winning region of p
  inline bool
  parity_game::fix_strat_acc(unsigned rd, bool p, unsigned min_win_par,
                             unsigned max_par)
  {
    
    for (unsigned v : c_scc_->states()){
      // Only current attractor and owned and winning vertices are concerned
      if ((subgame_[v]!=rd) || !w_(v,p) || (owner_[v]!=p) || (s_[v]>0)){
        continue;
      }
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
      SPOT_ASSERT(min_win_par <= e_s.acc.max_set() - 1);
      SPOT_ASSERT(e_s.acc.max_set() - 1 <= max_par);
      
      // Strategy heuristic : go to the oldest subgame
      unsigned min_subgame_idx = -1u;
      
      s_[v] = -1;
      for (const auto & e_fix : out(v))
      {
        if (subgame_[e_fix.dst]>=rd){
          unsigned this_par = e_fix.acc.max_set() - 1;
          // This edge must have less than max_par, otherwise it would have already been attracted
          SPOT_ASSERT((this_par<=max_par) || ((this_par&1)!=(max_par&1)));
          // if it is accepting and leads to the winning region
          // -> valid fix
          if ((min_win_par<=this_par) && (this_par<=max_par) && w_(e_fix.dst, p))
          {
            if (subgame_[e_fix.dst] < min_subgame_idx)
            {
              // Max par edge to older subgame found
              s_[v] = arena_->edge_number(e_fix);
              min_subgame_idx = subgame_[e_fix.dst];
            }
          }
        }
      }
      if (s_[v] == -1){
        // NO fix found
        // This state is NOT won by p
        return true; // true -> grown
      }
    }
    // Nothing to fix or all fixed
    return false; // false -> not grown all good
  }
  
  inline void parity_game::zielonka()
  {
    while(!w_stack_.empty())
    {
      auto this_work = w_stack_.back();
      w_stack_.pop_back();
      
      switch(this_work.wstep)
      {
        case(0):
        {
          SPOT_ASSERT(this_work.rd==0);
          SPOT_ASSERT(this_work.min_par==0);
          
          unsigned rd;
          SPOT_ASSERT(this_work.max_par <= max_abs_par_);
          call_count_++;
          
          // Check if empty and get parities
//          std::set<unsigned, std::greater<unsigned>> all_parities;
//          bool is_empty;
//          std::tie(is_empty, all_parities) = inspect_scc(this_work.max_par);
          subgame_info_t subgame_info = inspect_scc(this_work.max_par);
          
          if (subgame_info.is_empty)
            // Nothing to do
            break;
          if (subgame_info.is_one_parity)
          {
            // Can be trivially solved
            one_par_subgame_solver(subgame_info);
            break;
          }
          
          
          // Compute the winning parity boundaries
          // -> Priority compression
          // Optional improves performance
          // Highest actually occurring
          unsigned max_par = *subgame_info.all_parities.begin();
          unsigned min_win_par = max_par;
          while ((min_win_par > 2)
                 && (!subgame_info.all_parities.count(min_win_par - 1)))
            min_win_par -= 2;
          SPOT_ASSERT(max_par > 0);
          SPOT_ASSERT(!subgame_info.all_parities.empty());
          SPOT_ASSERT(min_win_par > 0);
          
          // Get the player
          bool p = min_win_par & 1;
          SPOT_ASSERT((max_par & 1) == (min_win_par & 1));
          // Attraction to highest par
          // This increases rd_ and passes it to rd
          attr(rd, p, max_par, true, min_win_par);
          // All those attracted get subgame_[v] <- rd
          
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
          SPOT_ASSERT((min_win_par&1)==(max_par&1));
          bool p = min_win_par&1;
          // Check if the attractor of w_[!p] is equal to w_[!p]
          // if so, player wins
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
  
  inline void parity_game::restore(){
    // "Unfix" the edges leaving the sccs
    for (auto& e_stash : change_stash_){
      auto& e = arena_->edge_storage(e_stash.e_num);
      e.dst = e_stash.e_dst;
      e.acc = e_stash.e_acc;
    }
    // Done
  }
  
  // Checks if an scc is empty and reports the occurring parities
  inline parity_game::subgame_info_t
  parity_game::inspect_scc(unsigned max_par)
  {
//    std::pair<bool, std::set<unsigned, std::greater<unsigned>>> ret_val;
    subgame_info_t info;
//    bool& is_empty = ret_val.first;
//    std::set<unsigned, std::greater<unsigned>>& all_parities = ret_val.second;
    
    info.is_empty = true;
    info.is_one_player0 = true;
    info.is_one_player1 = true;
    for (unsigned v : c_scc_->states())
    {
      if (subgame_[v] != unseen_mark)
        continue;
      info.is_empty = false;
      
      bool multi_edge = false;
      for (const auto &e : out(v))
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
        if (owner_[v])
        {
          info.is_one_player1 = false;
        }else {
          info.is_one_player0 = false;
        }
      }
    } // v
    SPOT_ASSERT(info.all_parities.size()
                || (!info.all_parities.size() && info.is_empty));
    info.is_one_parity = info.all_parities.size()==1;
    // Done
    return info;
  }
  
  // Checks if an scc can be trivially solved
  inline parity_game::subgame_info_t
  parity_game::fix_scc()
  {
    auto scc_acc = info_->acc_sets_of(c_scc_idx_);
    // We will override all parities of edges leaving the scc
    bool added[]={false, false};
    acc_cond::mark_t odd_mark, even_mark;
    unsigned par_pair[2];
    unsigned scc_new_par = std::max(scc_acc.max_set(),1u);
    if (scc_new_par&1)
    {
      par_pair[1]=scc_new_par; par_pair[0]=scc_new_par+1;
    }else{
      par_pair[1]=scc_new_par+1; par_pair[0]=scc_new_par;
    }
    even_mark = acc_cond::mark_t({par_pair[0]});
    odd_mark = acc_cond::mark_t({par_pair[1]});
    
    // Only necessary to pass tests
    max_abs_par_ = std::max(par_pair[0], par_pair[1]);
    
    for (unsigned v : c_scc_->states())
    {
      SPOT_ASSERT(subgame_[v] == unseen_mark);
      for (auto &e : out(v))
      {
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
    }
    
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
  }
  
  inline void parity_game::one_par_subgame_solver(const subgame_info_t& info)
  {
    // The entire subgame is won by the player of the only parity
    // Any edge will do
    // todo optim for smaller circuit
    one_parity_count_++;
    // This subgame gets its own counter
    ++rd_;
    unsigned rd = rd_;
    unsigned one_par = *info.all_parities.begin();
    bool winner = one_par&1;
    
    for (unsigned v : c_scc_->states())
    {
      if (subgame_[v] != unseen_mark)
        continue;
      // State of the subgame
      subgame_[v] = rd;
      w_.set(v,winner);
      // Get the strategy
      SPOT_ASSERT(s_[v] == -1);
      for (const auto& e : out(v))
      {
        if ((subgame_[e.dst] >= rd)
            && ((e.acc.max_set() - 1)== one_par))
        {
          // Ok for strat
          s_[v] = arena_->edge_number(e);
          break;
        }
      }
      SPOT_ASSERT((0<s_[v]) && (s_[v]<unseen_mark));
    }
    // Done
  }
  
}//spot
