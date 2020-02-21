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
import time

a = spot.automaton("""
HOA: v1
name: "(FGp0 & ((XFp0 & F!p1) | F(Gp1 & XG!p0))) | G(F!p0 & (XFp0 | F!p1) &
F(Gp1 | G!p0))"
States: 14
Start: 0
AP: 2 "p1" "p0"
Acceptance: 6 (Fin(0) & Fin(1)) | ((Fin(4)|Fin(5)) & (Inf(2)&Inf(3)))
properties: trans-labels explicit-labels trans-acc complete
properties: deterministic
--BODY--
State: 0
[!0] 1
[0] 2
State: 1
[!0&!1] 1 {0 1 2 3 5}
[0&!1] 3
[!0&1] 4
[0&1] 5
State: 2
[0&!1] 2 {1}
[!0&1] 4
[!0&!1] 6
[0&1] 7
State: 3
[0&!1] 3 {1 3}
[!0&1] 4
[!0&!1] 6 {0 1 2 3 5}
[0&1] 8
State: 4
[!0&!1] 4 {1 2 3 5}
[!0&1] 4 {2 4 5}
[0&!1] 5 {1 3}
[0&1] 5 {4}
State: 5
[!0&1] 4 {2 4 5}
[0&!1] 5 {1 3}
[0&1] 8 {2 4}
[!0&!1] 9 {1 2 3 5}
State: 6
[0&!1] 3 {1 3}
[!0&1] 4
[0&1] 5
[!0&!1] 10
State: 7
[!0&1] 4
[0&!1] 7 {1 3}
[!0&!1] 11
[0&1] 12 {0 4}
State: 8
[!0&1] 4 {2 4 5}
[0&1] 5 {4}
[0&!1] 8 {1 3}
[!0&!1] 11 {1 3 5}
State: 9
[!0&1] 4 {2 4 5}
[0&!1] 5 {1 3}
[0&1] 5 {4}
[!0&!1] 11 {1 3 5}
State: 10
[!0&1] 4
[0&1] 8
[!0&!1] 10 {0 1 2 3 5}
[0&!1] 13 {1 2 3}
State: 11
[!0&1] 4 {2 4 5}
[0&!1] 8 {1 2 3}
[0&1] 8 {2 4}
[!0&!1] 11 {1 2 3 5}
State: 12
[!0&1] 4
[0&1] 7 {0 2 4}
[!0&!1] 9
[0&!1] 12 {1 3}
State: 13
[!0&1] 4
[0&1] 5
[!0&!1] 10 {0 1 3 5}
[0&!1] 13 {1 3}
--END--""")
scc_ = spot.scc_info(a)
spot.partial_degeneralize(scc_.split_on_sets(2, [])[0])


a = spot.automaton("""
HOA: v1
States: 2
Start: 0
AP: 2 "p0" "p1"
Acceptance: 5 (Inf(0)&Inf(1)) | ((Fin(2)|Fin(3)) & Fin(4))
--BODY--
State: 0
[!0 & 1] 0 {2 3}
[!0 & !1] 0 {3}
[0] 1
State: 1
[0&1] 1 {1 2 4}
[0&!1] 1 {4}
[!0&1] 1 {0 1 2 3}
[!0&!1] 1 {0 3}
--END--""")
p = spot.car(a, False, False, False, False, False, False, True)
assert spot.are_equivalent(a, p)
a = spot.automaton("""
HOA: v1 States: 6 Start: 0 AP: 2 "p0" "p1" Acceptance: 6 Inf(5) |
Fin(2) | Inf(1) | (Inf(0) & Fin(3)) | Inf(4) properties: trans-labels
explicit-labels trans-acc --BODY-- State: 0 [0&1] 2 {4 5} [0&1] 4 {0 4}
[!0&!1] 3 {3 5} State: 1 [0&!1] 3 {1 5} [!0&!1] 5 {0 1} State: 2 [!0&!1]
0 {0 3} [0&!1] 1 {0} State: 3 [!0&1] 4 {1 2 3} [0&1] 3 {3 4 5} State:
4 [!0&!1] 1 {2 4} State: 5 [!0&1] 4 --END--
""")
p = spot.car(a, False, False, False, False, False, False, True)
if (not p.num_states() == 8):
    print(p.num_states())
assert spot.are_equivalent(a, p)

for f in spot.randltl(15, 1000):
    print(f)
    d = spot.translate(f, "det", "G")
    p = spot.car(d, False, False, False, False, False, False, True)
    p2 = spot.car(d, False, False, False, False, False, False, True)
    if(not spot.are_equivalent(p, d)):
        print(f)
        assert(False)

for f in spot.randltl(5, 10000):
    print(f)
    n = spot.translate(f)
    p = spot.car(n, False, False, False, False, False, False, True)
    assert(spot.are_equivalent(n, p))

a = spot.translate('!(GFa -> (GFb & GF(!b & !Xb)))', 'gen', 'det')
b = spot.car(a, False, False, False, False, False, False, True)
assert spot.are_equivalent(a, b)
