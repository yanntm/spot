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
import argparse
import datetime

parser = argparse.ArgumentParser()
parser.add_argument("repo_state")

parser.add_argument('--no-binary', dest='binary', action='store_false')
parser.add_argument('--binary', dest='binary', action='store_true', \
                    default=False)

parser.add_argument('--states', dest='nb_q', type=int, default=100)
parser.add_argument('--automata', dest='nb_auts', type=int, default=10)
parser.add_argument('--density', dest='nb_density', type=int, default=10)
parser.add_argument('--ap', dest='nb_ap', type=int, default=10)
parser.add_argument('--iterations', dest='iterations', type=int, default=10)

args = parser.parse_args()
repo_state = args.repo_state
binary = args.binary
nb_q = args.nb_q
nb_auts = args.nb_auts
nb_density = args.nb_density
nb_ap = args.nb_ap
iterations = args.iterations

f = open("result", "w+")

auts_aux = []
now = str(datetime.datetime.now())

def binary_product():
    aut = auts_aux[0]
    for i in range(len(auts_aux) - 1):
        aut = spot.product(aut, auts_aux[i + 1])

def product_n():
    spot.product_n(auts_aux)

def print_in_file(name, n, Q, density, ap, t):
    output = (now + ',' + name + "," + str(n) + "," + str(Q) + "," \
              + str(density) + "," + str(ap) + "," + str(t))
    f.write(output + "\n")
    print(output)


for Q in range(nb_q):
    for density in range(nb_density):
        for ap in range(nb_ap):
            auts = tuple(spot.automata("randaut -n" + str(nb_auts) + " Q" \
                         + str(Q) + " -e." + str(density) + " " \
                         + str(ap) + "|"))
            for n in range(2, nb_auts):
                auts_aux = auts[:n]
                if binary:
                    t = timeit.timeit(binary_product, number=iterations)
                    print_in_file("binary", n, Q, density, ap, t)
                t = timeit.timeit(product_n, number=iterations)
                print_in_file(repo_state, n, Q, density, ap, t)
