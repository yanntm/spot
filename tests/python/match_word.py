# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2018 Laboratoire de Recherche et
# Développement de l'Epita (LRDE).
# This file is part of Spot, a model checking library.
#
# Spot is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# Spot is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import sys
import spot

inputs = ['G a', 'a;a;a;cycle{a;a;a}', True,
          'G b', 'a;a;a;cycle{a;a;a}', False,
          'G a', 'a;a;a;cycle{a;b;a}', False,
          'G a', 'a;a;b;cycle{a;a;a}', False,
                                        
          'F a', 'a;a;a;cycle{a;a;a}', True,
          'F b', 'a;a;a;cycle{a;a;a}', False,
          'F a', 'b;b;b;cycle{b;a;b}', True,
          'F b', 'a;a;b;cycle{a;a;a}', True,

          'a U b', 'a;a;a;cycle{a;a;a}', False,
          'a W b', 'a;a;a;cycle{a;a;a}', True,
          'b M a', 'a;a;a;cycle{a;a;a}', False,
          'b R a', 'a;a;a;cycle{a;a;a}', True,

          'a U b', 'a;a;a;cycle{a;b;a}', True,
          'a U b', 'a;a;a;cycle{a;a&b;a}', True,
          'a U b', 'a;a;b;cycle{a;a;a}', True,
          'a U b', 'b;a;a;cycle{a;a;a}', True,
          'a U b', 'c;c;b;cycle{a;a&b;a}', False,
          'a U b', 'a;a;a;cycle{c;a&b;a}', False,

          'a W b', 'a;a;a;cycle{a;b;a}', True,
          'a W b', 'a;a;a;cycle{a;a&b;a}', True,
          'a W b', 'a;a;b;cycle{a;a;a}', True,
          'a W b', 'b;a;a;cycle{a;a;a}', True,
          'a W b', 'c;c;b;cycle{a;a&b;a}', False,
          'a W b', 'a;a;a;cycle{c;a&b;a}', False,

          'b M a', 'a;a;a;cycle{a;b;a}', False,
          'b M a', 'a;a;a;cycle{a;a&b;a}', True,
          'b M a', 'a;a;b;cycle{a;a;a}', False,
          'b M a', 'a;a;a&b;cycle{a;a;a}', True,
          'b M a', 'b;a;a;cycle{a;a;a}', False,
          'b M a', 'a&b;a;a;cycle{a;a;a}', True,
          'b M a', 'c;c;b;cycle{a;a&b;a}', False,
          'b M a', 'a;a;a;cycle{c;a&b;a}', False,

          'b R a', 'a;a;a;cycle{a;b;a}', False,
          'b R a', 'a;a;a;cycle{a;a&b;a}', True,
          'b R a', 'a;a;b;cycle{a;a;a}', False,
          'b R a', 'a;a;a&b;cycle{a;a;a}', True,
          'b R a', 'b;a;a;cycle{a;a;a}', False,
          'b R a', 'a&b;a;a;cycle{a;a;a}', True,
          'b R a', 'c;c;b;cycle{a;a&b;a}', False,
          'b R a', 'a;a;a;cycle{c;a&b;a}', False,

          '!a', 'a;a;a;cycle{c;a&b;a}', False,
          '!b', 'a;a;a;cycle{c;a&b;a}', True,

          'X a', 'a;a;a;cycle{c;a&b;a}', True,
          'X b', 'a;a;a;cycle{c;a&b;a}', False,
          'X b', 'a;b;a;cycle{c;a&b;a}', True,
          'X b', 'a;a&b;a;cycle{c;a&b;a}', True,

          'a & b', 'a&b;a&b;a&b;a&b;cycle{a&b;a&b}', True,
          'a & b', 'b;a&b;a&b;a&b;cycle{a&b;a&b}', False,

          'a | b', 'a&b;a&b;a&b;a&b;cycle{a&b;a&b}', True,
          'a | b', 'b;a&b;a&b;a&b;cycle{a&b;a&b}', True,
          'a | b', 'c;a&b;a&b;a&b;cycle{a&b;a&b}', False,
          
          'a U (b & c)', 'a&b;a&c;cycle{a&b;b&c}', True,
          'FGa', 'b;b;b;cycle{a;a;a}', True,
          'GFa', 'b;b;b;cycle{b;a;b}', True,
          '!a U (b & c)', 'b;c;cycle{b;b&c}', True,
          '!a U (b & c)', 'b;c;cycle{a&b;b&c}', False,
          'FGa', 'b;b;b;cycle{b;a;a}', False,
          'GFa', 'b;b;a;cycle{b;b;b}', False,
          
          'a U b', 'cycle{a;b}', True,
          'F(G(b & c) M a)', 'd;d;b&c;cycle{b&c;b&c&a}', True,
          'F a', 'cycle{b}', False,
          'a & b', 'cycle{a&b;b&c}', True ]


assert(len(inputs) % 3 == 0)
for i in range(0, int(len(inputs) / 3)):
    f = spot.formula(inputs[3 * i])
    w = spot.parse_word(inputs[3 * i + 1])
    assert(spot.match_word(f, w) == inputs[3 * i + 2])
