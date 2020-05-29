// -*- coding: utf-8 -*-
// Copyright (C) 2017-2019 Laboratoire de Recherche et Développement
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
#include <spot/twaalgos/aiger.hh>

#include <cmath>
#include <deque>
#include <map>
#include <unordered_map>
#include <vector>
#include <sstream>

#include <spot/twa/twagraph.hh>
#include <spot/misc/bddlt.hh>
#include <spot/misc/minato.hh>

namespace spot
{
  namespace
  {
    static std::vector<std::string>
    name_vector(unsigned n, const std::string& prefix)
    {
      std::vector<std::string> res(n);
      for (unsigned i = 0; i != n; ++i)
        res[i] = prefix + std::to_string(i);
      return res;
    }
    
    // A class to represent an AIGER circuit
    class aig
    {
    private:
      unsigned num_inputs_;
      unsigned max_var_;
      std::map<unsigned, std::pair<unsigned, unsigned>> and_gates_;
      std::vector<unsigned> latches_;
      std::vector<unsigned> outputs_;
      std::vector<std::string> input_names_;
      std::vector<std::string> output_names_;
      // Cache the function computed by each variable as a bdd.
      std::unordered_map<unsigned, bdd> var2bdd_;
      std::unordered_map<bdd, unsigned, bdd_hash> bdd2var_;
    
    public:
      aig(const std::vector<std::string>& inputs,
          const std::vector<std::string>& outputs,
          unsigned num_latches)
          : num_inputs_(inputs.size()),
            max_var_((inputs.size() + num_latches)*2),
            latches_(num_latches),
            outputs_(outputs.size()),
            input_names_(inputs),
            output_names_(outputs)
      {
        bdd2var_[bddtrue] = 1;
        var2bdd_[1] = bddtrue;
        bdd2var_[bddfalse] = 0;
        var2bdd_[0] = bddfalse;
      }
      
      aig(unsigned num_inputs, unsigned num_latches, unsigned num_outputs)
          : aig(name_vector(num_inputs, "in"), name_vector(num_outputs, "out"),
                num_latches)
      {}
      
      unsigned input_var(unsigned i, bdd b)
      {
        assert(i < num_inputs_);
        unsigned v = (1 + i) * 2;
        bdd2var_[b] = v;
        var2bdd_[v] = b;
        return v;
      }
      
      unsigned latch_var(unsigned i, bdd b)
      {
        assert(i < latches_.size());
        unsigned v = (1 + num_inputs_ + i) * 2;
        bdd2var_[b] = v;
        var2bdd_[v] = b;
        return v;
      }
      
      void set_output(unsigned i, unsigned v)
      {
        outputs_[i] = v;
      }
      
      void set_latch(unsigned i, unsigned v)
      {
        latches_[i] = v;
      }
      
      unsigned aig_true() const
      {
        return 1;
      }
      
      unsigned aig_false() const
      {
        return 0;
      }
      
      unsigned aig_not(unsigned v)
      {
        unsigned not_v = v ^ 1;
        assert(var2bdd_.count(v));
        var2bdd_[not_v] = !(var2bdd_[v]);
        bdd2var_[var2bdd_[not_v]] = not_v;
        return not_v;
      }
      
      unsigned aig_and(unsigned v1, unsigned v2)
      {
        assert(var2bdd_.count(v1));
        assert(var2bdd_.count(v2));
        bdd b = var2bdd_[v1] & var2bdd_[v2];
        auto it = bdd2var_.find(b);
        if (it != bdd2var_.end())
          return it->second;
        max_var_ += 2;
        and_gates_[max_var_] = {v1, v2};
        bdd2var_[b] = max_var_;
        var2bdd_[max_var_] = b;
        return max_var_;
      }
      
      unsigned aig_and(std::vector<unsigned> vs)
      {
        if (vs.empty())
          return aig_true();
        if (vs.size() == 1)
          return vs[0];
        if (vs.size() == 2)
          return aig_and(vs[0], vs[1]);
        unsigned m = vs.size() / 2;
        unsigned left =
            aig_and(std::vector<unsigned>(vs.begin(), vs.begin() + m));
        unsigned right =
            aig_and(std::vector<unsigned>(vs.begin() + m, vs.end()));
        return aig_and(left, right);
      }
      
