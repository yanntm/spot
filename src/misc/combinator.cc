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

#include "combinator.hh"

namespace spot
{
  Combinator::Tree::Tree (bdd val) :
    child_ (),
    val_ (val),
    done_ (false)
  {
  }

  Combinator::Tree::~Tree ()
  {
    Combinator::Tree::iterator i = this->begin ();
    for (; i != this->end (); i++)
      delete *i;
  }

  void
  Combinator::Tree::add_child (Tree* c)
  {
    child_.push_back (c);
  }

  void
  Combinator::Tree::check_done ()
  {
    bool res = true;

    Tree::iterator i;
    for (i = child_.begin (); i != child_.end (); i ++)
      {
	res &= (*i)->done_;
      }

    this->done_ = res;
  }

  bool
  Combinator::Tree::done_get ()
  {
    return this->done_;
  }

  bdd
  Combinator::Tree::val_get ()
  {
    return this->val_;
  }

  std::list<Combinator::Tree*>*
  Combinator::Tree::child_get ()
  {
    return &child_;
  }

  Combinator::Tree::iterator
  Combinator::Tree::begin ()
  {
    return this->child_.begin ();
  }

  Combinator::Tree::iterator
  Combinator::Tree::end ()
  {
    return this->child_.end ();
  }

  Combinator::Combinator (bdd acc_all)
  {
    Combinator::Tree* root = new Combinator::Tree (bddfalse);
    int n = 0;

    std::list<bdd> l;

    while (acc_all != bddfalse)
      {
	bdd next = bdd_satone (acc_all);
	acc_all -= next;
	l.push_back (next);
	++n;
      }

    int i = 0;
    while (i < n)
    {
      root->add_child (build (l));
      l.push_back (l.front ());
      l.pop_front ();
      ++i;
    }
    t_ = root;
  }

  Combinator::~Combinator ()
  {
    delete t_;
  }

  std::list<bdd>*
  Combinator::operator() ()
  {
    if (t_->done_get ())
      return 0;

    std::list<bdd>* res = new std::list<bdd> ();
    op_rec (t_, res);

    return res;
  }

  void
  Combinator::op_rec (Tree* t, std::list<bdd>* l)
  {
    if (t->val_get () != bddfalse)
      l->push_back (t->val_get ());
    Tree::iterator i = t->begin ();

    while (i != t->end () && (*i)->done_get ())
      ++i;

    if (i != t->end ())
      op_rec (*i, l);

    t->check_done ();
  }

  Combinator::Tree*
  Combinator::build (std::list<bdd> l)
  {
    bdd cur = l.front ();
    l.pop_front ();
    Tree* res = new Tree (cur);
    int n = l.size ();
    int i = 0;
    while (i < n)
      {
	res->add_child (build (l));
	l.push_back (l.front ());
	l.pop_front ();
	++i;
      }

    return res;
  }

  size_t
  Combinator::tgba_size (const tgba* a, size_t max)
  {
    std::set<size_t> seen;
    std::queue<state*> tovisit;
    state* init = a->get_init_state ();
    size_t hash = init->hash ();

    tovisit.push (init);
    seen.insert (hash);
    size_t count = 0;

    while (!tovisit.empty () && (max == 0 || count < max))
    {
      ++count;
      state* cur = tovisit.front ();

      tovisit.pop ();
      tgba_succ_iterator* i = a->succ_iter (cur);

      for (i->first (); !i->done (); i->next ())
      {
	state* dst = i->current_state ();
	hash = dst->hash ();

	if (seen.find (hash) == seen.end ())
	{
	  tovisit.push (dst);
	  seen.insert (hash);
	}
	else
	  dst->destroy ();
      }
      delete i;
      cur->destroy ();
    }

    return count;
  }
}
