// -*- coding: utf-8 -*-
// Copyright (C) 2013, 2014, 2015 Laboratoire de Recherche et
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

#include "ltlast/allnodes.hh"
#include "ltlvisit/simplify.hh"
#include "ltlvisit/clone.hh"
#include "ltlvisit/apcollect.hh"
#include "ltlvisit/remove_x.hh"

namespace spot
{
  namespace ltl
  {
    namespace
    {

#define AND(x, y)      multop::instance(multop::And, (x), (y))
#define OR(x, y)       multop::instance(multop::Or, (x), (y))
#define ORRAT(x, y)    multop::instance(multop::OrRat, (x), (y))
#define NOT(x)         unop::instance(unop::Not, (x))
#define G(x)           unop::instance(unop::G, (x))
#define U(x, y)        binop::instance(binop::U, (x), (y))
#define STAR(x)        bunop::instance(bunop::Star, (x), 0)
#define PLUS(x)        bunop::instance(bunop::Star, (x), 1)
#define FSTAR(x, i, j) bunop::instance(bunop::FStar, (x), (i), (j))
#define FUSION(x, y)   multop::instance(multop::Fusion, (x), (y))
#define CONCAT(x, y)   multop::instance(multop::Concat, (x), (y))

      // This is the κ function from Dax at al. 2009, that rewrite a
      // SERE into an siSERE.  It is trivially extended to also accept
      // that FStar operator in the input.
      class kappa : public clone_visitor
      {
	typedef clone_visitor super;

	// atomic proposition assignments of the
	// form   !p0 & !p1 & p2 & p3 & !p4 ...
	std::vector<const formula*> two_p;
      public:
	kappa(const formula* sere)
	{
	  atomic_prop_set aps;
	  atomic_prop_collect(sere, &aps);

	  std::vector<multop::vec> tmp{
	    {constant::true_instance()}
	  };
	  for (auto& p: aps)
	    {
	      size_t n = tmp.size();
	      for (size_t i = 0; i < n; ++i)
		{
		  multop::vec v(tmp[i]);
		  for (auto vv: v)
		    vv->clone();
		  v.push_back(p->clone());
		  tmp[i].push_back(NOT(p->clone()));
		  tmp.emplace_back(std::move(v));
		}
	    }
	  two_p.reserve(tmp.size());
	  for (auto& v: tmp)
	    two_p.emplace_back(multop::instance(multop::And,
						new multop::vec(v)));
	}
	virtual ~kappa() override
	{
	  for (auto& v: two_p)
	    v->destroy();
	}

	using super::visit;
	void visit(const multop* uo) override
	{
	  if (uo->op() != multop::Concat)
	    {
	      super::visit(uo);
	      return;
	    }

	  auto ks = recurse(uo->nth(0));
	  auto kt = recurse(uo->all_but(0));

	  auto bigu = new multop::vec;
	  for (auto& a: two_p)
	    bigu->push_back(FUSION(PLUS(a->clone()),
				   CONCAT(STAR(a->clone()), kt->clone())));
	  result_ = FUSION(ks, multop::instance(multop::OrRat, bigu));
	  if (uo->nth(0)->accepts_eword())
	    result_ = ORRAT(result_, kt);
	  else
	    kt->destroy();
	}

	void visit(const bunop* bo) override
	{
	  if (bo->op() != bunop::Star)
	    {
	      super::visit(bo);
	      return;
	    }

	  // The rule is inspired from from the Dax et al. paper, but
	  // handles bounded Star operators.
	  //
	  // A first remark is about the following rule for κ(s*) in the
	  // paper:
	  //   ε || κ(s) || κ(s):(...;κ(s))[:+]
	  // It can be written as
	  //   ε || κ(s):(...;κ(s))[:*]
	  // because the latter is simply equivalent to
	  //   ε || (κ(s):1) || κ(s):(...;κ(s))[:+]
	  // and the only thing :1 does on (κ(s):1) is to remove any
	  // empty word.
	  //
	  // Now consider κ(s[*2]).  If we follow the rule for κ(s;s)
	  // we should output:
	  //   κ(s):(...;κ(s)) || κ(s)    if s accepts ε, or just
	  //   κ(s):(...;κ(s))            oherwise
	  // we replace this output by
	  //   κ(s):(...;κ(s))[:*0..1] || ε  if s accepts ε, or just
	  //   κ(s):(...;κ(s))[:*1]          oherwise
	  //
	  // For the general case, we use κ(s[*i..j]) =
	  //   κ(s):(...;κ(s))[:*0..j-1] || ε    if s accepts ε or i = 0
	  //   κ(s):(...;κ(s))[:*i-1..j-1]       if i>0 and s accepts ε
	  //
	  // in case j = unbounded, we of course assume that j-1 = unbounded.

	  auto ks = recurse(bo->child());

	  unsigned min = 0;
	  bool e = bo->child()->accepts_eword();
	  if (!e)
	    {
	      min = bo->min();
	      if (min > 0)
		--min;
	    }
	  unsigned max = bo->max();
	  assert(max != 0);
	  if (max != bunop::unbounded)
	    --max;

	  auto bigu = new multop::vec;
	  for (auto& a: two_p)
	    {
	      bigu->push_back(FUSION(PLUS(a->clone()),
				     CONCAT(STAR(a->clone()), ks->clone())));
	      auto f = CONCAT(STAR(a->clone()), ks->clone());
	      f->destroy();
	    }
	  result_ =
	    FUSION(ks, FSTAR(multop::instance(multop::OrRat, bigu), min, max));
	  if (e)
	    result_ = ORRAT(result_, constant::empty_word_instance());
	}