      unsigned aig_or(unsigned v1, unsigned v2)
      {
        unsigned n1 = aig_not(v1);
        unsigned n2 = aig_not(v2);
        return aig_not(aig_and(n1, n2));
      }
      
      unsigned aig_or(std::vector<unsigned> vs)
      {
        for (unsigned i = 0; i < vs.size(); ++i)
          vs[i] = aig_not(vs[i]);
        return aig_not(aig_and(vs));
      }
      
      unsigned aig_pos(unsigned v)
      {
        
        return v & ~1;
      }
      
      void remove_unused()
      {
        // Run a DFS on the gates and latches to collect
        // all nodes connected to the output.
        std::deque<unsigned> todo;
        std::vector<bool> used(max_var_ / 2 + 1, false);
        auto mark = [&](unsigned v)
        {
          unsigned pos = aig_pos(v);
          if (!used[pos/2])
          {
            used[pos/2] = 1;
            todo.push_back(pos);
          }
        };
        for (unsigned v : outputs_)
          mark(v);
        while (!todo.empty())
        {
          unsigned v = todo.front();
          todo.pop_front();
          auto it_and = and_gates_.find(v);
          if (it_and != and_gates_.end())
          {
            mark(it_and->second.first);
            mark(it_and->second.second);
          }
          else if (v <= (num_inputs_ + latches_.size()) * 2
                   && v > num_inputs_ * 2)
          {
            mark(latches_[v / 2 - num_inputs_ - 1]);
          }
        }
        // Erase and_gates that were not seen in the above
        // exploration.
        auto e = and_gates_.end();
        for (auto i = and_gates_.begin(); i != e;)
        {
          if (!used[i->first/2])
            i = and_gates_.erase(i);
          else
            ++i;
        }
      }
      
      void
      print(std::ostream& os) const
      {
        os << "aag " << max_var_ / 2
           << ' ' << num_inputs_
           << ' ' << latches_.size()
           << ' ' << outputs_.size()
           << ' ' << and_gates_.size() << '\n';
        for (unsigned i = 0; i < num_inputs_; ++i)
          os << (1 + i) * 2 << '\n';
        for (unsigned i = 0; i < latches_.size(); ++i)
          os << (1 + num_inputs_ + i) * 2 << ' ' << latches_[i] << '\n';
        for (unsigned i = 0; i < outputs_.size(); ++i)
          os << outputs_[i] << '\n';
        for (auto& p : and_gates_)
          os << p.first
             << ' ' << p.second.first
             << ' ' << p.second.second << '\n';
        for (unsigned i = 0; i < num_inputs_; ++i)
          os << 'i' << i << ' ' << input_names_[i] << '\n';
        for (unsigned i = 0; i < outputs_.size(); ++i)
          os << 'o' << i << ' ' << output_names_[i] << '\n';
      }
    };
    
    static std::vector<bool>
    state_to_vec(unsigned s, unsigned size)
    {
      std::vector<bool> v(size);
      for (unsigned i = 0; i < size; ++i)
      {
        v[i] = s & 1;
        s >>= 1;
      }
      return v;
    }
    
    static std::vector<bool>
    output_to_vec(bdd b,
                  const std::unordered_map<unsigned, unsigned>& bddvar_to_outputnum)
    {
      std::vector<bool> v(bddvar_to_outputnum.size());
      while (b != bddtrue && b != bddfalse)
      {
        unsigned i = bddvar_to_outputnum.at(bdd_var(b));
        v[i] = (bdd_low(b) == bddfalse);
        if (v[i])
          b = bdd_high(b);
        else
          b = bdd_low(b);
      }
      return v;
    }
    
    static bdd
    state_to_bdd(unsigned s, bdd all)
    {
      bdd b = bddtrue;
      unsigned size = bdd_nodecount(all);
      if (size)
      {
        unsigned st0 = bdd_var(all);
        for (unsigned i = 0; i < size; ++i)
        {
          b &= (s & 1)  ? bdd_ithvar(st0 + i) : bdd_nithvar(st0 + i);
          s >>= 1;
        }
      }
      return b;
    }
    
