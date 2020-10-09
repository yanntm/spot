#!/usr/bin/env python3
## -*- coding: utf-8 -*-
## Copyright (C) 2020 Laboratoire de Recherche et Développement de
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

import copy
import numpy as np
import PySimpleGUI as sg

def correlation(x, y):
    def average_index(arr, element):
        return np.mean(np.where(arr == element))
    sortedx = np.sort(x)
    sortedy = np.sort(y)
    sum_squares = 0
    for i in range(x.size):
        x_ = average_index(sortedx, x[i])
        y_ = average_index(sortedy, y[i])
        sum_squares += (x_ - y_) ** 2
    return 1 - (6 * sum_squares / (x.size ** 3 - x.size))

def correlation_matrix(features, names):
    table = np.zeros((len(features), len(features)))
    for x in range(len(features)):
        for y in range(len(features)):
            table[x][y] = abs(correlation(features[names[x]],
                              features[names[y]]))
    return table

def filtered_correlation_matrix(features, names, excluded, filter, value):
    f = copy.deepcopy(features)
    excludedvalues = features[filter]
    for e in excluded:
        f.pop(e)
    mask = (excludedvalues == value)
    for feature in f:
        f[feature] = f[feature][mask]
    return correlation_matrix(f, [x for x in names if x not in excluded]), f

