// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et Développement de l'Epita.
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

#pragma once

#include <memory>

#include <spot/twa/twa.hh>
#include <spot/kripke/kripke.hh>
#include <spot/kripke/kripkegraph.hh>

// from TChecker via libtchecker.la
#include <base/singleton.hh>
#include <semantics/semantics.hh>
#include <semantics/zg.hh>

namespace spot
{

  /// \ingroup zgastwa
  /// \brief Class for a state in a zone graph
  class SPOT_API zg_as_twa_state: public kripke_graph_state,
                                  public semantics::zg::state_t
  {
  public:
    /// \brief Constructor
    /// The \a cond argument is useless here, it is used to be
    /// in accord with kripke representation
    zg_as_twa_state(bdd cond = bddfalse)
      : kripke_graph_state(cond), semantics::zg::state_t()
    {
    }

    /// \brief Copy constructor
    zg_as_twa_state(zg_as_twa_state const & s)
      : kripke_graph_state(s.cond()), semantics::zg::state_t(s)
    {
      _zone = s._zone->copy();
    }

    ~zg_as_twa_state()
    {}

    virtual int compare(const spot::state* other) const override
    {
      auto o = down_cast<const zg_as_twa_state*>(other);

      SPOT_ASSERT(_zone != nullptr);
      SPOT_ASSERT(o->_zone != nullptr);

      if (this->syntax::state_t::operator==(*o) &&
          (*_zone == *(o->_zone)))
        return 0;
      else if (o < this)
        return -1;
      else
        return 1;
    }

    virtual size_t hash() const override
    {
      SPOT_ASSERT(_zone != nullptr);
      size_t seed = this->syntax::state_t::hash();
      boost::hash_combine(seed, _zone->hash());
      return seed;
    }

    virtual zg_as_twa_state* clone() const override
    {
      // from zg
      return new zg_as_twa_state (*this);

      // from kripke
      //return const_cast<zg_as_twa_state*>(this);
    }

    virtual void destroy() const override
    {}
  };

  /// \ingroup zgastwa
  /// \brief Class for the iterator of edges of the automaton
  class SPOT_API zg_as_twa_succ_iterator: public kripke_succ_iterator
  {
  private:
    semantics::zg::zone_graph_t* zone_graph_;
    const zg_as_twa_state src_state_;
    semantics::zg::edge_range_t edge_list_;
    syntax::vedge_t vedge_;

  public:
    zg_as_twa_succ_iterator(syntax::model_t& model,
                            const zg_as_twa_state& state,
                            const bdd& cond = bddfalse)
      : kripke_succ_iterator(cond),
        zone_graph_(semantics::zg::factory_t::instance().new_zone_graph(model)),
        src_state_(state), edge_list_(zone_graph_->outgoing_edges(src_state_)),
        vedge_(nullptr)
    {}

    zg_as_twa_succ_iterator(semantics::zg::zone_graph_t* z_g,
                            const zg_as_twa_state& state,
                            const bdd& cond = bddfalse)
      : kripke_succ_iterator(cond), zone_graph_(z_g), src_state_(state),
        edge_list_(z_g->outgoing_edges(state)), vedge_(nullptr)
    {}

    virtual ~zg_as_twa_succ_iterator()
    {}

    virtual bool first() override
    {
      edge_list_.first();
      if (this->done())
        return false;

      vedge_ = edge_list_.current();
      return !this->done();
    }

    virtual bool next() override
    {
      /*
        In TChecker, there is a test to verify if the next status
        is valid or not, to skip it during exploring.
        Maybe it's just for the DST or maybe it's necessary ?
      */
      if (this->done())
        return false;

      edge_list_.advance();
      if (this->done())
        return false;

      vedge_ = edge_list_.current();
      return true;
    }

    virtual bool done() const override
    {
      return edge_list_.at_end();
    }

    virtual const zg_as_twa_state* dst() const override
    {
      SPOT_ASSERT(!done());
      zg_as_twa_state res;
      zone_graph_->next(src_state_, vedge_, res);
      return new zg_as_twa_state(res);
    }
  };

  /// \ingroup zgastwa
  /// \brief Class for representation in Spot of a timed automaton
  class SPOT_API zg_as_twa: public kripke
  {
  private:
    semantics::zg::zone_graph_t* zone_graph_;
    labels_t useless_accepting_labels;
    zg_as_twa_state init_state_;

  public:
    zg_as_twa(const bdd_dict_ptr& d, syntax::model_t& model,
              semantics::sem_flags_t flags = semantics::SEM_DEFAULT)
      : kripke(d)
    {
      zone_graph_ = semantics::zg::factory_t::instance().new_zone_graph(model, flags);
      zone_graph_->initial(init_state_);
    }

    ~zg_as_twa()
    {
      // FIXME delete zone_graph_; ?????
    }

    virtual const zg_as_twa_state* get_init_state() const override
    {
      return &init_state_;
    }

    virtual zg_as_twa_succ_iterator*
    succ_iter(const state* st) const override
    {
      const zg_as_twa_state* zg_st = down_cast<const zg_as_twa_state*>(st);
      return new zg_as_twa_succ_iterator(zone_graph_, *zg_st);
    }

    virtual bdd state_condition(const state* s) const override
    {
      auto zg_s = down_cast<const zg_as_twa_state*>(s);
      return zg_s->cond();
    }

    virtual std::string format_state(const state* s) const override
    {
      std::stringstream ss;
      auto zg_s = down_cast<const zg_as_twa_state*>(s);
      zg_s->output(ss);
      return ss.str();
    }
  };
}
