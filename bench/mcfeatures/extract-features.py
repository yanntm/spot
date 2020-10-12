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

from optparse import OptionParser
import csv
import os
import subprocess
import sys

def get_options():
    parser = OptionParser(usage="usage: %prog [options] filename",
                          version='%prog')

    parser.add_option('-v', '--verbose', action='store_true')
    parser.add_option('-t', '--time-only', action='store_true', dest='timeonly')
    parser.add_option('-p', '--threads', action='append')
    parser.add_option('-o', '--out', action='store', dest='fileout',
                      default=None)
    parser.add_option('--exe', action='store',
                      default='../../tests/ltsmin/modelcheck')
    options, args = parser.parse_args()
    if len(args) != 1:
        parser.print_help()
        sys.exit(1)
    return options, args[0]

def get_fields(options):
    fields = ['model', 'formula', 'time', 'memory', 'emptiness', 'states',
              'transitions']
    if not options.timeonly:
        fields += ['sccs', 'repeated_transitions']
        fields += ['incidence_in', 'incidence_out', 'average_incidence_ratio',
                   'repeated_transitions', 'terminal', 'weak',
                   'inherently_weak', 'very_weak', 'complete', 'universal',
                   'unambiguous', 'semi_deterministic', 'stutter_invariant']
    if not options.threads:
        fields += ['bloemen_time', 'cndfs_time']
    else:
        for nb_threads in options.threads:
            fields += ['bloemen_time_' + nb_threads, 'cndfs_time_' + nb_threads]
    return fields


def extract(options, filein, fields):
    def get_time(option, threads, row):
        p = subprocess.Popen([options.exe, option, '-t',
                              '-m', row['model'], '-f', row['formula'],
                              '-p', str(threads)],
                              stdout=subprocess.PIPE)
        p.wait()
        output = p.stdout.readlines()
        del p
        for line in output:
            line = line.decode()
            if 'TOTAL' in line:
                return line.split()[-3]

    with open(filein, newline='') as csvin:
        with open(options.fileout, 'w', newline='') as csvout:
            reader = csv.reader(csvin)
            writer = csv.DictWriter(csvout, fieldnames=fields)
            writer.writeheader()

            for row in reader:
                print(os.path.basename(row[0]))
                rowdict = {}
                for i in range(7):
                    rowdict[fields[i]] = row[i]

                if not options.threads:
                    rowdict['bloemen_time'] = get_time('-B', 1, rowdict)
                    rowdict['cndfs_time'] = get_time('-C', 1, rowdict)
                else:
                    for nbt in options.threads:
                        rowdict['bloemen_time_' + nbt] =\
                                                get_time('-B', nbt, rowdict)
                        rowdict['cndfs_time_' + nbt] =\
                                                get_time('-C', nbt, rowdict)

                if not options.timeonly:
                    p = subprocess.Popen([options.exe, '-x', '-t',
                                          '-m', rowdict['model'],
                                          '-f', rowdict['formula']],
                                          stdout=subprocess.PIPE)
                    p.wait()
                    output = p.stdout.readlines()
                    del p
                    for i in range(len(output)):
                        line = str(output[i])
                        if 'incidence' in line:
                            data = output[i + 1].decode()[:-1].split(',')

                            rowdict['incidence_in'] = data[0]
                            rowdict['incidence_out'] = data[1]
                            rowdict['average_incidence_ratio'] = data[2]
                            rowdict['repeated_transitions'] = data[3]

                        if 'terminal' in line:
                            data = output[i + 1].decode()[:-1].split(',')

                            rowdict['terminal'] = data[0]
                            rowdict['weak'] = data[1]
                            rowdict['inherently_weak'] = data[2]
                            rowdict['very_weak'] = data[3]
                            rowdict['complete'] = data[4]
                            rowdict['universal'] = data[5]
                            rowdict['unambiguous'] = data[6]
                            rowdict['semi_deterministic'] = data[7]
                            rowdict['stutter_invariant'] = data[8]

                        if 'sccs' in line:
                            rowdict['sccs'] = output[i + 1].decode()\
                                                           .split(',')[-3]
                            break
                writer.writerow(rowdict)
                csvout.flush()

if __name__ == '__main__':
    options, filein = get_options()
    if options.fileout is None:
        basename = os.path.basename(filein)
        options.fileout = 'features_' + basename
    fieldnames = get_fields(options)
    extract(options, filein, fieldnames)
