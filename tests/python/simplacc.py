# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2020 Laboratoire de Recherche et Développement de l'Epita
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


auts = list(spot.automata("""
HOA: v1 States: 2 Start: 0 AP: 2 "a" "b" Acceptance: 2 Inf(0) | Inf(1) --BODY--
State: 0 [1] 0 [0] 1 {0} State: 1 [0] 1 [1] 0 {1} --END--
HOA: v1 States: 2 Start: 0 AP: 2 "a" "b" Acceptance: 2 (Inf(0) | Inf(1)) &
Fin(0) --BODY-- State: 0 [1] 0 [0] 1 {0} State: 1 [0] 1 [1] 0 {1} --END--
HOA: v1 States: 2 Start: 0 AP: 2 "a" "b" Acceptance: 3 (Inf(0) | Inf(1)) &
(Fin(0) | Inf(2)) --BODY-- State: 0 [1] 0 [0] 1 {0} State: 1 [0] 1 {2} [1] 0
{1} --END--
HOA: v1 States: 2 Start: 0 AP: 2 "a" "b" Acceptance: 4 (Inf(0) |
(Inf(1)&(Inf(3)|Fin(2)))) --BODY-- State: 0 [1] 0 {3} [0] 1 {0} State: 1 [0] 1
{2} [1] 0 {1} --END--
/* The next automaton was incorrectly simplified. */
HOA: v1 States: 4 Start: 0 AP: 2 "p0" "p1" Acceptance: 6 ((Fin(1) | Inf(2)) &
Inf(5)) | (Fin(0) & (Fin(1) | (Fin(3) & Inf(4)))) properties: trans-labels
explicit-labels trans-acc complete properties: deterministic --BODY-- State: 0
[!0&!1] 0 {2} [0&1] 1 {0 5} [0&!1] 1 {0 2 5} [!0&1] 2 {1} State: 1 [0&1] 1 {0}
[!0&!1] 1 {2} [0&!1] 1 {0 2} [!0&1] 2 {1} State: 2 [!0&!1] 0 {2 3} [0&!1] 0 {0
2 3 5} [!0&1] 2 {1 4} [0&1] 3 {0 5} State: 3 [!0&!1] 0 {2 3} [0&!1] 0 {0 2 3 5}
[!0&1] 2 {1 4} [0&1] 3 {0} --END--"""))

res = []
for a in auts:
    b = spot.simplify_acceptance(a)
    assert b.equivalent_to(a)
    res.append(str(b.get_acceptance()))

    a.set_acceptance(a.num_sets(), a.get_acceptance().complement())
    b = spot.simplify_acceptance(a)
    assert b.equivalent_to(a)
    res.append(str(b.get_acceptance()))

assert res == [
   'Inf(0)',
   'Fin(0)',
   'Inf(1) & Fin(0)',
   'Fin(1) | Inf(0)',
   'Inf(1) & (Fin(0) | Inf(2))',
   'Fin(1) | (Fin(2) & Inf(0))',
   '(Fin(1) | Inf(2)) & Inf(0)',
   'Fin(0) | (Fin(2) & Inf(1))',
   '((Fin(1) | Inf(2)) & Inf(5)) | (Fin(0) & (Fin(1) | (Fin(3) & Inf(4))))',
   '(Fin(5) | (Fin(2) & Inf(1))) & (Inf(0) | ((Fin(4) | Inf(3)) & Inf(1)))',
   ]
