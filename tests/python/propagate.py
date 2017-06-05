# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2017  Laboratoire de Recherche et Développement de l'Epita
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
from sys import exit

aut = spot.automaton("""
HOA: v1
States: 4
Start: 0
AP: 1 "a"
Acceptance: 2 Inf(0)&Inf(1)
--BODY--
State: 0
[0] 1 {0}
State: 1
[0] 1 {0}
[0] 2
State: 2
[0] 3 {1}
State: 3
[0] 0
[0] 3 {1}
--END--
""")
aut2 = spot.propagate_acc(aut)
assert aut2.to_str() == """HOA: v1
States: 4
Start: 0
AP: 1 "a"
acc-name: generalized-Buchi 2
Acceptance: 2 Inf(0)&Inf(1)
properties: trans-labels explicit-labels trans-acc
--BODY--
State: 0
[0] 1 {0 1}
State: 1
[0] 1 {0}
[0] 2 {0 1}
State: 2
[0] 3 {0 1}
State: 3
[0] 0 {0 1}
[0] 3 {1}
--END--"""

aut = spot.automaton("""
HOA: v1
States: 4
Start: 0
AP: 1 "a"
Acceptance: 2 Inf(0)&Inf(1)
--BODY--
State: 0
[0] 1 {0}
State: 1
[0] 1 {0}
[0] 2
State: 2
[0] 3 {1}
State: 3
[0] 3 {1}
--END--
""")
aut2 = spot.propagate_acc(aut)
assert aut2.to_str() == """HOA: v1
States: 4
Start: 0
AP: 1 "a"
acc-name: generalized-Buchi 2
Acceptance: 2 Inf(0)&Inf(1)
properties: trans-labels explicit-labels trans-acc
--BODY--
State: 0
[0] 1 {0}
State: 1
[0] 1 {0}
[0] 2
State: 2
[0] 3 {1}
State: 3
[0] 3 {1}
--END--"""
