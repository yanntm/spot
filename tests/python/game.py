#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2020 Laboratoire de Recherche et Développement de
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
import buddy

def compare_strings(s_sol, s_ref):
    res = s_sol == s_ref
    if not res:
        print("Expected:\n", s_ref,
              "\nObtained:\n", s_sol)
        print("Diff starts at\n")
        for i in range(min(len(s_sol), len(s_ref))):
            if (s_sol[i] != s_ref[i]):
                print("Expected:\n", s_ref[i:],
                      "\nObtained:\n", s_sol[i:])
                break
    return res

def compare_strategies(s_sol, s_ref, kind=None, style=None):
    s_sol_split = s_sol.split("\n")
    s_ref_split = s_ref.split("\n")
    sol_strat = None
    for sol_strat in s_sol_split:
        if sol_strat.startswith("spot.highlight.edges"):
            break
    ref_strat = None
    for ref_strat in s_ref_split:
        if ref_strat.startswith("spot.highlight.edges"):
            break

    res = compare_strings(sol_strat, ref_strat)
    if not res:
        print("kind: ", kind, " style: ", style)
    return res






g = spot.automaton("""HOA: v1 States: 9 Start: 0 AP: 2 "a" "b"
acc-name: Streett 1 Acceptance: 2 Fin(0) | Inf(1) properties:
trans-labels explicit-labels state-acc spot-state-player: 0 1 0 1 0 1
0 1 1 --BODY-- State: 0 {1} [1] 1 [1] 3 State: 1 {1} [1] 2 State: 2
{1} [0] 8 State: 3 {1} [1] 4 State: 4 {1} [0] 5 State: 5 {1} [0] 6
State: 6 {1} [1] 7 State: 7 State: 8 {1} [0] 2 --END--""")

assert spot.solve_parity_game(g) == False

assert compare_strings(spot.highlight_strategy(g).to_str("HOA", "1.1"),
"""HOA: v1.1
States: 9
Start: 0
AP: 2 "a" "b"
acc-name: Streett 1
Acceptance: 2 Fin(0) | Inf(1)
properties: trans-labels explicit-labels state-acc !complete
properties: !deterministic exist-branch
spot.highlight.states: 0 5 1 4 2 4 3 5 4 5 5 5 6 5 7 5 8 4
spot.highlight.edges: 2 5 3 4 6 5 8 5 9 4
spot.state-player: 0 1 0 1 0 1 0 1 1
--BODY--
State: 0 {1}
[1] 1
[1] 3
State: 1 {1}
[1] 2
State: 2 {1}
[0] 8
State: 3 {1}
[1] 4
State: 4 {1}
[0] 5
State: 5 {1}
[0] 6
State: 6 {1}
[1] 7
State: 7
State: 8 {1}
[0] 2
--END--"""
)

g = spot.translate("GF(a&X(a)) -> GFb")
a = buddy.bdd_ithvar(g.register_ap("a"))
b = buddy.bdd_ithvar(g.register_ap("b"))
gdpa = spot.tgba_determinize(spot.degeneralize_tba(g),
                             False, True, True, False)
spot.change_parity_here(gdpa, spot.parity_kind_max, spot.parity_style_odd)
gsdpa = spot.split_2step(gdpa, a, b, True, True)
spot.colorize_parity_here(gsdpa, True)
assert spot.solve_parity_game(gsdpa)
assert compare_strings(spot.highlight_strategy(gsdpa).to_str("HOA", "1.1"),
"""HOA: v1.1
States: 18
Start: 0
AP: 2 "a" "b"
acc-name: parity max odd 7
Acceptance: 7 Fin(6) & (Inf(5) | (Fin(4) & (Inf(3) """
+"""| (Fin(2) & (Inf(1) | Fin(0))))))
properties: trans-labels explicit-labels trans-acc colored complete
properties: deterministic
spot.highlight.states: 0 4 1 4 2 4 3 4 4 4 5 4 6 4 7 4 8 4 9 4 """
+"""10 4 11 4 12 4 13 4 14 4 15 4 16 4 17 4
spot.highlight.edges: 15 4 17 4 20 4 22 4 24 4 26 4 28 4 30 4 31 4 32 4 33 4
spot.state-player: 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 1 1 1
--BODY--
State: 0
[0] 7 {1}
[!0] 8 {1}
State: 1
[0] 9 {1}
[!0] 10 {1}
State: 2
[0] 11 {1}
[!0] 12 {1}
State: 3
[!0] 10 {1}
[0] 13 {1}
State: 4
[!0] 12 {1}
[0] 14 {1}
State: 5
[0] 15 {1}
[!0] 16 {1}
State: 6
[!0] 16 {1}
[0] 17 {1}
State: 7
[!1] 3 {2}
[1] 4 {2}
State: 8
[!1] 1 {2}
[1] 2 {2}
State: 9
[!1] 3 {5}
[1] 6 {5}
State: 10
[!1] 1 {5}
[1] 5 {5}
State: 11
[!1] 4 {3}
[1] 4 {5}
State: 12
[!1] 2 {3}
[1] 2 {5}
State: 13
[!1] 3 {6}
[1] 4 {6}
State: 14
[!1] 4 {4}
[1] 4 {5}
State: 15
[t] 6 {5}
State: 16
[t] 5 {5}
State: 17
[t] 4 {6}
--END--"""
)

# Test the different parity conditions
gdpa = spot.tgba_determinize(spot.degeneralize_tba(g),
                             False, True, True, False)

g_test = spot.change_parity(gdpa, spot.parity_kind_max, spot.parity_style_odd)
g_test_split = spot.split_2step(g_test, a, b, True, True)
assert spot.solve_parity_game(g_test_split)
# ref_sol = spot.highlight_strategy(g_test_split).to_str("HOA", "1.1")
# They should all give the same strategy
for kind in [spot.parity_kind_min, spot.parity_kind_max]:
    for style in [spot.parity_style_even, spot.parity_style_odd]:
        g_test = spot.change_parity(gdpa, kind, style)
        g_test_split = spot.split_2step(g_test, a, b, True, True)
        assert spot.solve_parity_game(g_test_split)
        # assert compare_strategies(
        #     spot.highlight_strategy(g_test_split).to_str("HOA", "1.1"),
        #     ref_sol, kind, style)
        g_test_split = spot.split_2step(g_test, a, b, True, True)
        spot.colorize_parity_here(g_test_split, True)
        assert spot.solve_parity_game(g_test_split)
        # assert compare_strategies(
        #     spot.highlight_strategy(g_test_split).to_str("HOA", "1.1"),
        #     ref_sol, kind, style)

