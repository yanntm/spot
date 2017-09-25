#!/usr/bin/env python3
# Copyright (C) 2017 Laboratoire de Recherche et Développement de
# l'Epita (LRDE).
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

import csv
import os
import re
import spot
import subprocess
import multiprocessing

def run(cmd):
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE,
                                  universal_newlines=True,
                                  shell=True)
    except Exception as e:
        print(str(e))
        print(cmd)
        return [''] * 8
    tmp = [x.strip() for x in res.stdout.split(',')]
    return tmp

def get_mem(line):
    try:
        val = int(re.search('=(\d+)', line).group(1))
    except AttributeError:
        val = 0
    return val

def add_line(csv_path, row):
    with open(csv_path, 'a') as f:
        writer = csv.writer(f, delimiter=',')
        writer.writerow(row)
        f.flush()

def run_ltl(ltl, folder, csv_path):
    # Getting common datas.
    f = spot.formula(ltl)
    c = spot.mp_class(f)
    sync_rec = f.is_syntactic_recurrence()
    aut_pos = spot.translate(ltl, 'BA')
    aut_pos_det = spot.is_deterministic(aut_pos)
    aut_pos_st = aut_pos.num_states()
    aut_neg = spot.translate('!({})'.format(ltl), 'BA')
    aut_neg_det = spot.is_deterministic(aut_neg)
    aut_neg_st = aut_neg.num_states()
    common = [ltl, c, sync_rec, aut_pos_det, aut_pos_st, aut_neg_det,
              aut_neg_st]

    three_time = []
    for i in range(3):
        three_time.append([i])
        for algo in [1, 2]:
            three_time[i].extend(run('{} "{}" {}'.format(
                folder + 'bench/prcheck/dba_builder', ltl, algo)))

    mem = []
    res_path = folder + 'bench/prcheck/save.out'
    for algo in [1, 2]:
        run('{} "{}" {}'.format(
            folder + 'libtool e valgrind --tool=massif '
            + '--massif-out-file={} '.format(res_path)
            + folder + 'bench/prcheck/dba_builder', ltl, algo))

        with open(res_path, 'r') as f:

            massif_lines = f.readlines()
            mem_heap = []
            mem_heap_extra = []
            mem_stacks = []
            for massif_l in massif_lines:
                if 'mem_heap_B' in massif_l:
                    mem_heap.append(get_mem(massif_l))
                elif 'mem_heap_extra_B' in massif_l:
                    mem_heap_extra.append(get_mem(massif_l))
                elif 'mem_stacks_B' in massif_l:
                    mem_stacks.append(get_mem(massif_l))

        idx = mem_heap.index(max(mem_heap))
        mem.append([mem_heap[idx], mem_heap_extra[idx], mem_stacks[idx]])

    for i in range(3):
        out = []
        out.extend(common)
        out.extend(three_time[i][0:13])
        out.extend(mem[0])
        out.extend(three_time[i][13:23])
        out.extend(mem[1])
        add_line(csv_path, out)

def run_bench(folder, header, source, formulas):
    csv_path = folder + 'bench/prcheck/dba.{}.csv'.format(source)
    try:
        os.remove(csv_path)
    except OSError:
        pass
    add_line(csv_path, header)
    for ltl in formulas:
        print('formula {}'.format(ltl))
        if ltl.strip() == "":
            continue
        p = multiprocessing.Process(target=run_ltl,
                                    args=(ltl, folder, csv_path,))
        p.start()
        p.join(timeout=120)
        if p.is_alive():
            p.terminate()
            timedout = [ltl]
            timedout += [''] * 35
            add_line(csv_path, timedout)

def get_ltl(folder, kind):
    ltl = []
    with open(folder + 'bench/prcheck/{}'.format(kind), 'r') as f:
        lines = f.readlines()
    for f in lines:
        if spot.is_recurrence(spot.formula(f[:-1])):
            ltl.append(f[:-1])
    return ltl


def main():
    # Prepare things.
    folder = os.environ['SPOT_BENCH_PRCHECK']
    header = ['ltl', 'class', 'sync.rec', 'pos_aut.det',
              'pos_aut.st', 'neg_aut.det', 'neg_aut.st', 'run',
              'meth1.trad.walltime', 'meth1.nca.walltime',
              'meth1.dca.walltime', 'meth1.dualize.walltime',
              'meth1.cputime.sys', 'meth1.cputime.user', 'meth1.walltime',
              'meth1.pages',
              'meth1.res.det', 'meth1.res.st', 'meth1.res.aut',
              'meth1.mem.heap', 'meth1.mem.extra', 'meth1.mem.stacks',
              'meth2.trad.walltime', 'meth2.det.walltime',
              'meth2.rabin.walltime', 'meth2.buchi.walltime',
              'meth2.cputime.sys', 'meth2.cputime.user', 'meth2.walltime',
              'meth2.pages',
              'meth2.res.det', 'meth2.res.st', 'meth2.res.aut',
              'meth2.mem.heap', 'meth2.mem.extra', 'meth2.mem.stacks']
    ltl_rand = get_ltl(folder, 'random')
    ltl_patt = get_ltl(folder, 'patterns')

    run_bench(folder, header, 'random', ltl_rand)
    run_bench(folder, header, 'patterns', ltl_patt)


if __name__ == '__main__':
    main()
