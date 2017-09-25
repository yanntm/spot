// -*- coding: utf-8 -*-
// Copyright (C) 2014, 2015, 2016 Laboratoire de Recherche et
// Développement de l'Epita (LRDE).
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

#include <config.h>
#include <spot/misc/timer.hh>
#include <spot/tl/hierarchy.hh>
#include <spot/tl/formula.hh>
#include <spot/tl/parse.hh>
#include <spot/twa/fwd.hh>
#include <spot/twa/acc.hh>
#include <spot/twaalgos/ltl2tgba_fm.hh>
#include <spot/twaalgos/cobuchi.hh>
#include <spot/twaalgos/dualize.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/postproc.hh>
#include <spot/twaalgos/remfin.hh>
#include <spot/twaalgos/totgba.hh>
#include <spot/misc/memusage.hh>

#include <spot/twaalgos/hoa.hh>
#include <spot/misc/escape.hh>

#include <stack>
#include <spot/misc/hash.hh>
#include <spot/misc/bddlt.hh>
#include <spot/twaalgos/sccinfo.hh>

# define val(X) X / clocks_per_sec

namespace
  {
    struct mp_hash
    {
      size_t operator()(std::pair<unsigned, const spot::bitvect_array*> bv)
        const
      {
        size_t res = 0;
        size_t size = bv.second->size();
        for (unsigned i = 0; i < size; ++i)
          res = spot::wang32_hash(res ^
                                    spot::wang32_hash(bv.second->at(i).hash()));
        res = spot::wang32_hash(res ^ bv.first);
        return res;
      }
    };

    struct mp_equal
    {
      bool operator()(std::pair<unsigned, const spot::bitvect_array*> bvl,
                      std::pair<unsigned, const spot::bitvect_array*> bvr) const
      {
        if (bvl.first != bvr.first)
          return false;
        size_t size = bvl.second->size();
        for (unsigned i = 0; i < size; ++i)
          if (bvl.second->at(i) != bvr.second->at(i))
            return false;
        return true;
      }
    };

    typedef std::unordered_map
      <std::pair<unsigned, const spot::bitvect_array*>,
       unsigned, mp_hash, mp_equal>
      dca_st_mapping;
    class dca_breakpoint_cons final
    {
      protected:
        spot::const_twa_graph_ptr aut_;
        spot::vect_nca_info* nca_info_;
        unsigned nb_copy_;
        bdd ap_;
        std::vector<bdd> num2bdd_;
        std::map<bdd, unsigned, spot::bdd_less_than> bdd2num_;

        // Each state is characterized by a spot::bitvect_array of 2 bitvects:
        // bv1 -> the set of original states that it represents
        // bv2 -> a set of marked states (~)
        // To do so, we keep a correspondance between a state number and its
        // bitvect representation.
        dca_st_mapping bv_to_num_;
        std::vector<std::pair<unsigned, spot::bitvect_array*>> num_2_bv_;

        // Next states to process.
        std::stack<std::pair<std::pair<unsigned, spot::bitvect_array*>,
          unsigned>>
          todo_;
        // All allocated bitvect that must be freed at the end.
        std::vector<const spot::bitvect_array*> toclean_;

      public:
        dca_breakpoint_cons(const spot::const_twa_graph_ptr aut,
                            spot::vect_nca_info* nca_info,
                            unsigned nb_copy)
          : aut_(aut),
            nca_info_(nca_info),
            nb_copy_(nb_copy),
            ap_(aut->ap_vars())
        {
          //  Get all bdds.
          bdd all = bddtrue;
          for (unsigned i = 0; all != bddfalse; ++i)
            {
              bdd one = bdd_satoneset(all, ap_, bddfalse);
              num2bdd_.push_back(one);
              bdd2num_[one] = i;
              all -= one;
            }
        }

        ~dca_breakpoint_cons()
        {
          for (auto p : *nca_info_)
            delete p;
        }

        spot::twa_graph_ptr run(bool named_states)
        {
          unsigned ns = aut_->num_states();
          unsigned nc = num2bdd_.size();

          // Fill bv_aut_trans_ which is a bitvect of all possible transitions
          // of each state for each letter.
          auto bv_aut_trans_ = std::unique_ptr<spot::bitvect_array>(
              spot::make_bitvect_array(ns, ns * nc));
          for (unsigned src = 0; src < ns; ++src)
            {
              size_t base = src * nc;
              for (auto& t: aut_->out(src))
                {
                  bdd all = t.cond;
                  while (all != bddfalse)
                    {
                      bdd one = bdd_satoneset(all, ap_, bddfalse);
                      all -= one;
                      bv_aut_trans_->at(base + bdd2num_[one]).set(t.dst);
                    }
                }
            }

          spot::twa_graph_ptr res = make_twa_graph(aut_->get_dict());
          res->copy_ap_of(aut_);
          res->set_co_buchi();

          // Rename states of resulting automata (for display purposes).
          std::vector<std::string>* state_name = nullptr;
          if (named_states)
            {
              state_name = new std::vector<std::string>();
              res->set_named_prop("state-names", state_name);
            }

          // Function used to add a new state. A new state number is associated
          // to the state if has never been added before, otherwise the old
          // state number is returned.
          auto new_state = [&](std::pair<unsigned, spot::bitvect_array*> bv_st)
            -> unsigned
          {
            auto p = bv_to_num_.emplace(bv_st, 0);
            if (!p.second)
              return p.first->second;

            p.first->second = res->new_state();
            todo_.emplace(bv_st, p.first->second);
            assert(num_2_bv_.size() == p.first->second);
            num_2_bv_.push_back(bv_st);

            // For display purposes only.
            if (named_states)
              {
                assert(p.first->second == state_name->size());
                std::ostringstream os;
                bool not_first = false;
                os << '{';
                for (unsigned s = 0; s < ns; ++s)
                  {
                    if (bv_st.second->at(1).get(s))
                      {
                        if (not_first)
                          os << ',';
                        else
                          not_first = true;
                        os << '~';
                      }
                    if (bv_st.second->at(0).get(s))
                      os << s;
                  }
                os << '|' << bv_st.first << '}';
                state_name->emplace_back(os.str());
              }
            return p.first->second;
          };

          // Set init state
          auto bv_init = spot::make_bitvect_array(ns, 2);
          toclean_.push_back(bv_init);
          bv_init->at(0).set(aut_->get_init_state_number());
          res->set_init_state(new_state(std::make_pair(0, bv_init)));

          // Processing loop
          while (!todo_.empty())
            {
              auto top = todo_.top();
              todo_.pop();

              // Bitvect array of all possible moves for each letter.
              auto bv_trans = spot::make_bitvect_array(ns, nc);
              for (unsigned s = 0; s < ns; ++s)
                if (top.first.second->at(0).get(s))
                  for (unsigned l = 0; l < nc; ++l)
                    bv_trans->at(l) |= bv_aut_trans_->at(s * nc + l);
              toclean_.push_back(bv_trans);

              // Bitvect array of all possible moves for each state marked
              // for each letter. If no state is marked (breakpoint const.),
              // nothing is set.
              bool marked = !top.first.second->at(1).is_fully_clear();
              auto bv_trans_mark = spot::make_bitvect_array(ns, nc);
              if (marked)
                for (unsigned s = 0; s < ns; ++s)
                  if (top.first.second->at(1).get(s))
                    for (unsigned l = 0; l < nc; ++l)
                      bv_trans_mark->at(l) |= bv_aut_trans_->at(s * nc + l);
              toclean_.push_back(bv_trans_mark);

              for (unsigned l = 0; l < nc; ++l)
                {
                  auto bv_res = spot::make_bitvect_array(ns, 2);
                  toclean_.push_back(bv_res);
                  bv_res->at(0) |= bv_trans->at(l);
                  // If this state has not any outgoing edges.
                  if (bv_res->at(0).is_fully_clear())
                    continue;

                  // Set states that must be marked.
                  for (const auto& p : *nca_info_)
                    {
                      if (p->clause_num != top.first.first)
                        continue;

                      if (*p->all_dst == bv_res->at(0))
                        if ((marked && bv_trans_mark->at(l).get(p->state_num))
                             || (!marked && bv_res->at(0).get(p->state_num)))
                          bv_res->at(1).set(p->state_num);
                    }

                  unsigned i = bv_res->at(1).is_fully_clear() ?
                                (top.first.first + 1) % (nb_copy_ + 1)
                              : top.first.first;

                  res->new_edge(top.second,
                                new_state(std::make_pair(i, bv_res)),
                                num2bdd_[l]);
                }
            }

          // Set accepting states.
          spot::scc_info si(res);
          bool state_based = (bool)aut_->prop_state_acc();
          unsigned res_size = res->num_states();
          for (unsigned s = 0; s < res_size; ++s)
            if (num_2_bv_[s].second->at(1).is_fully_clear())
              for (auto& edge : res->out(s))
                if (si.scc_of(edge.dst) == si.scc_of(s) || state_based)
                  edge.acc = spot::acc_cond::mark_t({0});

          // Delete all bitvect arrays allocated by this method (run).
          for (auto v: toclean_)
            delete v;

          res->merge_edges();
          return res;
        }
    };
  }


