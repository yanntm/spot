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
#include <spot/twaalgos/blif.hh>

#include <cmath>
#include <unordered_map>
#include <vector>
#include <sstream>

#include <spot/twa/twagraph.hh>
#include <spot/misc/bddlt.hh>

namespace spot
{
  namespace{
    std::stringstream os_buffer;
    std::vector<int> support_vars_buffer;

    void print_one(signed char* varset, int size)
    {
      for (int v : support_vars_buffer)
      {
        assert(v<size);
        os_buffer.put(varset[v] < 0 ? '-' : (char)('0' + varset[v]));
      }
      os_buffer.put(' ');
      os_buffer.put('1');
      os_buffer.put('\n');
    }

    void print_table(std::ostream& os, bdd b, std::vector<int>& support_vars)
    {
      support_vars_buffer = support_vars;
      bdd_allsat(b, print_one);
      os << os_buffer.rdbuf();
    }


    std::vector<int> minimal_sat(bdd b)
    {
      std::vector<int> best_highs, this_highs;
      bdd s1;
      while (b != bddfalse && b != bddtrue)
      {
        s1 = bdd_satone(b);
        b -= s1;
        this_highs.clear();
        while (s1 != bddfalse && s1 != bddtrue)
        {
          unsigned topvar = bdd_var(s1);
          if (bdd_low(s1) == bddfalse)
          {
            this_highs.push_back(topvar);
            s1 = bdd_high(s1);
          }else{
            s1 = bdd_low(s1);
          }
        }
        if (best_highs.empty() || (this_highs.size()<best_highs.size()))
          best_highs.swap(this_highs);
      }
      return best_highs;
    }
  }


