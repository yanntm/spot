# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2018 Laboratoire de Recherche et
# Développement de l'Epita (LRDE).
#
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

input1 = ['b S a', 'S',
          'b E a', 'E',
          'H a', 'H',
          'O a', 'O',
          'Y a', 'Y',
          ]
assert(len(input1) % 2 == 0)
for i in range (0, int(len(input1) / 2)):
    f = spot.formula(input1[i * 2])
    print(f)
    assert(f.kindstr() == input1[i * 2 + 1])


input2 = ['G(a S b)', 'G(Oa)', 'F(Hc)', 'F(a E b)']
for i in range (0 , len(input2)):
    f = spot.formula(input2[i])
    print( ' -- ')
    print(input2[i])
    g = spot.translate_past(f)
    print(g)
    print( ' -- ')

for i in range(0, len(input2)):
    print(input2[i])
    v = spot.translate_past(spot.formula(input2[i]))
    print(v)
    v.translate()
