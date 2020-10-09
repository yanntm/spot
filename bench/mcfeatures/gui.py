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

import PySimpleGUI as sg
import numpy as np
import os

from correlations import *
from plot import *
from data_analysis import filter_features

def make_layout(table, names, excluded, cachefolder, threads, windowsize):
    name_max_len = len(max(names, key=len))
    layouttable = []
    for i in range(len(names)):
        if names[i] in excluded:
            continue
        row = [sg.Text(names[i].rjust(name_max_len, ' '), size=(20, 1))]
        for j in range(i + 1):
            if names[j] in excluded:
                continue
            elt = table[i][j]
            name = '%s/%s' % (names[i], names[j])
            row.append(sg.Button(str(round(elt, 2)).ljust(4, '0'),
                                 size=(1, 1),
                                 tooltip=name,
                                 key=name))
        layouttable.append(row)


    timebuttons = []
    for thr in threads:
        timebuttons.append(sg.Button('Time' + thr,
                            tooltip='time difference between bloemen and' +
                            'cndfs execution', key='Time' + thr))
    sortbuttons = [sg.Button('Filter') if not excluded\
                  else sg.Button('See all')]
    for name in excluded:
        sortbuttons.append(sg.Button('Sort ' + name))

    layoutwindow = [
                     [
                       sg.Column([
                                   [sg.Column(layouttable, key='table')],
                                   timebuttons
                                 ],
                                 key='left', expand_x=True,
                                 size=(None,
                                       windowsize[1] - windowsize[1] // 8)),
                       sg.Column([
                                   [sg.Image(cachefolder +
                                             'time_difference%s.png' %\
                                                                    threads[0],
                                             key='image')]
                                 ])
                     ],
                     sortbuttons,
                     [
                       sg.Column([[sg.Button('a', key='sb0', visible=False)]],
                                 pad=(0,0), key='sc0'),
                       sg.Column([[sg.Button('b', key='sb1', visible=False)]],
                                 pad=(0,0), key='sc1'),
                       sg.Column([[sg.Button('c', key='sb2', visible=False)]],
                                 pad=(0,0), key='sc2')
                     ],
                     [sg.Button('Exit')]
                   ]
    return layoutwindow

def generate_images(features, names, excluded, basepath):
    for i in range(len(names)):
        if names[i] in excluded:
            continue
        for j in range(i + 1):
            if names[j] in excluded:
                continue
            filename = basepath + '%s-%s.png' % (names[i], names[j])
            if os.path.isfile(filename):
                continue
            scatter_plot(features[names[j]], features[names[i]],
                         'scatter plot of %s over %s' %\
                         (names[j], names[i]), names[j], names[i])
            plt.savefig(filename)
            plt.clf()

def update_filter_buttons(window, values):
    window['sb2'].Update(visible=False)
    window['sb1'].Update(visible=False)
    window['sb0'].Update(text=values[0], visible=True)
    if values.size > 1:
        window['sb1'].Update(text=values[1], visible=True)
        if values.size > 2:
            window['sb2'].Update(text=values[2], visible=True)

def gui_display(features, names, simplenames, cachefolder, threads):
    sg.theme('DarkAmber')
    tmpwindow = sg.Window('get_size')
    windowsize = tmpwindow.GetScreenDimensions()
    windowsize = (windowsize[0] - windowsize[0] // 12,
                  windowsize[1] - windowsize[1] // 12)
    tmpwindow.close()

    sortby, feature1, features2 = '', '', ''
    values, filter = None, None
    complexnames = [x for x in names if x not in simplenames]

    table = correlation_matrix(features, names)
    generate_images(features, names, [], cachefolder + 'scps/scp-')
    layout = make_layout(table, names, [], cachefolder, threads, windowsize)
    window = sg.Window('Correlation Table', layout, size=windowsize,
                       finalize=True)
    while True:
        event, _ = window.read()

        if event == sg.WIN_CLOSED or event == 'Exit':
            break
        elif event == 'Filter' or event == 'See all':
            sortby, feature1, features2 = '', '', ''
            values, filter = None, None
            excluded = [] if event == 'See all' else simplenames
            newlayout = make_layout(table, names, excluded, cachefolder,
                                    threads, windowsize)
            window.close()
            newwindow = sg.Window('Correlation Table', newlayout,
                                  size=windowsize, finalize=True)
            window = newwindow
        elif 'Sort' in event:
            sortby = event.split(' ')[-1]
            values = np.unique(features[sortby])
            update_filter_buttons(window, values)
        elif 'sb' in event and len(event) == 3:
            filter = values[int(event[-1])]
            f = filter_features(features, names, simplenames, sortby, filter)
            ftable = correlation_matrix(f, complexnames)
            newlayout = make_layout(ftable, complexnames,
                                    excluded, cachefolder, threads, windowsize)
            window.close()
            newwindow = sg.Window('Correlation Table', newlayout,
                                  size=windowsize, finalize=True)
            window = newwindow
            update_filter_buttons(window, values)
            generate_images(f, complexnames, excluded,
                            '%sscps/scp%s%s-' % (cachefolder, sortby,
                                                 str(filter)))

        elif 'Time' in  event:
            window['image'].update(cachefolder + 'time_difference%s.png' %\
                                   (event[-2:] if event != 'Time' else ''))
        else:
            feature1, feature2 = event.split('/')
            window['image'].update(cachefolder +
                                   'scps/scp%s%s-%s-%s.png' %
                                   (sortby,
                                    str(filter) if filter is not None else '',
                                    feature1, feature2))
    window.close()
