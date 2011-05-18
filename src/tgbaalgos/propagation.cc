// Copyright (C) 2009, 2011 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
// Copyright (C) 2004, 2005 Laboratoire d'Informatique de Paris 6 (LIP6),
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

namespace spot
{
  Propagation::Propagation (const tgba* a) :
    a_ (a),
    acc_ (new acc_map ()),
    rec_ (new exp_map ()),
    count_ (1),
    again_ (false)
  {
    state_set* ss = new state_set ();
    spot::state* s =  a_->get_init_state ();

    dfs (s, ss);

    s->destroy ();
    delete ss;
  }

  Propagation::~Propagation ()
  {
    delete acc_;
    delete rec_;
  }

  void
  Propagation::dfs (state* s, state_set* seen)
  {
    tgba_succ_iterator* i = a_->succ_iter (s);
    size_t hash = s->hash ();
    seen->insert (hash);

    if (acc_->find (hash) == acc_->end ())
      acc_->insert (std::pair<size_t, bdd> (hash, bddtrue));

    //optimization : filling the map while descending
    for (i->first (); !i->done (); i->next ())
    {
      bdd cmp = (*acc_)[hash];
      (*acc_)[hash] &= i->current_acceptance_conditions ();
      again_ |= cmp != (*acc_)[hash];
    }

    delete i;
    i = a_->succ_iter (s);
    bdd inter = bddtrue;
    for (i->first (); !i->done (); i->next ())
    {
      state* to = i->current_state ();
      size_t tohash = to->hash ();

      //going through the graph
      if (seen->find (tohash) == seen->end ())
	dfs (to, seen);

      if ((*acc_)[tohash] != bddtrue)
	inter &= (*acc_)[tohash];
      else
	inter = bddfalse;
      to->destroy ();
    }
    delete i;

    if (inter != bddtrue)
    {
      bdd cmp = (*acc_)[hash];
      (*acc_)[hash] |= inter;
      again_ |= cmp != (*acc_)[hash];
    }
  }


  tgba*
  Propagation::propagate ()
  {
    tgba_explicit_string* res =
      new tgba_explicit_string (a_->get_dict ());

    spot::state* s = a_->get_init_state ();
    state_set* ss = new state_set ();

    int cpt = 0;
    while (again_)
    {
      if (cpt > 1)
	std::cerr << "Propagation more than one round" << std::endl;
      dfs (s, ss);
      ss->clear ();
      again_ = false;
      cpt++;
    }

    ss->clear ();
    propagate_run (res, s, ss);

    s->destroy ();
    delete ss;
    return res;
  }

  /**
   ** Reconstruct an automaton given the set of acceptance conditions
   **/
  void
  Propagation::propagate_run (tgba_explicit_string* a,
			      state* s,
			      state_set* seen)
  {
    size_t hash = s->hash ();
    seen->insert (hash);
    tgba_succ_iterator* i = a_->succ_iter (s);

    if (rec_->find (hash) == rec_->end ())
      rec_->insert (std::pair<size_t, int> (hash, count_++));

    for (i->first (); !i->done (); i->next ())
      {
	state* to = i->current_state ();
	size_t tohash = to->hash ();

	if (rec_->find (tohash) == rec_->end ())
	  rec_->insert (std::pair<size_t, int> (tohash, count_++));

	tgba_explicit::transition* t =
	  a->create_transition (tostr ((*rec_)[hash]),
				tostr ((*rec_)[tohash]));

	bdd newacc = i->current_acceptance_conditions ();
	if ((*acc_)[tohash] != bddtrue)
	  newacc |= (*acc_)[tohash];

	a->add_acceptance_conditions (t, newacc);
	a->add_conditions (t, i->current_condition ());

	if (seen->find (tohash) == seen->end ())
	  propagate_run (a, to, seen);

	to->destroy ();
      }

    delete i;
  }
}
