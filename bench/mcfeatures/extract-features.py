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
import os
import subprocess
import sys

if len(sys.argv) != 3:
    sys.exit(1)
infile = sys.argv[1]
outfile = sys.argv[2]

with open(outfile, 'w+') as f:
    f.write('model,formula,time,memory,emptiness,states,transitions,'
            + 'bloemen_time,cndfs_time,'
            + 'incidence_in,incidence_out,average_incidence_ratio,'
            + 'repeated_transitions,terminal,weak,inherently_weak,'
            + 'very_weak,complete,universal,unambiguous,'
            + 'semi_deterministic,stutter_invariant,sccs\n')

with open(infile, newline='') as csvfile:
    reader = csv.reader(csvfile)
    for row in reader:
        model = row[0]
        formula = row[1]
        row[0] = os.path.basename(model)
        print(row[0])

        csvline = ','.join(row)

        # bloemen time
        p = subprocess.Popen(['../../tests/ltsmin/modelcheck', '-B', '-t',
                              '-m', model, '-f', formula],
                              stdout=subprocess.PIPE)
        p.wait()
        output = p.stdout.readlines()
        del p
        for line in output:
            line = line.decode()
            if 'TOTAL' in line:
                csvline += ',' + line.split()[-3]

        # cndfs time
        p = subprocess.Popen(['../../tests/ltsmin/modelcheck', '-C', '-t',
                              '-m', model, '-f', formula],
                              stdout=subprocess.PIPE)
        p.wait()
        output = p.stdout.readlines()
        del p
        for line in output:
            line = line.decode()
            if 'TOTAL' in line:
                csvline += ',' + line.split()[-3]


        # other features
        p = subprocess.Popen(['../../tests/ltsmin/modelcheck', '-x',
                              '-m', model, '-f', formula],
                              stdout=subprocess.PIPE)
        p.wait()
        output = p.stdout.readlines()
        del p
        for i in range(len(output)):
            line = str(output[i])
            if 'incidence' in line:
                csvline = csvline + ',' + output[i + 1].decode()[:-1]
            if 'terminal' in line:
                csvline = csvline + ',' + output[i + 1].decode()[:-1]
            if 'sccs' in line:
                csvline = csvline + ',' + output[i + 1].decode().split(',')[-3]
                break

        csvline = csvline.replace('yes', '1').replace('no','-1')\
                         .replace('maybe', '0').replace('EMPTY', '1')\
                         .replace('NONEMPT', '0')
        with open(outfile, 'a+') as f:
            f.write(csvline + '\n')
