#!/usr/bin/env python3

import csv
import numpy as np
import sys
from matplotlib import pyplot as plt

# begin GLOBALS
intfeatures = ['memory', 'states', 'transitions', 'incidence_in',\
               'incidence_out', 'repeated_transitions', 'terminal', 'weak',\
               'inherently_weak', 'very_weak', 'complete', 'universal',\
               'unambiguous', 'semi_deterministic', 'stutter_invariant']
floatfeatures = ['average_incidence_ratio']
stringfeatures = ['emptiness']
nb_int = len(intfeatures)
nb_float = len(floatfeatures)
nb_string = len(stringfeatures)
nb_features = nb_int + nb_float + nb_string
features = []
for _ in range(nb_features):
    features.append([])
time = []
# end GLOBALS

def read_csv():
    global features, time
    with open(sys.argv[1], newline='') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            time.append(float(row['time']))
            for i in range(nb_int):
                features[i].append(int(row[intfeatures[i]]))
            for i in range(nb_float):
                features[i + nb_int].append(float(row[floatfeatures[i]]))
            for i in range(nb_string):
                features[i + nb_int + nb_float].append(row[stringfeatures[i]])
    npfeatures = []
    for f in features:
        npfeatures.append(np.asarray(f))
    features = npfeatures
    time = np.asarray(time)

def generate_plots():
    def generate_plot(x, y, basename):
        if np.unique(y).size < 4:
            plt.plot(y, x, 'ob')
        else:
            plt.plot(x, y, 'ob')
        plt.savefig(basename + '.png')
        plt.clf()

    x = time
    for i in range(nb_int):
        y = features[i]
        generate_plot(x, y, intfeatures[i])

    for i in range(nb_float):
        y = features[i + nb_int]
        generate_plot(x, y, floatfeatures[i])

    for i in range(nb_string):
        y = features[i + nb_int + nb_float]
        generate_plot(x, y, stringfeatures[i])

def log_correlations():
    def average_index(arr, element):
        return np.mean(np.where(arr == element))
    def correlation(x, y):
        sortedx = np.sort(x)
        sortedy = np.sort(y)
        sum_squares = 0
        for i in range(x.size):
            x_ = average_index(sortedx, x[i])
            y_ = average_index(sortedy, y[i])
            sum_squares += (x_ - y_) ** 2
        return 1 - (6 * sum_squares / (x.size ** 3 - x.size))

    x = time
    for i in range(nb_int):
        print('correlation %s : ' % intfeatures[i], correlation(x, features[i]))
    for i in range(nb_float):
        print('correlation %s : ' % floatfeatures[i],
              correlation(x, features[i + nb_int]))


if __name__ == '__main__':
    read_csv()
    generate_plots()
    log_correlations()
