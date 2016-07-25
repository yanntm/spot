#!/usr/bin/env python3
## -*- coding: utf-8 -*-
## Copyright (C) 2016 Laboratoire de Recherche et Développement de
## l'Epita (LRDE).
##
## This file is part of Spot, a model checking library.
##
## Spot is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or
## (at your option) any later version.
##
## Spot is distributed in the hope that it will be useful, but WITHOUT
## ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
## or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
## License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.

import spot
import timeit

f_result = open("result", "w+")
f_input = open("input", "w")

first = spot.translate("Gba")
second = spot.translate("Gba")
q = 0
e = 0

def gen_auts():
    res = []
    for avg_e in range(1, 16, 4):
        for n_letters in range(1, 5):
            new_auts = tuple(spot.automata("randaut -n3 -Q15 -E" \
                             + str(avg_e) + " -D " + str(n_letters) + "|"))
            for aut in new_auts:
                res.append((aut, aut.num_edges(), n_letters))
    return res

left_auts = gen_auts()
right_auts = gen_auts()

def regular_product():
    global q
    global e
    global first
    global second
    aut = spot.product(first, second)
    q = aut.num_states()
    e = aut.num_edges()

def parity_product():
    global q
    global e
    global first
    global second
    aut = spot.parity_product(first, second)
    q = aut.num_states()
    e = aut.num_edges()

def print_input_in_file(n, n_letters, e_left, e_right, Q, E, time):
    output = str(n) + ',' + str(n_letters) + ',' + str(e_left) + ',' \
             + str(e_right) + ',' + str(Q) + ',' + str(E) + ',' + str(time) \
             + '\n'
    f_input.write(output)

def print_result_in_file(n, left_acc, right_acc, q, e, time):
    output = str(n) + ',' + str(left_acc) + ',' + str(right_acc) + ',' \
             + str(q) + ',' + str(e) + ',' + str(time) + '\n'
    f_result.write(output)

n = 0
for t_left in left_auts:
    for t_right in right_auts:
        if t_left[2] == t_right[2]:
            first = t_left[0]
            second = t_right[0]
            t_regular = timeit.timeit(regular_product, number=1)
            print_input_in_file(n, t_left[2], t_left[1], t_right[1], q, e, t_regular)
            for left_acc in range(1, 5):
                for right_acc in range(1, 5):
                    left = spot.rand_parity(left, left_acc)
                    right = spot.rand_parity(right, right_acc)
                    t_parity = timeit.timeit(parity_product, number=1)
                    print_result_in_file(n, left_acc, right_acc, q, e, t_paruty)
            n += 1
