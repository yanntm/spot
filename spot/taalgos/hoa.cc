// -*- coding: utf-8 -*-
// Copyright (C) 2010, 2012, 2014-2016, 2018 Laboratoire de Recherche
// et Developpement de l Epita (LRDE).
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
#include <ostream>
#include <spot/ta/tgta.hh>
#include <spot/taalgos/hoa.hh>
#include <spot/twa/bddprint.hh>
#include <spot/taalgos/reachiter.hh>
#include <spot/misc/escape.hh>
#include <spot/misc/bareword.hh>
#include <spot/misc/minato.hh>
#include <cstdlib>
#include <cstring>

using namespace std::string_literals;

namespace spot
{
  namespace
  {
    class dotty_bfsh final: public ta_reachable_iterator_breadth_first
    {
      typedef std::map<int, unsigned> ap_map;
      ap_map ap;

      typedef std::vector<int> vap_t;
      vap_t vap;

      typedef std::map<bdd, std::string, bdd_less_than> sup_map;
      sup_map sup;

      void number_fill_sup()
      {
        typedef std::unordered_map<const state*, unsigned,
                state_ptr_hash, state_ptr_equal> state_map;
        state_map sm;

        auto aut = t_automata_;
        std::string empty;

        // Scan the whole automaton to number states.
        std::deque<const state*> todo;

        const state* init = aut->get_artificial_initial_state();
        sm[init] = 0;
        todo.push_back(init);

        while (!todo.empty())
        {
          auto* i = aut->succ_iter(todo.front());
          todo.pop_front();
          for (i->first(); !i->done(); i->next())
          {
            const state* dst = i->dst();
            bdd cond = i->cond();

            sup.insert(std::make_pair(cond, empty));
            if (sm.insert(std::make_pair(dst, sm.size())).second)
              todo.push_back(dst);
            else
              dst->destroy();
          }
          delete i;
        }
      }

      std::ostream&
        emit_acc(std::ostream& os, acc_cond::mark_t b)
        {
          // FIXME: We could use a cache for this.
          if (!b)
            return os;
          os << " {";
          bool notfirst = false;
          for (auto v: b.sets())
          {
            if (notfirst)
              os << ' ';
            else
              notfirst = true;
            os << v;
          }
          os << '}';
          return os;
        }

      void number_all_ap()
      {
        sup_map::iterator i;
        bdd all = bddtrue;
        for (i = sup.begin(); i != sup.end(); ++i)
          all &= bdd_support(i->first);

        while (all != bddtrue)
        {
          int v = bdd_var(all);
          all = bdd_high(all);
          ap.insert(std::make_pair(v, vap.size()));
          vap.push_back(v);
        }

        for (i = sup.begin(); i != sup.end(); ++i)
        {
          bdd cond = i->first;
          if (cond == bddtrue)
          {
            i->second = "t";
            continue;
          }
          if (cond == bddfalse)
          {
            i->second = "f";
            continue;
          }
          std::ostringstream s;
          bool notfirstor = false;

          minato_isop isop(cond);
          bdd cube;
          while ((cube = isop.next()) != bddfalse)
          {
            if (notfirstor)
              s << " | ";
            bool notfirstand = false;
            while (cube != bddtrue)
            {
              if (notfirstand)
                s << '&';
              else
                notfirstand = true;
              bdd h = bdd_high(cube);
              if (h == bddfalse)
              {
                s << '!' << ap[bdd_var(cube)];
                cube = bdd_low(cube);
              }
              else
              {
                s << ap[bdd_var(cube)];
                cube = h;
              }
            }
            notfirstor = true;
          }
          i->second = s.str();
        }
      }

      void parse_opts(const char* options)
      {
        while (char c = *options++)
          switch (c)
          {
            case '.':
            case 'A':
              is_ta_ = true;
              break;
            case 'c':
              is_ta_ = false;
              break;
            case 'C':
            case 'h':
            case 'f':
            case 'v':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case 'a':
            case 'b':
            case 'B':
            case 'e':
            case 'n':
            case 'N':
            case 'o':
            case 'r':
            case 'R':
            case 's':
            case 't':
            case '+':
            case '<':
            case '#':
              // All these options are implemented by dotty() on TGBA,
              // but are not implemented here.  We simply ignore them,
              // because raising an exception if they are in
              // SPOT_DEFAULT would be annoying.
              break;
            default:
              throw std::runtime_error
                ("unknown option for print_hoa(): "s + c);
          }
      }

      public:
      dotty_bfsh(std::ostream& os, const const_ta_ptr& a,
          const char* opt) :
        ta_reachable_iterator_breadth_first(a), os_(os)
      {
        parse_opts(opt ? opt : ".");
      }

      void start() override
      {
        number_fill_sup();
        number_all_ap();
        os_ << "HOA: v1\n";
        os_ << "name: \"a\"\n";
        os_ << "Start: 1\n";
        os_ << "AP: " << vap.size();
        auto d = t_automata_->get_dict();
        for (vap_t::const_iterator i = vap.begin();
            i != vap.end(); ++i)
          escape_str(os_ << " \"", d->bdd_map[*i].f.ap_name()) << '"';
        os_ << '\n';

        if (is_ta_)
          os_ << "Acceptance: 2 Fin(0) | Inf(1)\n";
        else
        {
          unsigned num_acc = t_automata_->acc().num_sets();
          os_ << "Acceptance: " << num_acc << ' ';
          t_automata_->acc().get_acceptance().to_text(os_);
          os_ << '\n';
        }
        os_ << "Spot.Testing:\n";
        os_ << "--BODY--\n";

        // Always copy the environment variable into a static string,
        // so that we (1) look it up once, but (2) won't crash if the
        // environment is changed.
        static std::string extra = []()
        {
          auto s = getenv("SPOT_DOTEXTRA");
          return s ? s : "";
        }();

        artificial_initial_state_ =
          t_automata_->get_artificial_initial_state();
        assert(artificial_initial_state_);
      }

      void end() override
      {
        os_ << "--END--\n";
      }

      void process_state(const state* s, int n) override
      {
        os_ << "State: " << n-1 << '\n';
        is_acc_ = t_automata_->is_accepting_state(s);
        bool is_livelock =
          t_automata_->is_livelock_accepting_state(s);
        std::string acc = "\n";
        if (is_livelock && is_acc_)
          acc = " {0 1}\n";
        else if (is_livelock)
          acc = " {1}\n";
        else if (is_acc_)
          acc = " {0}\n";
        os_ << "[t] " << n-1 << acc;
      }

      void process_link(int, int out, const ta_succ_iterator* si) override
      {
        std::string label = sup[si->cond()];
        os_ << '[' << label << "] " << out-1;
        if (is_ta_ && is_acc_)
          os_ << " {0}";
        else
          emit_acc(os_, si->acc());
        os_ << '\n';
      }

      private:
      std::ostream& os_;
      const spot::state* artificial_initial_state_;

      bool is_ta_ = false;
      bool is_acc_ = false;
    };

  }

  std::ostream&
    print_hoa(std::ostream& os, const const_ta_ptr& a, const char* opt)
    {
      dotty_bfsh d(os, a, opt);
      d.run();
      return os;
    }
}
