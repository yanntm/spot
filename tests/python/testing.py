# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2019 Laboratoire de Recherche et
# Développement de l'Epita (LRDE).
# Copyright (C) 2003, 2004 Laboratoire d'Informatique de Paris 6 (LIP6),
# département Systèmes Répartis Coopératifs (SRC), Université Pierre
# et Marie Curie.
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

def lt2ta(f):
    return spot.remove_stuttering_lasso(spot.statelabeldiff(spot.translate(f)))

#tests statelabeldiff
a1 = spot.translate('a U Gb')
ta1 = spot.statelabeldiff(a1)
assert spot.are_equivalent(ta1, a1)

#tests remove_diff
rm1 = spot.remove_testing(ta1)
assert spot.are_equivalent(ta1, rm1)

#tests remove_stuttering_lasso
f = spot.translate("GFa & GFb")
assert spot.are_equivalent(f, lt2ta("GFa & GFb"))

