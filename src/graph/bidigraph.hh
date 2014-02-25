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
# include <unordered_set>
# include <vector>

namespace spot
{

  class tgba;
  class state;

  namespace graph
  {

    class bidistate;
    typedef std::vector<bidistate*> bidigraph_states;
    class bidistate
    {
    public:
      bidistate()
        : out_(new bidigraph_states()), in_(new bidigraph_states())
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
        return in_->size();
      }

      unsigned int
      get_out_degree() const
      {
        return out_->size();
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
      }

      void
      add_pred(bidistate* s)
      {
        in_->emplace_back(s);
      }

      // Remove the transition going to s.
      void
      remove_succ(const bidistate* s);

      // Remove the transition coming from s.
      void
      remove_pred(const bidistate* s);

    private:
      bidigraph_states* out_;
      bidigraph_states* in_;
    };

    /// \brief A representation of a graph with no information on transitions,
    /// nor on the accepting and initial states.  Bidigraphs have information on
    /// their out and in degree.
    class SPOT_API bidigraph : public tgba_reachable_iterator_breadth_first
    {
    public:
      /// When creating a bidigraph from a tgba, make sure to call it's
      /// destructor before destroying the \a tgba and it's corresponding
      /// dictionary.
      bidigraph(const tgba* g);
      ~bidigraph();
      void remove_state(bidistate* s);

      /// From a tgba state's hash, get a list of bidistate*. To get the
      /// correct bidistate*, iterate through a list of bidistate*.
      /// For each of these bidistates, get the corresponding tgba_state and
      /// compare it to \a tgba_state.
      bidistate* get_bidistate(const spot::state* tgba_state) const;

      const spot::state* get_tgba_state(bidistate* bidistate) const;

      bidigraph_states& get_bidistates() const;

      friend SPOT_API std::ostream&
      operator<<(std::ostream& os, const bidigraph& bdg);

    private:
      bidigraph_states* states_;
      std::unordered_map<bidistate*, const spot::state*> bidistate_to_tgba_;
      std::unordered_map<const spot::state*, bidistate*,
                         const spot::state_ptr_hash,
                         spot::state_ptr_equal> tgba_to_bidistate_;

      /// Check the existance of a state in the bidigraph
      bool exist(const spot::state* tgba_state) const;

      void create_bidistate(const spot::state* tgba_state);
      virtual void process_state(const state* s, int n, tgba_succ_iterator* si);
      virtual void process_link(const state* in_s, int in,
                                const state* out_s, int out,
                                const tgba_succ_iterator* si);

    };
  } // graph namespace
}

#endif // SPOT_GRAPH_BIDIGRAPH_HH
