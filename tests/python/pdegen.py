# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2019, 2020 Laboratoire de Recherche et Développement de
# l'Epita (LRDE).
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

# Test that the spot.gen package works, in particular, we want
# to make sure that the objects created from spot.gen methods
# are usable with methods from the spot package.


import spot

a, b, d = spot.automata("""
HOA: v1
States: 2
Start: 0
AP: 1 "p0"
Acceptance: 3 Inf(0)&Inf(1)|Fin(2)
--BODY--
State: 0
[0] 1 {1 2}
State: 1
[0] 0 {0}
--END--
HOA: v1
States: 3
Start: 2
AP: 1 "p0"
Acceptance: 3 Inf(0)&Inf(1)|Fin(2)
--BODY--
State: 0
[0] 1 {1 2}
State: 1
[0] 0 {0}
State: 2
[0] 0 {0}
[0] 0 {1}
--END--
HOA: v1
States: 1
Start: 0
AP: 1 "p0"
Acceptance: 1 Fin(0)
--BODY--
State: 0
[0] 0 {0}
--END--
""")

da = spot.partial_degeneralize(a, [0, 1])
assert da.equivalent_to(a)
assert da.num_states() == 2

db = spot.partial_degeneralize(b, [0, 1])
assert db.equivalent_to(b)
assert db.num_states() == 3

db.copy_state_names_from(b)
dbhoa = db.to_str('hoa')
assert dbhoa == """HOA: v1
States: 3
Start: 0
AP: 1 "p0"
acc-name: Streett 1
Acceptance: 2 Fin(0) | Inf(1)
properties: trans-labels explicit-labels state-acc deterministic
--BODY--
State: 0 "2#0"
[0] 1
State: 1 "0#0" {0 1}
[0] 2
State: 2 "1#0" {1}
[0] 1
--END--"""

c = spot.automaton("randaut -A'(Fin(0)&Inf(1)&Inf(2))|Fin(2)' 1 |")
dc = spot.partial_degeneralize(c, [1, 2])
assert dc.equivalent_to(c)
assert str(dc.get_acceptance()) == '(Fin(0) & Inf(2)) | Fin(1)'

dd = spot.partial_degeneralize(d, [])
assert dd.equivalent_to(d)
assert dd.num_states() == 1
assert str(dd.get_acceptance()) == 'Inf(1) & Fin(0)'

aut5 = spot.automaton(""" HOA: v1 States: 8 Start: 0 AP: 3 "p0" "p1" "p2"
acc-name: generalized-Buchi 10 Acceptance: 10
Inf(0)&Inf(1)&Inf(2)&Inf(3)&Inf(4)&Inf(5)&Inf(6)&Inf(7)&Inf(8)&Inf(9)
properties: trans-labels explicit-labels trans-acc deterministic --BODY--
State: 0 [0&!1&2] 3 {2 4 9} State: 1 [!0&1&2] 5 {2 6 7 8} [!0&!1&!2] 0 {2}
State: 2 [0&!1&2] 3 {1 4 9} State: 3 [0&!1&2] 4 {0 1 5 9} State: 4 [!0&!1&2] 1
{7} [0&!1&2] 6 {1 2} State: 5 [!0&1&2] 7 {3 5} State: 6 [!0&!1&2] 2 {1 3 5}
State: 7 [0&!1&!2] 0 {4 7} --END--""")

daut5 = spot.degeneralize_tba(aut5)
assert daut5.equivalent_to(aut5)
sets = list(range(aut5.num_sets()))
pdaut5 = spot.partial_degeneralize(aut5, sets)
assert pdaut5.equivalent_to(aut5)
assert daut5.num_states() == 10
assert pdaut5.num_states() == 8

aut6 = spot.automaton("""HOA: v1 States: 6 Start: 0 AP: 3 "p0" "p1" "p2"
acc-name: generalized-Buchi 3 Acceptance: 3 Inf(0)&Inf(1)&Inf(2) properties:
trans-labels explicit-labels trans-acc deterministic --BODY-- State: 0
[0&!1&!2] 4 {1} State: 1 [!0&!1&!2] 2 {0 1} State: 2 [!0&1&2] 0 {1} State: 3
[0&1&!2] 5 {1} State: 4 [!0&1&!2] 0 {1 2} [0&!1&!2] 3 {0} State: 5 [!0&1&2] 1
--END-- """)
daut6 = spot.degeneralize_tba(aut6)
assert daut6.equivalent_to(aut6)
sets = list(range(aut6.num_sets()))
pdaut6 = spot.partial_degeneralize(aut6, sets)
assert pdaut6.equivalent_to(aut6)
assert daut6.num_states() == 8
assert pdaut6.num_states() == 8


aut7 = spot.automaton("""HOA: v1 States: 8 Start: 0 AP: 3 "p0" "p1" "p2"
acc-name: generalized-Buchi 4 Acceptance: 4 Inf(0)&Inf(1)&Inf(2)&Inf(3)
properties: trans-labels explicit-labels trans-acc deterministic --BODY--
State: 0 [0&!1&2] 1 {2 3} State: 1 [0&!1&2] 0 {0 2} [0&!1&!2] 6 State: 2
[0&1&2] 0 [!0&!1&2] 5 [!0&1&!2] 6 {1} State: 3 [0&!1&2] 5 [0&!1&!2] 6 State: 4
[!0&!1&!2] 3 State: 5 [0&1&!2] 0 [!0&1&2] 7 State: 6 [0&1&2] 2 {1} State: 7
[!0&!1&2] 0 {0} [!0&1&!2] 4 --END--""")
daut7 = spot.degeneralize_tba(aut7)
assert daut7.equivalent_to(aut7)
sets = list(range(aut7.num_sets()))
pdaut7 = spot.partial_degeneralize(aut7, sets)
assert pdaut7.equivalent_to(aut7)
assert daut7.num_states() == 11
assert pdaut7.num_states() == 10

aut8 = spot.automaton("""HOA: v1 States: 8 Start: 0 AP: 3 "p0" "p1" "p2"
acc-name: generalized-Buchi 5 Acceptance: 5 Inf(0)&Inf(1)&Inf(2)&Inf(3)&Inf(4)
properties: trans-labels explicit-labels trans-acc deterministic --BODY--
State: 0 [!0&1&!2] 7 {0} State: 1 [!0&1&2] 1 {4} [0&!1&2] 6 {1 2} State: 2
[!0&!1&2] 3 {1 2} [0&1&2] 5 State: 3 [0&1&2] 2 {2} State: 4 [!0&!1&!2] 3 State:
5 [!0&1&!2] 0 {1 3} State: 6 [0&1&2] 4 [0&1&!2] 6 State: 7 [!0&!1&!2] 1
--END--""")
daut8 = spot.degeneralize_tba(aut8)
assert daut8.equivalent_to(aut8)
sets = list(range(aut8.num_sets()))
pdaut8 = spot.partial_degeneralize(aut8, sets)
assert pdaut8.equivalent_to(aut8)
assert daut8.num_states() == 22
assert pdaut8.num_states() == 9
