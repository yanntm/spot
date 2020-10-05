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

import numpy as np
import PySimpleGUI as sg

def correlation(x, y):
    def average_index(arr, element):
        return np.mean(np.where(arr == element))
    sortedx = np.sort(x)
    sortedy = np.sort(y)
    sum_squares = 0
    for i in range(x.size):
        x_ = average_index(sortedx, x[i])
        y_ = average_index(sortedy, y[i])
        sum_squares += (x_ - y_) ** 2
    return 1 - (6 * sum_squares / (x.size ** 3 - x.size))

def correlation_matrix(features):
    table = np.zeros((len(features), len(features)))
    for x in range(len(features)):
        for y in range(len(features)):
            table[x][y] = abs(correlation(features[x], features[y]))
    return table

def gui_display_table(table, names):
    sg.theme('DarkAmber')   # Add a touch of color
    # All the stuff inside your window.

    name_max_len = len(max(names, key=len))
    layout = []
    for i in range(len(table)):
        row = [sg.Text(names[i].rjust(name_max_len, ' '), size=(20, 1))]
        for j in range(i + 1):
            elt = table[i][j]
            name = '%s/%s' % (names[i], names[j])
            row.append(sg.Button(str(round(elt, 2)).ljust(4, '0'),
                                 tooltip=name, key=name))
        layout.append(row)
    layout.append([sg.Button('Exit')])

    window = sg.Window('Correlation Table', layout)
    image_window = None
    while True:
        event, _ = window.read()

        if event == sg.WIN_CLOSED or event == 'Exit':
            break
        else:
            feature1, feature2 = event.split('/')
            if image_window:
                image_window.close()
                image_window = None
            image_window = sg.Window(event,
                                    [[sg.Image('scps/scp-%s-%s.png'
                                               % (feature1, feature2))],
                                    [sg.Button('Close')]])
        if image_window:
            img_event, _ = image_window.read()
            if img_event == sg.WIN_CLOSED or img_event == 'Close':
                image_window.close()
                image_window = None

    window.close()
