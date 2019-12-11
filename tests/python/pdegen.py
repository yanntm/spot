# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2019 Laboratoire de Recherche et Développement de
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
[0] 2
State: 1 "1#0"
[0] 2
State: 2 "0#1" {0 1}
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
