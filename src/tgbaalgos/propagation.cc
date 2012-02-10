// Copyright (C) 2009, 2011 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
// Copyright (C) 2003, 2004 Laboratoire d'Informatique de Paris 6 (LIP6),
// département Systèmes Répartis Coopératifs (SRC), Université Pierre
// et Marie Curie.
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Spot; see the file COPYING.  If not, write to the Free
// Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.

#include "propagation.hh"

#include "tgba/tgbaexplicit.hh"
#include "reachiter.hh"
#include "misc/hash.hh"

namespace spot
{
  typedef Sgi::hash_set<const state*,
			state_ptr_hash, state_ptr_equal> state_set;
  typedef Sgi::hash_map<const state*, bdd,
			state_ptr_hash, state_ptr_equal> state_map;
  typedef Sgi::hash_map<const state*, unsigned int,
			state_ptr_hash, state_ptr_equal> exp_map;
  namespace
  {
    // return true if a change has been made to the acc map during the run
    // false otherwise
    bool
    propagation_dfs(const tgba* a, const state* s,
		    state_set& seen, state_map& acc)
    {
      bool changed = false;

      seen.insert(s);

      if (acc.find(s) == acc.end())
	acc.insert(std::make_pair<const state*, bdd>(s, bddtrue));

      tgba_succ_iterator* i = a->succ_iter(s);
      for (i->first(); !i->done(); i->next())
	{
	  bdd cmp = acc[s];
	  acc[s] &= i->current_acceptance_conditions();
	  changed |= cmp != acc[s];
	}

      delete i;

      i = a->succ_iter(s);
      for (i->first(); !i->done(); i->next())
	{
	  state* to = i->current_state();

	  if (seen.find(to) == seen.end())
	    changed |= propagation_dfs(a, to, seen, acc);

	  bdd cmp = acc[s];
	  acc[s] |= acc[to];
	  changed |= cmp != acc[s];

	  to->destroy();
	}
      delete i;

      return changed;
    }

    template<typename T>
    class rewrite_automata: public tgba_reachable_iterator_depth_first
    {
    public:
      rewrite_automata(const T* a,
			state_map& acc)
	: tgba_reachable_iterator_depth_first(a),
	  acc_(acc),
	  rec_(),
	  newa_(new T(a->get_dict())),
	  basea_(a)
      {
      }

      void
      process_state(const state* s, int n, tgba_succ_iterator* si)
      {
	(void) si;

	if (rec_.find(s) == rec_.end())
	  rec_.insert(std::make_pair<const state*, unsigned int>(s, n));
      }

      void
      process_link(const state* in_s, int in,
		    const state* out_s, int out,
		    const tgba_succ_iterator* si)
      {
	(void) in;
	(void) out;

	const tgba_explicit_succ_iterator* sie =
	  down_cast<const tgba_explicit_succ_iterator*>(si);

	const state_explicit* in_se =
	  down_cast<const state_explicit*>(in_s);

	tgba_explicit::transition* t =
	  newa_->clone_transition(basea_, in_se, sie);

	bdd newacc = si->current_acceptance_conditions() | acc_[out_s];
	newa_->add_acceptance_conditions(t, newacc);
	newa_->add_conditions(t, si->current_condition());
      }

      T*
      get_new_automata() const
      {
	return newa_;
      }

    protected:
      state_map& acc_;
      exp_map rec_;
      T* newa_;
      const T* basea_;
    };

    const tgba_explicit*
    rewrite(const tgba* a, state_map& acc)
    {
      const tgba_explicit* res;

      const tgba_explicit_formula* tef;
      const tgba_explicit_number* ten;
      const tgba_explicit_string* tes;

      if ((tef = dynamic_cast<const tgba_explicit_formula*>(a)))
	{
	  rewrite_automata<tgba_explicit_formula> rw(tef, acc);
	  rw.run();
	  res = rw.get_new_automata();
	}
      else if ((ten = dynamic_cast<const tgba_explicit_number*>(a)))
	{
	  rewrite_automata<tgba_explicit_number> rw(ten, acc);
	  rw.run();
	  res = rw.get_new_automata();
	}
      else if ((tes = dynamic_cast<const tgba_explicit_string*>(a)))
	{
	  rewrite_automata<tgba_explicit_string> rw(tes, acc);
	  rw.run();
	  res = rw.get_new_automata();
	}
      else
	{
	  std::cout << "unhandled case" << std::endl;
	  assert(false);
	  return 0;
	}

      return res;
    }

  }

  const tgba*
  propagate_acceptance_conditions(const tgba* a)
  {
    state_set seen;
    state_map acc;
    state* init_state = a->get_init_state();

    while (propagation_dfs(a, init_state, seen, acc))
      {
      }

    init_state->destroy();

    //rewrite automata
    return rewrite (a, acc);
  }

  namespace
  {
    class rewrite_inplace: public tgba_reachable_iterator_depth_first
    {
    public:
      rewrite_inplace(tgba_explicit* a,
		       state_map& acc)
	: tgba_reachable_iterator_depth_first(a),
	  acc_(acc),
	  ea_(a)
      {
      }

      void
      process_link(const state* in_s, int in,
		    const state* out_s, int out,
		    const tgba_succ_iterator* si)
      {
	  (void) in_s;
	  (void) in;
	  (void) out;

	  const tgba_explicit_succ_iterator* tmpit =
	    down_cast<const tgba_explicit_succ_iterator*>(si);

	  tgba_explicit::transition* t = ea_->get_transition(tmpit);
	  t->acceptance_conditions |= acc_[out_s];
      }

    protected:
      state_map& acc_;
      tgba_explicit* ea_;
    };
  }

  void
  propagate_acceptance_conditions_inplace(tgba_explicit* a)
  {
    state_set seen;
    state_map acc;
    state* init_state = a->get_init_state();

    while (propagation_dfs(a, init_state, seen, acc))
      {
      }

    init_state->destroy();

    rewrite_inplace ri(a, acc);
    ri.run();
  }
}