static
spot::twa_graph_ptr
new_method(const spot::const_twa_graph_ptr& aut)
{
  spot::process_timer p1;
  spot::twa_graph_ptr res = nullptr;
  std::vector<spot::acc_cond::rs_pair> pairs;
  spot::vect_nca_info nca_info;
  if (aut->acc().is_streett_like(pairs) || aut->acc().is_parity())
    {
      // NCA
      p1.start();
      spot::nsa_to_nca(aut, false, &nca_info);
      p1.stop();
      std::cout << p1.walltime() << ',';

      // DCA
      p1.start();
      dca_breakpoint_cons dca(aut, &nca_info, 0);
      res = dca.run(false);
      p1.stop();
      std::cout << p1.walltime() << ',';
    }
  else if (aut->get_acceptance().is_dnf())
    {
      // NCA
      p1.start();
      spot::dnf_to_nca(aut, false, &nca_info);
      p1.stop();
      std::cout << p1.walltime() << ',';

      // DCA
      p1.start();
      unsigned nb_copy = 0;
      for (const auto& p : nca_info)
        if (nb_copy < p->clause_num)
          nb_copy = p->clause_num;
      dca_breakpoint_cons dca(aut, &nca_info, nb_copy);
      res = dca.run(false);
      p1.stop();
      std::cout << p1.walltime() << ',';
    }
  else
    throw std::runtime_error("res_realizable() only works with "
                             "Streett-like, Parity or any "
                             "acceptance condition in DNF");

  p1.start();
  res = spot::dualize(res);
  p1.stop();
  std::cout << p1.walltime() << ',';

  return res;
}

