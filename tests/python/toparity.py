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
p = spot.to_parity(a, True)
assert spot.are_equivalent(a, p)
assert p.to_str() == """HOA: v1
States: 2
Start: 0
AP: 2 "a" "b"
acc-name: Streett 1
Acceptance: 2 Fin(0) | Inf(1)
properties: trans-labels explicit-labels trans-acc colored complete
properties: deterministic
--BODY--
State: 0 "0 [0@0]"
[0&1] 0 {1}
[0&!1] 1 {0}
[!0&1] 0 {1}
[!0&!1] 0 {0}
State: 1 "0 [0@1]"
[0&1] 0 {1}
[0&!1] 1 {1}
[!0&1] 0 {1}
[!0&!1] 1 {0}
--END--"""

for f in spot.randltl(4, 400):
    d = spot.translate(f, "det", "G")
    p = spot.to_parity(d)
    #assert spot.are_equivalent(p, d)
    if not spot.are_equivalent(p, d):
        print(f)
        print(p.to_str())
        assert False

for f in spot.randltl(5, 2000):
    n = spot.translate(f)
    p = spot.to_parity(n)
    assert spot.are_equivalent(n, p)