    // Switch initial state and 0 in the AIGER encoding, so that the
    // 0-initialized latches correspond to the initial state.
    static unsigned
    encode_init_0(unsigned src, unsigned init)
    {
      return src == init ? 0 : src == 0 ? init : src;
    }
    
    // Transforms an automaton into an AIGER circuit
    static aig
    aut_to_aiger(const const_twa_graph_ptr& aut, const bdd& all_outputs)
    {
      // The aiger circuit cannot encode the acceptance condition
      // Test that the acceptance condition is true
      if (!aut->acc().is_t())
        throw std::runtime_error("Cannot turn automaton into aiger circuit");
      
      // Encode state in log2(num_states) latches.
      // TODO how hard is it to compute the binary log of a binary integer??
      unsigned log2n = std::ceil(std::log2(aut->num_states()));
      unsigned st0 = aut->get_dict()->register_anonymous_variables(log2n, aut);
      bdd all_states = bddtrue;
      for (unsigned i = 0; i < log2n; ++i)
        all_states &= bdd_ithvar(st0 + i);
      
      std::vector<std::string> input_names;
      std::vector<std::string> output_names;
      bdd all_inputs = bddtrue;
      std::unordered_map<unsigned, unsigned> bddvar_to_inputnum;
      std::unordered_map<unsigned, unsigned> bddvar_to_outputnum;
      for (const auto& ap : aut->ap())
      {
        int bddvar = aut->get_dict()->has_registered_proposition(ap, aut);
        assert(bddvar >= 0);
        bdd b = bdd_ithvar(bddvar);
        if (bdd_implies(all_outputs, b)) // ap is an output AP
        {
          bddvar_to_outputnum[bddvar] = output_names.size();
          output_names.emplace_back(ap.ap_name());
        }
        else // ap is an input AP
        {
          bddvar_to_inputnum[bddvar] = input_names.size();
          input_names.emplace_back(ap.ap_name());
          all_inputs &= b;
        }
      }
      
      unsigned num_outputs = bdd_nodecount(all_outputs);
      unsigned num_latches = bdd_nodecount(all_states);
      unsigned init = aut->get_init_state_number();
      
      aig circuit(input_names, output_names, num_latches);
      bdd b;
      
      // Latches and outputs are expressed as a DNF in which each term
      // represents a transition.
      // latch[i] (resp. out[i]) represents the i-th latch (resp. output) DNF.
      std::vector<std::vector<unsigned>> latch(num_latches);
      std::vector<std::vector<unsigned>> out(num_outputs);
      for (unsigned s = 0; s < aut->num_states(); ++s)
        for (auto& e: aut->out(s))
        {
          minato_isop cond(e.cond);
          while ((b = cond.next()) != bddfalse)
          {
            bdd input = bdd_existcomp(b, all_inputs);
            bdd letter_out = bdd_existcomp(b, all_outputs);
            auto out_vec = output_to_vec(letter_out, bddvar_to_outputnum);
            unsigned dst = encode_init_0(e.dst, init);
            auto next_state_vec = state_to_vec(dst, log2n);
            unsigned src = encode_init_0(s, init);
            bdd state_bdd = state_to_bdd(src, all_states);
            std::vector<unsigned> prod;
            while (input != bddfalse && input != bddtrue)
            {
              unsigned v =
                  circuit.input_var(bddvar_to_inputnum[bdd_var(input)],
                                    bdd_ithvar(bdd_var(input)));
              if (bdd_high(input) == bddfalse)
              {
                v = circuit.aig_not(v);
                input = bdd_low(input);
              }
              else
                input = bdd_high(input);
              prod.push_back(v);
            }
            
            while (state_bdd != bddfalse && state_bdd != bddtrue)
            {
              unsigned v =
                  circuit.latch_var(bdd_var(state_bdd) - st0,
                                    bdd_ithvar(bdd_var(state_bdd)));
              if (bdd_high(state_bdd) == bddfalse)
              {
                v = circuit.aig_not(v);
                state_bdd = bdd_low(state_bdd);
              }
              else
                state_bdd = bdd_high(state_bdd);
              prod.push_back(v);
            }
            unsigned t = circuit.aig_and(prod);
            for (unsigned i = 0; i < next_state_vec.size(); ++i)
              if (next_state_vec[i])
                latch[i].push_back(t);
            for (unsigned i = 0; i < num_outputs; ++i)
              if (out_vec[i])
                out[i].push_back(t);
          }
        }
      for (unsigned i = 0; i < num_latches; ++i)
        circuit.set_latch(i, circuit.aig_or(latch[i]));
      for (unsigned i = 0; i < num_outputs; ++i)
        circuit.set_output(i, circuit.aig_or(out[i]));
      circuit.remove_unused();
      return circuit;
    }
    
    
    
    
    
    
    
    
    
    
    
