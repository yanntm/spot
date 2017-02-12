// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et Développement de l'Epita
// (LRDE).
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 3 of the License, or (at your option) any later
// version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.

#include <spot/algebra/semigroup.hh>

#include <iostream>

namespace spot
{

  void
  explicit_fsg::print(std::ostream& os) const
  {
    os << " |";
    for (unsigned i = 0; i != law_.size(); ++i)
      {
        os << i << ',';
      }
    os << '\n';
    for (unsigned i = 0; i != law_.size(); ++i)
      {
        os << "--";
      }
    os << '\n';

    for (unsigned i = 0; i != law_.size(); ++i)
      {
        os << i << '|';
        for (unsigned j = 0; j != law_.size(); ++j)
          {
            os << product(i, j) << ',';
          }
        os << '\n';
      }
  }

  /// a pretty printer for words
  template<>
  std::ostream& operator<<<unsigned>(std::ostream& os, const word<unsigned>& w)
  {
    // the empty word is printed ε (in unicode)
    if (w.empty())
      os << "\u03b5";
    for (unsigned e : w)
      os << char('a' + e);
    return os;
  }
}
