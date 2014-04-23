// -*- coding: utf-8 -*-
// Copyright (C) 2014 Laboratoire de Recherche et Développement de
// l'Epita (LRDE).
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

#ifndef SPOT_GRAPH_BIDIGRAPH_HH
# define SPOT_GRAPH_BIDIGRAPH_HH

# include "misc/common.hh"
# include "tgba/state.hh"
# include "tgbaalgos/reachiter.hh"
# include <unordered_map>
# include <deque>
# include <vector>

namespace spot
{

  class tgba;
  class state;

  namespace graph
  {

    class bidistate;
    class bidigraph;
    typedef std::vector<bidistate*> bidigraph_states;
    class bidistate
    {
      friend class bidigraph;
      friend std::ostream& operator<<(std::ostream& os, const bidigraph& bdg);

      bidistate()
        : out_(new bidigraph_states()), in_(new bidigraph_states()),
          is_alive_(true),
          out_degree_(0),
          in_degree_(0),
          next_delta(nullptr),
          prev_delta(nullptr)
      {
      }

      ~bidistate()
      {
        delete out_;
        delete in_;
      }

      unsigned int
      get_in_degree() const
      {
        return in_degree_;
      }

      unsigned int
      get_out_degree() const
      {
        return out_degree_;
      }

      bidigraph_states*
      get_succ() const
      {
        return out_;
      }

      bidigraph_states*
      get_pred() const
      {
        return in_;
      }

      void
      add_succ(bidistate* s)
      {
        out_->emplace_back(s);
        ++out_degree_;
      }

      void
      add_pred(bidistate* s)
      {
        in_->emplace_back(s);
        ++in_degree_;
      }

      // Remove the transition going to s.
      void
      remove_succ()
      {
        --out_degree_;
      }

      // Remove the transition coming from s.
      void
      remove_pred()
      {
        --in_degree_;
      }

      bool
      is_alive()
      {
        return is_alive_;
      }

      void
      set_is_alive(bool value)
      {
        is_alive_ = value;
      }

      bidigraph_states* out_;
      bidigraph_states* in_;
      bool is_alive_;
      unsigned out_degree_;
      unsigned in_degree_;
      bidistate* next_delta;
      bidistate* prev_delta;
    };

    /// \brief A representation of a graph with no information on transitions,
    /// nor on the accepting and initial states.  Bidigraphs have information on
    /// their out and in degree.
    class SPOT_API bidigraph : public tgba_reachable_iterator
    {
    public:
      /// When creating a bidigraph from a tgba, make sure to call it's
      /// destructor before destroying the \a tgba and it's corresponding
      /// dictionary.
      bidigraph(const tgba* g, bool do_loops);
      ~bidigraph();
      void remove_state(bidistate* s);

      /// From a tgba state's hash, get a list of bidistate*. To get the
      /// correct bidistate*, iterate through a list of bidistate*.
      /// For each of these bidistates, get the corresponding tgba_state and
      /// compare it to \a tgba_state.
      bidistate* get_bidistate(const spot::state* tgba_state) const;

      const spot::state* get_tgba_state(bidistate* bidistate) const;

      virtual const state* next_state();

      const bidigraph_states& get_bidistates() const;
      bidistate* pop_source();
      bidistate* pop_sink();
      bidistate* pop_max_delta();
      // pops first element of delta_class
      bidistate* pop(unsigned delta_class);
      void add_delta(unsigned delta_class, bidistate* bds_id);

      friend SPOT_API std::ostream&
      operator<<(std::ostream& os, const bidigraph& bdg);

    protected:
      std::deque<const state*> todo; ///< A queue of states yet to explore.

    private:
      // Allows to ignore loops a -> a.
      bool do_loops_;
      // Needed to know which delta class to access next.
      unsigned max_index_;
      // Maximum out degree found in graph
      unsigned max_out_;
      // Maximum in degree found in graph
      unsigned max_in_;

      bidigraph_states states_;
      // deltas_[0] are sinks
      // deltas[max_in + max_out] are sources
      std::vector<bidistate*> deltas_; // 1 + max_in + max_out clases
      std::unordered_map<bidistate*, const spot::state*> bidistate_to_tgba_;
      std::unordered_map<const spot::state*, bidistate*,
                         const spot::state_ptr_hash,
                         spot::state_ptr_equal> tgba_to_bidistate_;

      // Put successors in new delta classes.
      void update_succs(bidigraph_states* succ);
      // Put predecessors in new delta classes.
      void update_preds(bidigraph_states* pred);
      void create_bidistate(const spot::state* tgba_state);
      virtual void add_state(const state* s);
      virtual void process_link(const state* in_s, int in,
                                const state* out_s, int out,
                                const tgba_succ_iterator* si);
      void remove_delta(bidistate* s, int step);

    };
  } // graph namespace
}

#endif // SPOT_GRAPH_BIDIGRAPH_HH
