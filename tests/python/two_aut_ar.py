#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2018 Laboratoire de Recherche et Développement de
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

import spot

def two_intersecting_automata(g):
    """return two random automata with a non-empty intersection"""
    for a, b in zip(g, g):
        if a.intersects(b):
            return a, b

def two_non_intersecting_automata(g):
    """return two random automata with an empty intersection"""
    for a, b in zip(g, g):
        if not a.intersects(b):
            return a, b

g = spot.automata('randaut -A "Inf(0)&Inf(1)|Inf(2)&Inf(3)" -Q5 -n -1'
                  ' --check=strength 2|')

a, b = two_intersecting_automata(g)
t = spot.two_aut_ec(a,b)
print(a.intersects(b))
print(bool(t))
ar, br = t.accepting_runs()
assert(ar.replay(spot.get_cout()))
assert(br.replay(spot.get_cout()))

a, b = two_non_intersecting_automata(g)
t = spot.two_aut_ec(a,b)
print(a.intersects(b))
print(bool(t))
assert(t is None)
