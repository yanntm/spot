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

for i in range(42):
    for nb_formula in range(1, 7):
        # Generate nb_formula LTL formulas
        for nb_ap in range(2,5):
            f = list(spot.randltl(nb_ap, nb_formula))

        # Convert them to automata
        auts = [g.translate() for g in f]

        # Compute the nth product
        prod = spot.product_n(auts)

        assert prod.is_empty() == spot.formula_And(f).translate().is_empty()

        # Compute the nth product of the the negated automata
        neg = spot.formula_Or([spot.formula_Not(g) for g in f]).translate()

        assert spot.product(prod, neg).is_empty()
