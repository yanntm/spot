#!/usr/bin/env python3

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

# aut1, aut2, strength aut1, strength aut2, time prd+ec tae, time prd otf, time ec otf, time prd exp, time ec exp, res tae, res otf, res exp, states prd, trans prd, time ar tae, time ar otf, time ar exp

def comp(ids):
    id1, id2 = ids
    aut1 = auts[id1]
    aut2 = auts[id2]

    start = time.perf_counter()
    res_tae = spot.two_aut_ec(aut1, aut2)
    end = time.perf_counter()
    time_prd_ec_tae = end - start

    start = time.perf_counter()
    prd_otf = spot.otf_product(aut1, aut2)
    end = time.perf_counter()
    time_prd_otf = end - start

    start = time.perf_counter()
    res_otf = prd_otf.is_empty()
    end = time.perf_counter()
    time_ec_otf = end - start

    start = time.perf_counter()
    prd_exp = spot.product(aut1, aut2)
    end = time.perf_counter()
    time_prd_exp = end - start

    start = time.perf_counter()
    res_exp = prd_exp.is_empty()
    end = time.perf_counter()
    time_ec_exp = end - start

    time_ar_tae = None
    time_ar_otf = None
    time_ar_exp = None

    if res_tae is not None:
        start = time.perf_counter()
        ar_tae = res_tae.left_accepting_run()
        end = time.perf_counter()
        time_ar_tae = end - start

        start = time.perf_counter()
        ar_otf = prd_otf.accepting_run()
        end = time.perf_counter()
        time_ar_otf = end - start

        start = time.perf_counter()
        ar_exp = prd_exp.accepting_run()
        end = time.perf_counter()
        time_ar_exp = end - start

    with done_lock:
        done.value += 1
    print("{}/{}".format(done.value, todo))
    return (id1, id2, bool(aut1.prop_weak()), bool(aut2.prop_weak()),
            time_prd_ec_tae, time_prd_otf, time_ec_otf, time_prd_exp, time_ec_exp,
            not bool(res_tae), res_otf, res_exp,
            prd_exp.num_states(), prd_exp.num_edges(),
            time_ar_tae, time_ar_otf, time_ar_exp)

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
            a = spot.remove_fin(a)
            spot.check_strength(a)
            auts.append(a)
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
