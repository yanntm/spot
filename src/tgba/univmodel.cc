// -*- coding: utf-8 -*-
// Copyright (C) 2014 Laboratoire de Recherche et Développement de
// l'Epita.
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

#include "univmodel.hh"
#include "bddprint.hh"
#include "ltlvisit/simplify.hh"
#include "misc/bddlt.hh"

namespace spot
{
  namespace
  {
    class universal_model_state : public state
    {
      bdd val_;
    public:
      universal_model_state(bdd val) : val_(val)
      {
      }

      virtual
      ~universal_model_state()
      {
      }

      bdd get_label() const
      {
	return val_;
      }

      virtual int
      compare(const state* other) const
      {
	const universal_model_state* o =
	  down_cast<const universal_model_state*>(other);
	assert(o);
	int id = val_.id();
	int oid = o->get_label().id();
	if (id < oid)
	  return -1;
	if (id > oid)
	  return 1;
	return 0;
      }

      virtual size_t
      hash() const
      {
	return val_.id();
      }

      virtual
      universal_model_state* clone() const
      {
	return new universal_model_state(*this);
      }
    };

    class universal_model_succ_iterator: public tgba_succ_iterator
    {
      bdd source_;
      bdd vars_;
      bdd restrict_;
      bdd all_;
      bdd cur_;
    public:
      universal_model_succ_iterator(bdd source, bdd vars, bdd restrictval)
	: source_(source), vars_(vars), restrict_(restrictval), all_(restrictval), cur_(bddfalse)
      {
      }

      virtual
      ~universal_model_succ_iterator()
      {
      }

      // iteration

      void
      first()
      {
	all_ = restrict_;
	cur_ = bdd_satoneset(all_, vars_, bddfalse);
      }

      void
      next()
      {
	all_ -= cur_;
	cur_ = bdd_satoneset(all_, vars_, bddfalse);
      }

      bool
      done() const
      {
	return all_ == bddfalse;
      }

      // inspection

      universal_model_state*
      current_state() const
      {
	return new universal_model_state(cur_);
      }

      bdd
      current_condition() const
      {
	if (source_ == bddtrue)
	  return cur_;
	if (source_ == bddfalse)
	  return bddtrue;
	return source_;
      }

      bdd
      current_acceptance_conditions() const
      {
	return bddfalse;
      }
    };

    class universal_model_tgba: public tgba
    {
      bdd_dict* d_;
      bdd vars_;
      bdd restrict_;
      bool push_;
    public:
      universal_model_tgba(bdd_dict* d,
			   const ltl::atomic_prop_set* ap,
			   bdd restrictval,
			   bool push)
	: d_(d), restrict_(restrictval), push_(push)
      {
	bdd vars = bddtrue;
	for (ltl::atomic_prop_set::const_iterator i = ap->begin();
	     i != ap->end(); ++i)
	  vars &= bdd_ithvar(d->register_proposition(*i, this));
	// Add the support of restrictval in case it uses variables
	// that have not been listed in ap.
	bdd sup = bdd_support(restrictval);
	vars_ = vars & sup;
	d_->register_propositions(sup, this);
      }

      virtual ~universal_model_tgba()
      {
	d_->unregister_all_my_variables(this);
      }

      virtual state* get_init_state() const
      {
	if (push_)
	  return new universal_model_state(bddfalse);
	else
	  return new universal_model_state(bddtrue);
      }

      virtual tgba_succ_iterator*
      succ_iter(const state* s,
		const state* = 0,
		const tgba* = 0) const
      {
	if (!push_)
	  return new universal_model_succ_iterator(bddtrue, vars_, restrict_);

	const universal_model_state* us = down_cast<const universal_model_state*>(s);
	assert(us);
	return new universal_model_succ_iterator(us->get_label(), vars_, restrict_);
      }

      virtual bdd_dict*
      get_dict() const
      {
	return d_;
      }

      virtual std::string
      format_state(const state* ostate) const
      {
	const universal_model_state* s =
	  down_cast<const universal_model_state*>(ostate);
	assert(s);
	return bdd_format_formula(d_, s->get_label());
      }

      virtual bdd
      all_acceptance_conditions() const
      {
	return bddfalse;
      }

      virtual bdd
      neg_acceptance_conditions() const
      {
	return bddtrue;
      }

    protected:
      virtual bdd
      compute_support_conditions(const state*) const
      {
	return restrict_;
      }

      virtual bdd compute_support_variables(const state*) const
      {
	return vars_;
      }
    private:
      // Disallow copy.
      universal_model_tgba(const universal_model_tgba&);
      universal_model_tgba& operator=(const universal_model_tgba&);
    };

  }


  SPOT_API tgba*
  universal_model(bdd_dict* d, ltl::atomic_prop_set* ap, bdd restrictval, bool push)
  {
    return new universal_model_tgba(d, ap, restrictval, push);
  }


  SPOT_API tgba*
  universal_model(bdd_dict* d, ltl::atomic_prop_set* ap,
		  const ltl::formula* restrictval, bool push)
  {
    ltl::ltl_simplifier l(d);
    return universal_model(d, ap, l.as_bdd(restrictval), push);
  }

}
