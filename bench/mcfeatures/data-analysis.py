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

import csv
import numpy as np
import sys
import os
from matplotlib import pyplot as plt

from plot import *
from correlations import *
from gui import *

def read_csv():
    intfeatures = ['bloemen_time', 'cndfs_time',\
                   'memory', 'states', 'transitions', 'incidence_in',\
                   'incidence_out', 'repeated_transitions', 'terminal', 'weak',\
                   'inherently_weak', 'very_weak', 'complete', 'universal',\
                   'unambiguous', 'semi_deterministic', 'stutter_invariant',\
                   'emptiness']
    floatfeatures = ['average_incidence_ratio']
    nb_int = len(intfeatures)
    nb_float = len(floatfeatures)
    nb_features = nb_int + nb_float
    features = []
    for _ in range(nb_features):
        features.append([])
    with open(sys.argv[1], newline='') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            for i in range(nb_int):
                features[i].append(int(row[intfeatures[i]]))
            for i in range(nb_float):
                features[i + nb_int].append(float(row[floatfeatures[i]]))
    npfeatures = []
    for f in features:
        npfeatures.append(np.asarray(f))
    features = npfeatures
    time = features[1] - features[0]
    features = np.concatenate(([time], features))
    names = (['time difference'] + intfeatures + floatfeatures)
    for i in range(len(names)):
        names[i] = names[i].replace('_', ' ')
    return features, names

def separate_by_value(features, position):
    # segfaults
    set_ = list(set(features[position]))
    out = [[] * (len(features) - 1)] * len(set_)

    for i in range(len(features)):
        if i == position:
            continue
        for j in range (len(features[0])):
            out[set_.index(features[position][j])][i].append(features[i][j])
    return out

def generate_time_scatter_plot(features):
    bloemen_time = features[1]
    cndfs_time = features[2]
    scatter_plot(bloemen_time, cndfs_time, 'time difference',
                 'bloemen time', 'cndfs time', True)
    plt.savefig('time_difference.png')
    plt.clf()

def pretty_print_table(table, names):
    name_max_len = len(max(names, key=len))
    for i in range(len(table)):
        row = table[i]
        print(names[i].rjust(name_max_len, ' '), end=' |')
        for j in range(i + 1):
            elt = row[j]
            print(str(round(elt, 2)).ljust(4, '0'), end='|')
        print()
    for i in range(name_max_len):
        print(' ' * (name_max_len + 1), end='')
        for name in names:
            if i < len(name):
                print('  %s  ' % name[i], end='')
            else:
                print('     ', end='')
        print()
    print()

if __name__ == '__main__':
    features, names = read_csv()
    generate_time_scatter_plot(features)
    for i in range(len(names)):
        for j in range(i + 1):
            filename = 'scps/scp-%s-%s.png' % (names[i], names[j])
            if os.path.isfile(filename):
                continue
            scatter_plot(features[j], features[i],
                         'scatter plot of %s over %s' % (names[j], names[i]),
                         names[j], names[i])
            plt.savefig(filename)
            plt.clf()
    correlation = correlation_matrix(features)
    gui_display_table(correlation, names)
