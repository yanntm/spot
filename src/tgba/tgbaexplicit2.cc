// Copyright (C) 2012 Laboratoire de Recherche et Développement
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
// 02111-1307, USA.

#include "tgbaexplicit2.hh"

namespace spot
{
  const std::string state_explicit_string::default_val ("empty");
  //just a random function to test the implementation
  void test ()
  {
    bdd_dict* d = 0;
    tgba_explicit<state_explicit_string> tgba(d);
    state_explicit_string* s1 = tgba.add_state ("toto");
    state_explicit_string* s2 = tgba.add_state ("tata");
    state_explicit_string::transition* t =
      tgba.create_transition (s1, s2);

    tgba_explicit_succ_iterator<state_explicit_string>* it = tgba.succ_iter (tgba.get_init_state ());
    for (it->first (); !it->done();it->next ())
    {
      state_explicit_string* se = it->current_state ();
      std::cout << (se)->label () << std::endl;
    }

    (void) tgba;
    (void) s1;
    (void) s2;
    (void) t;
  }
}
