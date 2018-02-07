# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2018 Laboratoire de Recherche et Développement de l'Epita
# (LRDE).
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

def negate(a):
    return spot.dualize(spot.tgba_determinize(spot.to_generalized_buchi(a)))

def equivalent(a, b):
    return  not a.intersects(negate(b)) and not b.intersects(negate(a))

a = spot.automaton("""
HOA: v1
States: 3
Start: 0
AP: 1 "a"
acc-name: Buchi
Acceptance: 1 Inf(0)
properties: trans-labels explicit-labels state-acc complete
properties: deterministic
--BODY--
State: 0
[!0] 1
[0] 2
State: 1
[t] 0
State: 2 {0}
[t] 0
--END--
""")

det = spot.determinizer_build(a, False, True, True, True)
det.deactivate_all()
det.activate(0)
det.activate(1)
det.run()
assert equivalent(a, det.get())

det.deactivate_all()
det.activate(0)
det.activate(2)
det.run()
assert equivalent(a, det.get())

