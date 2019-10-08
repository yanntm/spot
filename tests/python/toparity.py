#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2018, 2019 Laboratoire de Recherche et Développement de
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

a = spot.automaton("""HOA: v1
States: 1
Start: 0
AP: 2 "a" "b"
Acceptance: 2 Inf(0)|Inf(1)
--BODY--
State: 0
[0&1] 0 {0 1}
[0&!1] 0 {0}
[!0&1] 0 {1}
[!0&!1] 0
--END--""")
p = spot.to_parity(a)
assert spot.are_equivalent(a, p)

a = spot.automaton("""
HOA: v1 States: 6 Start: 0 AP: 2 "p0" "p1" Acceptance: 6 Inf(5) |
Fin(2) | Inf(1) | (Inf(0) & Fin(3)) | Inf(4) properties: trans-labels
explicit-labels trans-acc --BODY-- State: 0 [0&1] 2 {4 5} [0&1] 4 {0 4}
[!0&!1] 3 {3 5} State: 1 [0&!1] 3 {1 5} [!0&!1] 5 {0 1} State: 2 [!0&!1]
0 {0 3} [0&!1] 1 {0} State: 3 [!0&1] 4 {1 2 3} [0&1] 3 {3 4 5} State:
4 [!0&!1] 1 {2 4} State: 5 [!0&1] 4 --END--
""")
p = spot.to_parity(a)
assert p.num_states() == 22
assert spot.are_equivalent(a, p)

for f in spot.randltl(4, 400):
    d = spot.translate(f, "det", "G")
    p = spot.to_parity(d)
    assert spot.are_equivalent(p, d)

for f in spot.randltl(5, 2000):
    n = spot.translate(f)
    p = spot.to_parity(n)
    assert spot.are_equivalent(n, p)

# Issue #390.
a = spot.translate('!(GFa -> (GFb & GF(!b & !Xb)))', 'gen', 'det')
b = spot.to_parity(a)
assert a.equivalent_to(b)