static
spot::twa_graph_ptr
old_method(const spot::twa_graph_ptr& aut)
{
  // if aut is a non-deterministic TGBA, we do
  // TGBA->DPA->DRA->(D?)BA.  The conversion from DRA to
  // BA will preserve determinism if possible.
  spot::process_timer p1;
  p1.start();
  spot::postprocessor p;
  p.set_type(spot::postprocessor::Generic);
  p.set_pref(spot::postprocessor::Deterministic);
  p.set_level(spot::postprocessor::Low);
  auto dpa = p.run(aut);
  p1.stop();
  std::cout << p1.walltime() << ',';

  p1.start();
  spot::twa_graph_ptr dra = to_generalized_rabin(dpa);
  p1.stop();
  std::cout << p1.walltime() << ',';

  p1.start();
  auto ba = rabin_to_buchi_maybe(dra);
  p1.stop();
  std::cout << p1.walltime() << ',';

  return ba;
}

static
spot::twa_graph_ptr
convert_to_dba(int algo, const spot::formula& f, spot::bdd_dict_ptr& d)
{
  spot::process_timer p;

  spot::twa_graph_ptr start = nullptr;
  p.start();
  if (algo == 1)
    start = ltl_to_tgba_fm(spot::formula::Not(f), d, true);
  else if (algo == 2)
    start = ltl_to_tgba_fm(f, d, true);
  p.stop();
  std::cout << p.walltime() << ',';

  spot::twa_graph_ptr res = nullptr;
  if (algo == 1)
    res = new_method(start);
  else if (algo == 2)
    res = old_method(start);

  return res;
}

int
main(int argc, char** argv)
{
#ifdef _SC_CLK_TCK
  const long clocks_per_sec = sysconf(_SC_CLK_TCK);
#else
#  ifdef CLOCKS_PER_SEC
  const long clocks_per_sec = CLOCKS_PER_SEC;
#  else
  const long clocks_per_sec = 100;
#  endif
#endif

  // Handle arguments.
  if (argc != 3)
    {
      std::cerr << "usage: ./dba_builder <ltl> <algo>\n";
      exit(2);
    }
  std::string ltl(argv[1]);
  std::istringstream ss(argv[2]);
  int algo = 0;
  if (!(ss >> algo))
    {
      std::cerr << "Invalid number " << argv[1] << '\n';
      exit(2);
    }

  spot::bdd_dict_ptr d = spot::make_bdd_dict();
  spot::formula f = spot::parse_formula(ltl);
  spot::process_timer p;

  p.start();
  spot::twa_graph_ptr res = convert_to_dba(algo, f, d);
  p.stop();

  clock_t cpusys = p.cputime(false, true, true, true);
  clock_t cpuusr = p.cputime(true, false, true, true);
  double wall = p.walltime();
  int pages = spot::memusage();
  bool det = spot::is_universal(res);
  int nb_states = res->num_states();

  if (!res->acc().is_buchi())
    {
      std::cerr << "Resulting aut. does not have Büchi acceptance condition\n";
      exit(2);
    }

  std::ostringstream oss;
  spot::print_hoa(oss, res, "l");

  std::cout << val(cpusys) << ','
            << val(cpuusr) << ','
            << wall << ','
            << pages << ','
            << det << ','
            << nb_states << ',';
  spot::escape_rfc4180(std::cout, oss.str());

  return 0;
}