    // A class to represent an AIGER circuit
    class aig2
    {
    private:
      unsigned num_inputs_;
      unsigned max_var_;
//      std::map<unsigned, std::pair<unsigned, unsigned>> and_gates_;
      std::vector<std::pair<unsigned, unsigned>> and_gates_;
      std::vector<unsigned> latches_;
      std::vector<unsigned> outputs_;
      std::vector<std::string> input_names_;
      std::vector<std::string> output_names_;
      // Cache the function computed by each variable as a bdd.
      std::unordered_map<unsigned, bdd> var2bdd_;
      std::unordered_map<bdd, unsigned, bdd_hash> bdd2var_;
    
    public:
      aig2(const std::vector<std::string>& inputs,
           const std::vector<std::string>& outputs,
           unsigned num_latches)
          : num_inputs_(inputs.size()),
            max_var_((inputs.size() + num_latches)*2),
            latches_(num_latches),
            outputs_(outputs.size()),
            input_names_(inputs),
            output_names_(outputs)
      {
        bdd2var_[bddtrue] = 1;
        var2bdd_[1] = bddtrue;
        bdd2var_[bddfalse] = 0;
        var2bdd_[0] = bddfalse;
        
        bdd2var_.reserve(4*(num_inputs_+latches_.size()));
        var2bdd_.reserve(4*(num_inputs_+latches_.size()));
      }
      
      aig2(unsigned num_inputs, unsigned num_latches, unsigned num_outputs)
          : aig2(name_vector(num_inputs, "in"), name_vector(num_outputs, "out"),
                 num_latches)
      {}
      
      inline void register_new(unsigned v, const bdd& b)
      {
        SPOT_ASSERT(!var2bdd_.count(v) || var2bdd_.at(v)==b);
        SPOT_ASSERT(!bdd2var_.count(b)
                    ||bdd2var_.at(b)==v);
        var2bdd_[v] = b;
        bdd2var_[b] = v;
//        var2bdd_[aig_not(v)] = !b;
//        bdd2var_[!b] = aig_not(v);
      }
      
      inline unsigned input_var(unsigned i) const
      {
        assert(i < num_inputs_);
        unsigned v = (1 + i) * 2;
        return v;
      }
      
      inline unsigned latch_var(unsigned i)const
      {
        assert(i < latches_.size());
        unsigned v = (1 + num_inputs_ + i) * 2;
        return v;
      }
      
      inline unsigned gate_var(unsigned i)const
      {
        assert(i < and_gates_.size());
        unsigned v = (1 + num_inputs_ + latches_.size() + i) * 2;
        return v;
      }
      
      void set_output(unsigned i, unsigned v)
      {
        outputs_[i] = v;
      }
      
      void set_latch(unsigned i, unsigned v)
      {
        latches_[i] = v;
      }
      
      
      
      unsigned aig_true() const
      {
        return 1;
      }
      
      unsigned aig_false() const
      {
        return 0;
      }
      
