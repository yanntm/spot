#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2017 Laboratoire de Recherche et Développement de
# l'EPITA.
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

import time

import spot

g = spot.automata('randaut -A 4 -Q5 -n -1 2|')
end = time.time() + 10
while time.time() < end:
    a = spot.remove_fin(g.__next__())
    b = spot.remove_fin(g.__next__())
    if (spot.two_aut_ec(a, b) != spot.otf_product(a, b).is_empty()):
        print("spot.two_aut_ec(a, b) != spot.otf_product(a, b).is_empty()")
        print("\na:\n" + a.to_str())
        print("\nb:\n" + b.to_str())
        exit(2)