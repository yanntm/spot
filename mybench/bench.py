#!/usr/bin/python3

import csv
import multiprocessing
import os
import sys
import time

import spot

todo = int()
auts = list()

done = multiprocessing.Value('i', 0)
done_lock = multiprocessing.Lock()

# aut1, aut2, time tae, time otf, time prd, res tae, res otf, res prd, states, trans

def comp(ids):
    id1, id2 = ids
    aut1 = auts[id1]
    aut2 = auts[id2]

    start_tae = time.perf_counter()
    res_tae = spot.two_aut_ec(aut1, aut2)
    end_tae = time.perf_counter()

    start_otf = time.perf_counter()
    res_otf = spot.otf_product(aut1, aut2).is_empty()
    end_otf = time.perf_counter()

    start_prd = time.perf_counter()
    prd = spot.product(aut1, aut2)
    res_prd = prd.is_empty()
    end_prd = time.perf_counter()

    with done_lock:
        done.value += 1
    print("{}/{}".format(done.value, todo))
    return (id1, id2,
            end_tae - start_tae, end_otf - start_otf, end_prd - start_prd,
            res_tae, res_otf, res_prd,
            prd.num_states(), prd.num_edges())

if __name__ == '__main__':
    if len(sys.argv) < 4:
        print(sys.argv[0], "<# of automata> <size of thread pool> <randaut options...>", file=sys.stderr)
        exit(1)
    count = int(sys.argv[1])
    pool = int(sys.argv[2])
    options = ' '.join(sys.argv[3:])
    command = 'randaut -n {} {}'.format(count, options)
    combinations = list()
    for i in range(count):
        for j in range(i, count):
            combinations.append((i, j))
    todo = len(combinations)
    name = "bench-{}-{}.csv".format(count, os.uname()[1])

    with open("bench.log", 'w') as log:
        print("start:", time.asctime(), file=log)
        print(time.asctime())

        print(command, file=log)
        print(command)

        c = 0
        for a in spot.automata(command + "|"):
            print("{}/{}".format(c, count))
            auts.append(spot.remove_fin(a))
            c += 1

        print("end of generation:", time.asctime(), file=log)
        print(time.asctime())

        with multiprocessing.Pool(pool, maxtasksperchild=1) as p:
            times = p.map(comp, combinations)

        print("end of computing:", time.asctime(), file=log)
        print(time.asctime())

        with open(name, 'w') as f:
            writer = csv.writer(f)
            writer.writerows(times)
        print("Dumped CSV as", name)

        print("end of dumping:", time.asctime(), file=log)
        print(time.asctime())
