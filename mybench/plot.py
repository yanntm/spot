#!/usr/bin/python3

import csv
import os
import sys
import matplotlib.pyplot as plt

if len(sys.argv) != 2:
    print("{} <times.csv>".format(sys.argv[0]), file=sys.stderr)
    sys.exit(1)

basename = os.path.splitext(sys.argv[1])[0]

tae = list()
pec = list()
color = list()
g = 0
r = 0

with open("{}.csv".format(basename), 'r') as f:
    reader = csv.reader(f)
    for row in reader:
        tae_time = float(row[0])
        pec_time = float(row[1])
        tae.append(tae_time)
        pec.append(pec_time)
        if (tae_time < pec_time):
            color.append("green")
            g += 1
        else:
            color.append("red")
            r += 1

fig = plt.figure()
plt.scatter(pec, tae, c=color, marker='.')
plt.xlabel('product + emptiness check')
plt.ylabel('two-automaton emptiness check')
fig.suptitle('red:' + str(r) + ', green:' + str(g) + ', mean time ratio:' + str(round(100 * sum(tae)/sum(pec))) + '%')
plt.savefig("{}.pdf".format(basename), format='pdf')
plt.show()