      unsigned aig_not(unsigned v)
      {
        unsigned not_v = v ^ 1;
        if (!var2bdd_.count(not_v))
        {
          var2bdd_[not_v] = !(var2bdd_.at(v));
          bdd2var_[var2bdd_[not_v]] = not_v;
        }
        SPOT_ASSERT(var2bdd_.count(not_v)
                    && bdd2var_.count(var2bdd_[not_v]));
        return not_v;
      }
      
      unsigned aig_and(unsigned v1, unsigned v2)
      {
        assert(var2bdd_.count(v1));
        assert(var2bdd_.count(v2));
        bdd b = var2bdd_[v1] & var2bdd_[v2];
        auto it = bdd2var_.find(b);
        if (it != bdd2var_.end())
          return it->second;
        max_var_ += 2;
//        and_gates_[max_var_] = {v1, v2};
        and_gates_.emplace_back(v1, v2);
        register_new(max_var_, b);
        return max_var_;
      }
      
      unsigned aig_and(std::vector<unsigned> vs)
      {
        if (vs.empty())
          return aig_true();
        if (vs.size() == 1)
          return vs[0];
        if (vs.size() == 2)
          return aig_and(vs[0], vs[1]);
        unsigned m = vs.size() / 2;
        unsigned left =
            aig_and(std::vector<unsigned>(vs.begin(), vs.begin() + m));
        unsigned right =
            aig_and(std::vector<unsigned>(vs.begin() + m, vs.end()));
        return aig_and(left, right);
      }
      
      unsigned aig_and_ex(std::vector<unsigned> vs)
      {
        if (vs.empty())
          return aig_true();
        if (vs.size() == 1)
          return vs[0];
        if (vs.size() == 2)
          return aig_and(vs[0], vs[1]);
        
        do
        {
          if (vs.size()&1)
            // Odd size -> make even
            vs.push_back(aig_true());
          // Sort to increase reusage
          std::sort(vs.begin(), vs.end());
          // Reduce two by two inplace
          for (unsigned i=0; i<vs.size()/2; i++)
            vs[i] = aig_and(vs[2*i], vs[2*i+1]);
          vs.resize(vs.size()/2);
        }while(vs.size()>1);
        return vs[0];
      }
      
      unsigned aig_or(unsigned v1, unsigned v2)
      {
        unsigned n1 = aig_not(v1);
        unsigned n2 = aig_not(v2);
        return aig_not(aig_and(n1, n2));
      }
      
      unsigned aig_or(std::vector<unsigned> vs)
      {
        for (unsigned i = 0; i < vs.size(); ++i)
          vs[i] = aig_not(vs[i]);
        return aig_not(aig_and(vs));
      }
      
      unsigned aig_or_ex(std::vector<unsigned> vs)
      {
        for (unsigned i = 0; i < vs.size(); ++i)
          vs[i] = aig_not(vs[i]);
        return aig_not(aig_and_ex(vs));
      }
      
      unsigned aig_pos(unsigned v)
      {
        return v & ~1;
      }
      
      void remove_unused()
      {
        // Run a DFS on the gates to collect
        // all nodes connected to the output.
        std::vector<unsigned> todo;
        std::vector<bool> used(and_gates_.size(), false);
        
        //v_gate_i = (1+n_i+n_l+i)*2 if gate is in positive form
        // -> i = b_gate_i/2 - 1 - n_i - n_l
        auto v2lidx = [&](unsigned v){
          const unsigned N = 1+num_inputs_+latches_.size();
          
          unsigned i = v/2;
          if (i<N)
            return -1u;
          return i-N;
        };
        
        auto mark = [&](unsigned v)
        {
          unsigned idx = v2lidx(aig_pos(v));
          if ((idx != -1u) && !used[idx])
          {
            used[idx] = true;
            todo.push_back(aig_pos(v));
          }
        };
        for (unsigned v : outputs_)
          mark(v);
        for (unsigned v : latches_)
          mark(v);
        while (!todo.empty())
        {
          unsigned v = todo.back();
          todo.pop_back();
          
          unsigned idx = v2lidx(v);
          if (idx != -1u)
          {
            mark(and_gates_[idx].first);
            mark(and_gates_[idx].second);
          }
        }
        // Erase and_gates that were not seen in the above
        // exploration.
        for (unsigned idx=0; idx<used.size(); ++idx)
          if (!used[idx])
          {
            and_gates_[idx].first=0;
            and_gates_[idx].second=0;
          }
      }
      
