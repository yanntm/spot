#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2017 Laboratoire de Recherche et Développement de
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

import time
import sys

import spot

g = spot.automata('randaut -A "Inf(0)&Inf(1)|Inf(2)&Inf(3)" -Q5 -n -1'
                  ' --check=strength 2|')

count_ec = 0
count_ar = 0

end = time.time() + 10

while time.time() < end:
    a = next(g)
    b = next(g)
    ra = spot.twa_run(a)
    rb = spot.twa_run(b)

    res = spot.two_aut_ec(a, b, ra, rb)

    if spot.otf_product(a, b).is_empty() != res:
        print("spot.two_aut_ec(a, b) != spot.otf_product(a, b).is_empty()",
              file=sys.stderr)
        print("\na:\n" + a.to_str(), file=sys.stderr)
        print("\nb:\n" + b.to_str(), file=sys.stderr)
        exit(2)

    count_ec += 1

    if res == False:
        if ra.replay(spot.get_cout()) == False:
            print("Could not replay left accepting run", file=sys.stderr)
            print("\na:\n" + a.to_str(), file=sys.stderr)
            print("\nb:\n" + b.to_str(), file=sys.stderr)
            exit(2)
        if rb.replay(spot.get_cout()) == False:
            print("Could not replay right accepting run", file=sys.stderr)
            print("\na:\n" + a.to_str(), file=sys.stderr)
            print("\nb:\n" + b.to_str(), file=sys.stderr)
            exit(2)
        count_ar += 1

print("Computed {} emptiness checks and {} accepting runs".format(count_ec,
                                                                  count_ar))
