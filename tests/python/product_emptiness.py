#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2017-2018 Laboratoire de Recherche et Développement de
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
err = 0

def finish():
    print("Computed {} emptiness checks and {} accepting runs".format(count_ec,
                                                                      count_ar))
    exit(err)

def error(context, res_otf, res_tae, a, b, ra, rb):
    global err
    print(context, file=sys.stderr)
    print("res_otf =", res_otf, file=sys.stderr)
    print("bool(res) =", bool(res_tae), file=sys.stderr)
    print("type(res) =", type(res_tae), file=sys.stderr)
    print("dir(res) =", dir(res), file=sys.stderr)
    if 'this' in dir(res):
        print("res.this =", res.this, file=sys.stderr)
    if ra is not None:
        print("\nra:\n" + str(ra), file=sys.stderr)
    if rb is not None:
        print("\nrb:\n" + str(rb), file=sys.stderr)
    print("\na:\n" + a.to_str(), file=sys.stderr)
    print("\nb:\n" + b.to_str(), file=sys.stderr)
    err = 2

end = time.time() + 10

while time.time() < end and err == 0:
    ra = None
    rb = None
    try:
        a = next(g)
        b = next(g)

        res = spot.product_emptiness_check(a, b)
        res_otf = spot.otf_product(a, b).is_empty()

        if res_otf == bool(res):
            error("spot.product_emptiness_check(a, b) != "
                  "spot.otf_product(a, b).is_empty()",
                  res_otf, res, a, b, None, None)

        if res is not None:
            ra, rb = res.accepting_runs()
            if ra.replay(spot.get_cout()) is False:
                error("Could not replay left accepting run",
                      res_otf, res, a, b, ra, rb)
            if rb.replay(spot.get_cout()) is False:
                error("Could not replay right accepting run",
                      res_otf, res, a, b, ra, rb)
            if err == 0:
                count_ar += 1

        if err == 0:
            count_ec += 1
    except Exception as e:
        error(str(e), res_otf, res, a, b, ra, rb)

finish()