	virtual const formula* recurse(const formula* f) override
	{
	  if (f->is_boolean())
	    return PLUS(f->clone());
	  if (f->is_syntactic_stutter_invariant())
	    return f->clone();
	  f->accept(*this);
	  return this->result();
	}
      };

      const formula* replace_sere(const formula* sere)
      {
	kappa k(sere);
	return k.recurse(sere);
      }

      class remove_x_visitor : public clone_visitor
      {
	typedef clone_visitor super;
	atomic_prop_set aps;

      public:
	remove_x_visitor(const formula* f)
	{
	  atomic_prop_collect(f, &aps);
	}

	virtual
	~remove_x_visitor()
	{
	}

	using super::visit;

	void visit(const binop* bo) override
	{
	  binop::type op = bo->op();
	  switch (op)
	    {
	    case binop::Xor:
	    case binop::Implies:
	    case binop::Equiv:
	    case binop::U:
	    case binop::R:
	    case binop::W:
	    case binop::M:
	      result_ = binop::instance(op,
					recurse(bo->first()),
					recurse(bo->second()));
	      return;
	    case binop::EConcat:
	    case binop::EConcatMarked:
	    case binop::UConcat:
	      result_ = binop::instance(op,
					replace_sere(bo->first()),
					recurse(bo->second()));
	      return;
	    }
	  SPOT_UNREACHABLE();
	}

	void visit(const unop* uo) override
	{

	  unop::type op = uo->op();
	  switch (op)
	    {
	    case unop::Not:
	    case unop::F:
	    case unop::G:
	      result_ = unop::instance(op, recurse(uo->child()));
	      return;
	    case unop::X:
	      {
		const formula* c = recurse(uo->child());
		multop::vec* vo = new multop::vec;
		for (atomic_prop_set::const_iterator i = aps.begin();
		     i != aps.end(); ++i)
		  {
		    // First line
		    multop::vec* va1 = new multop::vec;
		    const formula* npi = NOT((*i)->clone());
		    va1->push_back((*i)->clone());
		    va1->push_back(U((*i)->clone(), AND(npi, c->clone())));
		    for (atomic_prop_set::const_iterator j = aps.begin();
			 j != aps.end(); ++j)
		      if (*j != *i)
			va1->push_back(OR(U((*j)->clone(), npi->clone()),
					  U(NOT((*j)->clone()), npi->clone())));
		    vo->push_back(multop::instance(multop::And, va1));
		    // Second line
		    multop::vec* va2 = new multop::vec;
		    va2->push_back(npi->clone());
		    va2->push_back(U(npi->clone(),
				     AND((*i)->clone(), c->clone())));
		    for (atomic_prop_set::const_iterator j = aps.begin();
			 j != aps.end(); ++j)
		      if (*j != *i)
			va2->push_back(OR(U((*j)->clone(), (*i)->clone()),
					  U(NOT((*j)->clone()),
					    (*i)->clone())));
		    vo->push_back(multop::instance(multop::And, va2));
		  }
		const formula* l12 = multop::instance(multop::Or, vo);
		// Third line
		multop::vec* va3 = new multop::vec;
		for (atomic_prop_set::const_iterator i = aps.begin();
		     i != aps.end(); ++i)
		  {
		    va3->push_back(OR(G((*i)->clone()),
				      G(NOT((*i)->clone()))));
		  }
		result_ = OR(l12, AND(multop::instance(multop::And, va3), c));
		return;
	      }
	    case unop::Closure:
	    case unop::NegClosure:
	    case unop::NegClosureMarked:
	      result_ = unop::instance(op, replace_sere(uo->child()));
	      return;
	    }
	  SPOT_UNREACHABLE();
	}

	virtual const formula* recurse(const formula* f) override
	{
	  if (f->is_syntactic_stutter_invariant())
	    return f->clone();
	  f->accept(*this);
	  return this->result();
	}
      };

    }

    const formula* remove_x(const formula* f)
    {
      remove_x_visitor v(f);
      return v.recurse(f);
    }
  }
}
