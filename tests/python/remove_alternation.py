# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2017  Laboratoire de Recherche et Développement
# de l'Epita
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

bdddict = spot.make_bdd_dict()

def equivalent(a, b):
    nega = spot.dualize(spot.tgba_determinize(a));
    negb = spot.dualize(spot.tgba_determinize(b));
    return not (a.intersects(negb) or b.intersects(nega))

a1 = spot.automaton("""
HOA: v1
States: 4
Start: 0
AP: 1 "a"
Acceptance: 1 Inf(0)
--BODY--
State: 0
[t] 1&2
State: 1
[t] 3 {0}
State: 2
[t] 3
State: 3
[t] 0
--END--
""")

ex1 = spot.automaton("""
HOA: v1
States: 1
Start: 0
AP: 1 "a"
Acceptance: 1 Inf(0)
--BODY--
State: 0
--END--
""")

assert equivalent(spot.remove_alternation_buchi(a1), ex1)

a2 = spot.automaton("""
HOA: v1
States: 4
Start: 0
AP: 1 "a"
Acceptance: 1 Inf(0)
--BODY--
State: 0
[0] 0&2 {0}
[!0] 0&1 {0}
State: 1
[0] 3
State: 2
[!0] 3
State: 3
[t] 3 {0}
--END--
""")

ex2 = spot.automaton("""
HOA: v1
States: 3
Start: 0
AP: 1 "a"
Acceptance: 1 Inf(0)
--BODY--
State: 0
[0] 1
[!0] 2
State: 1
[!0] 2 {0}
State: 2
[0] 1 {0}
--END--
""")

assert(equivalent(spot.remove_alternation_buchi(a2), ex2))

