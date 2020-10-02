#!/usr/bin/env python3

import csv
import numpy as np
import sys
from matplotlib import pyplot as plt

# begin GLOBALS
intfeatures = ['bloemen_time', 'cndfs_time',\
               'memory', 'states', 'transitions', 'sccs', 'incidence_in',\
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
time = None
# end GLOBALS

def read_csv():
    global features, time
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

def generate_time_scatter_plot():
    # getting data
    bloemen_time = features[0]
    cndfs_time = features[1]
    bloemen_time_empty = []
    bloemen_time_nonempty = []
    cndfs_time_empty = []
    cndfs_time_nonempty = []
    for i in range(bloemen_time.size):
        if features[nb_int - 1][i]:
            bloemen_time_empty.append(bloemen_time[i])
            cndfs_time_empty.append(cndfs_time[i])
        else:
            bloemen_time_nonempty.append(bloemen_time[i])
            cndfs_time_nonempty.append(cndfs_time[i])

    # plotting
    plt.title('time difference CNDF/Bloemen')
    plt.xlabel('cndfs execution time')
    plt.ylabel('bloemen execution time')
    max_time = max(np.amax(cndfs_time), np.amax(bloemen_time))
    min_time = min(np.amin(cndfs_time), np.amin(bloemen_time))
    plt.plot([min_time, max_time], [min_time, max_time], 'k-',
                            label='x = y axis')
    plt.plot(cndfs_time_empty, bloemen_time_empty, 'ob',
                       color='b', label='empty')
    plt.plot(cndfs_time_nonempty, bloemen_time_nonempty, 'ob',
                      color='r', label='non-empty')

    x, y = cndfs_time, bloemen_time
    b = (np.sum(x * y) - np.sum(x) * np.sum(y) / x.size)\
        / (np.sum(x * x) - ((np.sum(x) ** 2) / x.size))

    plt.plot([min_time, min(max_time, max_time / b)],
                            [min_time, min(max_time, b * max_time)],
                            'k-', linestyle='--', label='linear regression')

    plt.legend()
    plt.savefig('time_difference.png')
    plt.clf()

def generate_plots(correlations):
    def generate_plot(x, y, basename):
        if np.unique(y).size < 4:
            plt.plot(y, x, 'ob')
            plt.xlabel(basename.replace('_', ' '))
            plt.ylabel('time difference')
        else:
            plt.plot(x, y, 'ob')
            plt.ylabel(basename.replace('_', ' '))
            plt.xlabel('time difference')
        plt.title('%s (%s)' % (basename.replace('_', ' '),
                  correlations[basename]))
        plt.savefig(basename + '.png')
        plt.clf()

    x = time
    for i in range(nb_int):
        y = features[i]
        generate_plot(x, y, intfeatures[i])

    for i in range(nb_float):
        y = features[i + nb_int]
        generate_plot(x, y, floatfeatures[i])
    generate_time_scatter_plot()


def get_correlations():
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
    correlations = {}
    for i in range(nb_int):
        corr = correlation(x, features[i])
        correlations[intfeatures[i]] = corr
    for i in range(nb_float):
        corr = correlation(x, features[i + nb_int])
        correlations[floatfeatures[i]] = corr
    return correlations

if __name__ == '__main__':
    read_csv()
    correlations = get_correlations()
    generate_plots(correlations)

    for value, feature in sorted(((v,k) for k,v in correlations.items())):
        print('%s : %s' % (feature.ljust(23), value))

    average_time = 0
    for t in time:
        average_time += t
    average_time /= time.size

    print('\naverage time difference : %i (%i -> %i)'
          % (average_time, np.amin(time), np.amax(time)))
