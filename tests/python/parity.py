# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2016 Laboratoire de Recherche et Développement
# de l'EPITA.
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

def is_included(left, right):
    complement = spot.dtwa_complement(right)
    return spot.product(left, complement).is_empty()

def are_equiv(left, right):
    return is_included(left, right) and is_included(right, left)

maxmin = [ "max", "min" ]
oddeven = [ "odd", "even" ]
parity_acc_auts = ()
for parity_range in range(5):
    for Q in range(5):
        for AP in range(2, 5):
            for is_max in maxmin:
                for is_odd in oddeven:
                    tmp = tuple(spot.automata("randaut -D -Q" + str(Q) + \
                                                "-A \"parity " + is_max + " " +\
                                                is_even + "-n10 " + \
                                                str(parity_range) + "\"" + \
                                                str(AP) + "|"))
                    for aut in tmp[:5]:
                        for edge in aut:
                            randval = random.randint(0, 8)
                            if randval == 0:
                                #add edge acc
                            elif randval == 1:
                                #remove edge acc
                            elif randval == 2:
                                #add edge acc 0
                            elif randval == 3:
                                #add edge last acc
                    parity_acc_auts += aut


# Check change_parity_acc
truefalse = [ True, False ]
for i, aut in enumerate(parity_acc_auts):
    for oddtf in truefalse:
        for maxtf in truefalse:
            new_aut = spot.change_parity_acc(aut, oddtf, maxtf)
            assert are_equiv(aut, new_aut) and \
                   new_aut.is_parity() = [ True , oddtf, maxtf ], \
                   "change_parity_acc: " + str(i) + "nth failed"