  std::ostream&
  print_blif(std::ostream& os, const const_twa_ptr& aut)
  {
    if (!aut->get_named_prop<bdd>("synthesis-outputs"))
      throw std::runtime_error("synthesis-outputs necessary for "
                               "conversion to blif");

    auto ag = down_cast<const_twa_graph_ptr>(aut);
    bdd all_outputs = *aut->get_named_prop<bdd>("synthesis-outputs");

    // Get input and output vars
    std::vector<int> inputs, outputs;
    std::vector<std::string> input_names, output_names;
    std::unordered_map<int, unsigned> bddvar2num;
    // Dict from bddvar to "input" that is the inputs and latch outs
    std::unordered_map<int, std::string> bddvar2name;
    for (const auto& ap : aut->ap())
    {
      int bddvar = aut->get_dict()->has_registered_proposition(ap, aut);
      assert(bddvar >= 0);
      bdd b = bdd_ithvar(bddvar);
      if (bdd_implies(all_outputs, b)) // ap is an output AP
      {
        bddvar2num[bddvar] = output_names.size();
        output_names.emplace_back(ap.ap_name());
        outputs.push_back(bddvar);
      }
      else // ap is an input AP
      {
        bddvar2num[bddvar] = input_names.size();
        input_names.emplace_back(ap.ap_name());
        inputs.push_back(bddvar);
        bddvar2name[bddvar] = ap.ap_name();
      }
    }

    // Add the latches
    const unsigned log2n = std::ceil(std::log2(ag->num_states()));
    const unsigned st0 =
        aut->get_dict()->register_anonymous_variables(log2n, aut);
    std::vector<int> latch_vars_(log2n);
    std::iota(latch_vars_.begin(), latch_vars_.end(), st0); // latch i high
    // Add latch outs
    for (unsigned i=0; i<log2n; ++i)
    {
      std::stringstream ss;
      ss.write("latchout", 8);
      ss << i;
      bddvar2name[st0+i] = ss.str();
    }

    // Iterate over the edges
    // The edges are supposed to have the following form (as it results from
    // apply strategy) : (i_0 | i_1 | i_2) & (o_0 | o_1)
    // So no edges of the form (i_0 & o_0 | i_1 & o_0 | ... ) are allowed

    // Get the initial state and make sure to encode it as 0
    unsigned init = ag->get_init_state_number();
    auto encode_0 = [&](unsigned src)
    {
      return src == init ? 0 : src == 0 ? init : src;
    };

    auto state2vec = [&](unsigned s, std::vector<bool>& vec)
    {
      vec.clear();
      s = encode_0(s);
      for (unsigned i=0; i<log2n; ++i)
      {
        vec.push_back(s&1);
        s >>= 1;
      }
      assert(s <= 1);
    };

    auto state2bdd = [&](unsigned s)
    {
      static std::vector<bool> vec;
      state2vec(s, vec);
      bdd statebdd=bddtrue;
      for (unsigned i=0; i<log2n; i++)
        statebdd &= vec.at(i) ?
                    bdd_ithvar(latch_vars_[i]) : bdd_nithvar(latch_vars_[i]);
      return statebdd;
    };

    auto support2varvec = [&](bdd sup, std::vector<int>& varvec)
    {
      varvec.clear();
      do
      {
        varvec.push_back(bdd_var(sup));
        sup = bdd_high(sup);
      }while(sup != bddtrue);
    };

    std::vector<bdd> outs(outputs.size(), bddfalse);
    std::vector<bdd> latches(log2n, bddfalse);
    std::vector<bool> dst_vec(log2n);
    for (unsigned s=0; s<ag->num_states(); ++s)
    {
      bdd srcbdd = state2bdd(s);
//      std::cout << s << "->" << srcbdd << std::endl;
      for (const auto& e : ag->out(s))
      {
        // Encode the dst
        state2vec(e.dst, dst_vec);
        // Split to in and out
        bdd e_in = bdd_exist(e.cond, all_outputs);
        bdd e_out = bdd_existcomp(e.cond, all_outputs);
        assert(e.cond == bdd_and(e_in, e_out));
        // Get the satisfying one with the least highs
        std::vector<int> out_high = minimal_sat(e_out);
        // The condition to take this edges becomes src&e_in
        bdd new_cond = bdd_and(e_in, srcbdd);

        // we have to add this to the outs that are high
        // and the latches of dst
        for (unsigned i=0; i<log2n; ++i)
          if (dst_vec[i])
            latches[i] |= new_cond;
        for (int o: out_high)
          outs[bddvar2num.at(o)] |= new_cond;
      }
    }

    // Create the blif
    std::vector<int> support_vars;
    // Prepend ins and outs as comment
    os << '#' << input_names.size()+output_names.size() << '\n';
    for (unsigned i=0; i<input_names.size(); ++i)
      os << "#i" << i << ' ' << input_names[i] << '\n';
    for (unsigned i=0; i<output_names.size(); ++i)
      os << "#o" << i << ' ' << output_names[i] << '\n';
    os << ".model synt\n.inputs";
    for (const auto& s : input_names)
      os << ' ' << s;
    os << "\n.outputs";
    for (const auto& s : output_names)
      os << ' ' << s;
    os << '\n';
    // Create the latches
    for (unsigned i=0; i<log2n; ++i)
      os << ".latch latchin" << i << " latchout" << i << " ah NIL 0\n";
    // Create the truth table of each latch
    for (unsigned i=0; i<log2n; ++i)
    {
      // Special cases for FALSE and TRUE
      if (latches[i] == bddfalse || latches[i] == bddtrue)
      {
        os << ".names latchin" << i << '\n';
        os << (latches[i] == bddfalse ? "0\n" : "1\n");
        continue;
      }
      support2varvec(bdd_support(latches[i]), support_vars);
      os << ".names";
      for (int var : support_vars)
        os << ' ' << bddvar2name.at(var);
      // output
      os << " latchin" << i << '\n';
      // Thruth table
      print_table(os, latches[i], support_vars);
    }
    for (unsigned i=0; i<outs.size(); ++i)
    {
      if (outs[i] == bddfalse || outs[i] == bddtrue)
      {
        os << ".names " << output_names[i] << '\n';
        os << (outs[i] ==   bddfalse ? "0\n" : "1\n");
        continue;
      }
      support2varvec(bdd_support(outs.at(i)), support_vars);
      os << ".names";
      for (int var : support_vars)
        os << ' ' << bddvar2name.at(var);
      // output
      os << ' ' << output_names[i] << '\n';
      // Thruth table
      print_table(os, outs[i], support_vars);
    }
    os << ".end\n";
    return os;
  }
}