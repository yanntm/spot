// Copyright (C) 2011 Laboratoire de Recherche et Developpement
// de l'Epita (LRDE).
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
// 02111-1307, USA

#ifndef SPOT_MISC_COMBINATOR_HH
# define SPOT_MISC_COMBINATOR_HH

#include <list>
#include <queue>

#include "bdd.h"
#include "tgba/tgba.hh"

/// Data structure for the combinator class
namespace spot
{
  class Combinator
  {
  private:
    class Tree
    {
    public:
      Tree(bdd val);
      ~Tree();

      void add_child(Tree* c);
      void check_done();

      size_t hash()
      {
	return reinterpret_cast<char*>(this) - static_cast<char*>(0);
      }

      bool done_get();
      bdd val_get();
      std::list<Tree*>* child_get();

      typedef std::list<Tree*>::iterator iterator;
      iterator begin();
      iterator end();

    protected:
      std::list<Tree*> child_;
      bdd val_;
      bool done_;
    };

  public:
    Combinator(bdd acc_all);
    ~Combinator();

    std::list<bdd>* operator()();

    static size_t tgba_size(const tgba* a, size_t max = 0);

  protected:
    Tree* t_;

    Tree* build(std::list<bdd> l);
    void op_rec(Tree* t, std::list<bdd>* l);
  };
}

#endif /// !SPOT_MISC_COMBINATOR_HH
