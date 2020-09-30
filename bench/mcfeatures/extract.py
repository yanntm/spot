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
import os
import psutil
import resource
import spot
import spot.ltsmin
import subprocess
import sys
import time

def get_options():
    parser = OptionParser(version='ltsmin-bench')

    parser.add_option('-v', '--verbose', action='store_true', dest='verbose',
                default=False)
    parser.add_option('-i', '--iterations', action='store', dest='iterations',
                default=100,
                help='maximum number of iterations per file')
    parser.add_option('-m', '--memory', action='store', dest='memory',
                default=1024, type='int',
                help='maximum memory allowed for a given run (in MB)')
    parser.add_option('-t', '--time', action='store', dest='time',
                default=20, type='int',
                help='maximum time allowed to test each formula')
    parser.add_option('-q', '--quick', action='store_true', dest='quick',
                default=False,
                help='stop search for file if satisfactory result is found')
    parser.add_option('-o', '--out', action='store', dest='out', default=None,
                help='output file name (defaults to stdout)')
    parser.add_option('-e', '--empty', action='store_true', dest='empty',
                default=False,
                help='force extraction of formulas that have empty result')
    parser.add_option('-n', '--non-empty', action='store_true',
                dest='non_empty', default=False,
                help='force extraction of formulas that have non-empty result')
    parser.add_option('-s', '--start-at', action='store', dest='start',
                default=None,
                help='start execution at specified file')
    parser.add_option('--spot-folder', action='store', dest='spot',
                default='../..',
                help='path to spot root folder')
    parser.add_option('--spot-exe', action='store', dest='spot_exe',
                default=None,
                help='path to spot executable (overrides --spot-folder)')
    parser.add_option('--randltl-exe', action='store', dest='randltl_exe',
                default=None,
                help='path to randltl executable (overrides --spot-folder)')
    parser.add_option('--model-folder', action='store', dest='models',
                default='../../../models/dve/noprop',
                help='path to models folder')
    return parser.parse_args()

class Log:
    def __init__(self, file_name, formula='no formula', exe_time=0, memory=0,
                 emptiness='', states='', transitions=''):
        self.file_name = file_name
        self.formula = formula
        self.exe_time = exe_time
        self.memory = memory
        self.emptiness = emptiness
        self.states = states
        self.transitions = transitions

    def write(self, out):
        print('%s, %s, %f, %d, %s, %s, %s' % (self.file_name, self.formula,
              round(self.exe_time, 3), self.memory / (1024 * 1024),
              self.emptiness, self.states, self.transitions),
              file=out)
        out.flush()

def run_subprocess(file_name, formula):
    start_time = time.time()
    p = subprocess.Popen([options.spot_exe, '-m',
                   file_name, '-f',
                   formula, '--is-empty', '--csv'], stdout=subprocess.PIPE)

    total_time = 0
    mem = 0
    max_mem = 0
    while p.poll() == None:
        total_time = time.time() - start_time
        if total_time > options.time:
            if options.verbose:
                print(formula, 'exceeded max time.', file=sys.stderr)
            p.terminate()
            break
        mem = psutil.Process(p.pid).memory_info()[0]
        if mem > options.memory * 1024 * 1024:
            if options.verbose:
                print(formula, 'exceeded max memory.', file=sys.stderr)
            p.kill()
            break
        if mem > max_mem:
            max_mem = mem
    else:
        try:
            p.kill()
        except:
            pass
        try:
            process_log = p.communicate()[0].splitlines()[-1].decode().split(',')
            return Log(file_name, formula, total_time, max_mem,
                       process_log[-3], process_log[-2], process_log[-1])
        except:
            return Log(file_name, 'error')
    return Log(file_name, 'error')

def get_variables(path):
    m = spot.ltsmin.load(path)
    variables = []
    for i in range(m.state_size()):
        variables.append(m.state_variable_name(i))
    return ' '.join(variables)

def bench_file(file_name : str):
    def test_new_log(old_log, new_log):
        stop = False
        if options.quick:
            if info.exe_time > (options.time / 2):
                stop = True
        if options.empty and new_log.emptiness == 'EMPTY':
            if options.non_empty:
                if info.exe_time > best_log[0].exe_time:
                    return (info, best_log[1]), stop
            elif info.exe_time > best_log.exe_time:
                return info, stop
        if options.non_empty and new_log.emptiness == 'NONEMPTY':
            if options.empty:
                if info.exe_time > best_log[1].exe_time:
                    return (best_log[0], info), stop
            elif info.exe_time > best_log.exe_time:
                return info, stop
        if not options.empty and not options.non_empty:
            if info.exe_time > best_log.exe_time:
                return info, stop
        return best_log, False

    if options.verbose:
        print('|-- %s --|' % file_name)

    if os.path.splitext(file_name)[1] == '.prom':
        try:
            p = subprocess.Popen(['spins', '%s/%s' % (options.models, file_name)],
                                 stdout=subprocess.DEVNULL)
            p.wait()
        except:
            print('Unable to compile ' + file_name, file=sys.stderr)
            return Log(file_name, 'error')

        file_name = file_name + '.spins'
    else:
        file_name = options.models + '/' + file_name
    states = get_variables(file_name)

    stream = os.popen('%s -n %s %s' % (options.randltl_exe, options.iterations,
                      states))
    formulas = stream.readlines()

    if options.empty and options.non_empty:
        best_log = (Log(file_name, 'no formula'), Log(file_name, 'no formula'))
    else:
        best_log = Log(file_name, 'no formula')
    for formula in formulas:
        formula = formula[:-1]
        info = run_subprocess(file_name, formula)
        best_log, stop_execution = test_new_log(best_log, info)
        if stop_execution:
            return best_log
    return best_log

def manage_options():
    options, args = get_options()
    if options.spot_exe is None:
        options.spot_exe = options.spot + '/tests/ltsmin/modelcheck'
    if options.randltl_exe is None:
        options.randltl_exe = options.spot + '/bin/randltl'
    if options.out:
        options.out = open(options.out, 'w')
    else:
        options.out = sys.stdout
    return options

if __name__ == '__main__':
    global options
    options = manage_options()

    files = sorted(os.listdir(options.models))
    if options.start:
        files = files[files.index(options.start):]
    for file_name in files:
        if not os.path.isdir(options.models + '/' + file_name):
            info = bench_file(file_name)
            if options.empty and options.non_empty:
                info[0].write(options.out)
                info[1].write(options.out)
            else:
                info.write(options.out)
    options.out.close()