      unsigned isop2var(const bdd& isop,
                        const std::unordered_map<unsigned, unsigned>&
                            bddvar_to_num)
      {
        static std::vector<unsigned> prod(num_inputs_);
        prod.clear();
        // Check if existing
        auto it = bdd2var_.find(isop);
        if (it != bdd2var_.end())
          return it->second;
        
        // Create
        bdd remainder = isop;
        while (remainder!=bddfalse&&remainder!=bddtrue) {
          unsigned v =
              input_var(bddvar_to_num.at(bdd_var(remainder)));
          if (bdd_high(remainder)==bddfalse) {
            v = aig_not(v);
            remainder = bdd_low(remainder);
          } else
            remainder = bdd_high(remainder);
          prod.push_back(v);
        }
        unsigned isop_var = aig_and_ex(prod);
        // check
        SPOT_ASSERT(var2bdd_.find(isop_var)!=var2bdd_.end());
        SPOT_ASSERT(bdd2var_.find(isop)!=bdd2var_.end());
        // Done
        return isop_var;
      }
      
      
      void
      print(std::ostream& os) const
      {
        // Count active gates
        unsigned n_gates=0;
        for (auto& g : and_gates_)
          if ((g.first != 0) && (g.second != 0))
            ++n_gates;
        os << "aag " << max_var_ / 2
           << ' ' << num_inputs_
           << ' ' << latches_.size()
           << ' ' << outputs_.size()
           << ' ' << n_gates << '\n';
        for (unsigned i = 0; i < num_inputs_; ++i)
          os << (1 + i) * 2 << '\n';
        for (unsigned i = 0; i < latches_.size(); ++i)
          os << (1 + num_inputs_ + i) * 2 << ' ' << latches_[i] << '\n';
        for (unsigned i = 0; i < outputs_.size(); ++i)
          os << outputs_[i] << '\n';
        for (unsigned i=0; i<and_gates_.size(); ++i)
          if ((and_gates_[i].first != 0) && (and_gates_[i].second != 0))
            os << gate_var(i)
               << ' ' << and_gates_[i].first
               << ' ' << and_gates_[i].second << '\n';
        for (unsigned i = 0; i < num_inputs_; ++i)
          os << 'i' << i << ' ' << input_names_[i] << '\n';
        for (unsigned i = 0; i < outputs_.size(); ++i)
          os << 'o' << i << ' ' << output_names_[i] << '\n';
      }
      
    };
    
//    void reuse_outc(std::vector<bdd>& used_outc,
//                        const const_twa_graph_ptr& aut,
//                        const bdd& all_inputs,
//                        const bdd& all_outputs)
//    {
//      used_outc.resize(aut->num_edges()+1, bddfalse);
//      std::vector<bdd> outconds;
//      outconds.reserve(aut->num_edges()+1);
//      // Put dummy values for zero edge
//      used_outc[0] = bddtrue;
//      outconds.push_back(bddtrue);
//      for (const auto &e : aut->edges())
//        outconds.push_back(bdd_exist(e.cond, all_inputs));
//      // There are unassigned edges
//      auto continue_search = [&]()
//      {
//        return std::any_of(
//            used_outc.begin(), used_outc.end(),
//            [](const bdd &b)
//            { return b==bddfalse; }
//        );
//      };
//
//      do {
//        bdd comblast = bddtrue, combnew = bddtrue;
//        for (size_t i = 1; i<used_outc.size(); i++) {
//          if (used_outc[i]==bddfalse) {
//            combnew &= outconds[i];
//            if (combnew==bddfalse)
//              break;
//            comblast = combnew;
//          }
//        }
//        // Get a minterm in complast and "erase" all
//        // out-conds that can be satisfied by it
//        bdd letter_out = bdd_satoneset(comblast, all_outputs, bddfalse);
//        for (size_t i = 1; i<used_outc.size(); i++)
//          if ((used_outc[i]==bddfalse)
//              &&(bdd_implies(letter_out, outconds[i])))
//            // This letter can be used in this transition
//            used_outc[i] = letter_out;
//      } while (continue_search());
//    }
    
    inline unsigned count_high(bdd b)
    {
      unsigned high=0;
      while(b != bddtrue)
      {
        if (bdd_low(b) == bddfalse)
        {
          ++high;
          b = bdd_high(b);
        }else{
          b = bdd_low(b);
        }
      }
      return high;
    }
  
    void maxlow_outc(std::vector<bdd>& used_outc,
                         const const_twa_graph_ptr& aut,
                         const bdd& all_inputs)
    {
      used_outc.resize(aut->num_edges()+1, bddfalse);
      
      for (const auto &e : aut->edges())
      {
        unsigned idx = aut->edge_number(e);
        SPOT_ASSERT(e.cond!=bddfalse);
        bdd bout = bdd_exist(e.cond, all_inputs);
        unsigned n_high=-1u;
        while (bout != bddfalse)
        {
          bdd nextsat = bdd_satone(bout);
          bout -= nextsat;
          unsigned next_high = count_high(nextsat);
          if (next_high<n_high)
          {
            n_high = next_high;
            used_outc[idx] = nextsat;
          }
        }
        SPOT_ASSERT(used_outc[idx]!=bddfalse);
      }
      //Done
    }
    
    // Transforms an automaton into an AIGER circuit
    static aig2
    aut_to_aiger2(const const_twa_graph_ptr& aut, const bdd& all_outputs)
    {
      // The aiger circuit cannot encode the acceptance condition
      // Test that the acceptance condition is true
      if (!aut->acc().is_t())
        throw std::runtime_error("Cannot turn automaton into aiger circuit");
      
      // get the propositions
      std::vector<std::string> input_names;
      std::vector<std::string> output_names;
      bdd all_inputs = bddtrue;
      std::vector<bdd> all_inputs_vec;
      std::unordered_map<unsigned, unsigned> bddvar_to_num;
      for (const auto& ap : aut->ap())
      {
        int bddvar = aut->get_dict()->has_registered_proposition(ap, aut);
        assert(bddvar >= 0);
        bdd b = bdd_ithvar(bddvar);
        if (bdd_implies(all_outputs, b)) // ap is an output AP
        {
          bddvar_to_num[bddvar] = output_names.size();
          output_names.emplace_back(ap.ap_name());
        }
        else // ap is an input AP
        {
          bddvar_to_num[bddvar] = input_names.size();
          input_names.emplace_back(ap.ap_name());
          all_inputs &= b;
          all_inputs_vec.push_back(b);
        }
      }
      
      // Decide on which outcond to use
      // The edges of the automaton all have the form in&out
      // due to the unsplit
      // however we need the edge to be deterministic in out too
      std::vector<bdd> used_outc; // to use
      // Use heuristic to use "few" different conditions
//      reuse_outc(used_outc, aut, all_inputs, all_outputs);
      // Use heuristic out with the least highs
      maxlow_outc(used_outc, aut, all_inputs);
      
      // Encode state in log2(num_states) latches.
      unsigned log2n = std::ceil(std::log2(aut->num_states()));
      unsigned st0 = aut->get_dict()->register_anonymous_variables(log2n, aut);
      bdd all_states = bddtrue;
      for (unsigned i = 0; i < log2n; ++i)
        all_states &= bdd_ithvar(st0 + i);
      
      unsigned num_outputs = output_names.size();
      unsigned init = aut->get_init_state_number();
      SPOT_ASSERT((unsigned)bdd_nodecount(all_states) == log2n);
      SPOT_ASSERT(num_outputs == (unsigned)bdd_nodecount(all_outputs));
      aig2 circuit(input_names, output_names, log2n);
      
      // Register
      // latches
      for (unsigned i = 0; i < log2n; ++i)
        circuit.register_new(circuit.latch_var(i), bdd_ithvar(st0 + i));
      // inputs
      for (unsigned i = 0; i < all_inputs_vec.size(); ++i)
        circuit.register_new(circuit.input_var(i), all_inputs_vec[i]);
      // Latches and outputs are expressed as a DNF in which each term
      // represents a transition.
      // latch[i] (resp. out[i]) represents the i-th latch (resp. output) DNF.
      std::vector<std::vector<unsigned>> latch(log2n);
      std::vector<std::vector<unsigned>> out(num_outputs);
      
      std::unordered_map<bdd, unsigned, bdd_hash> incond_map;
      std::vector<unsigned> prod_state(log2n);
      for (unsigned s = 0; s < aut->num_states(); ++s)
      {
        unsigned src = encode_init_0(s, init);
        prod_state.clear();
        {
          unsigned src2 = src;
          for (unsigned i = 0; i<log2n; ++i) {
            unsigned v = circuit.latch_var(i);
            prod_state.push_back(src2&1 ? v : circuit.aig_not(v));
            src2>>=1;
          }
          SPOT_ASSERT(src2<=1);
        }
        unsigned state_var = circuit.aig_and_ex(prod_state);
        // Done state var
        
        for (auto &e: aut->out(s))
        {
          unsigned e_idx = aut->edge_number(e);
          // Same outcond for all ins
          const bdd& letter_out = used_outc[e_idx];
          auto out_vec = output_to_vec(letter_out, bddvar_to_num);
          
          unsigned dst = encode_init_0(e.dst, init);
          auto next_state_vec = state_to_vec(dst, log2n);
          
          // Get the isops over the input condition
          // Each isop only contains variables from in
          // -> directly compute the corresponding
          // variable <-> and gate
          bdd incond = bdd_exist(e.cond, all_outputs);
          auto incond_var_it = incond_map.find(incond);
          if (incond_var_it == incond_map.end())
          {
            // The incond and its isops have not yet been calculated
            minato_isop cond(incond);
            bdd isop;
            std::vector<unsigned> all_isop_var;
            while ((isop = cond.next())!=bddfalse){
              all_isop_var.push_back(circuit.isop2var(isop,
                                                      bddvar_to_num));
            }
            SPOT_ASSERT(!all_isop_var.empty());
            // Compute their OR
            unsigned incond_var_ = circuit.aig_or_ex(all_isop_var);
            incond_map[incond] = incond_var_;
            incond_var_it = incond_map.find(incond);
          }
          // AND with state
          unsigned t = circuit.aig_and(state_var, incond_var_it->second);
          for (unsigned i = 0; i<next_state_vec.size(); ++i)
            if (next_state_vec[i])
              latch[i].push_back(t);
          for (unsigned i = 0; i<num_outputs; ++i)
            if (out_vec[i])
              out[i].push_back(t);
        } // edge
      } // state
      
      for (unsigned i = 0; i < log2n; ++i)
        circuit.set_latch(i, circuit.aig_or_ex(latch[i]));
      for (unsigned i = 0; i < num_outputs; ++i)
        circuit.set_output(i, circuit.aig_or_ex(out[i]));
      circuit.remove_unused();
      return circuit;
    } // aut_to_aiger2
  }
  
  std::ostream&
  print_aiger_old(std::ostream& os, const const_twa_ptr& aut)
  {
    auto a = down_cast<const_twa_graph_ptr>(aut);
    if (!a)
      throw std::runtime_error("aiger output is only for twa_graph");
    
    bdd* all_outputs = aut->get_named_prop<bdd>("synthesis-outputs");
    
    aig circuit = aut_to_aiger(a, all_outputs ? *all_outputs : bdd(bddfalse));
    circuit.print(os);
    return os;
  }
  
  std::ostream&
  print_aiger(std::ostream& os, const const_twa_ptr& aut)
  {
    auto a = down_cast<const_twa_graph_ptr>(aut);
    if (!a)
      throw std::runtime_error("aiger output is only for twa_graph");
    
    bdd* all_outputs = aut->get_named_prop<bdd>("synthesis-outputs");
    
    aig2 circuit = aut_to_aiger2(a, all_outputs ? *all_outputs : bdd(bddfalse));
    circuit.print(os);
    return os;
  }
}
