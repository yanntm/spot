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
            row.append(sg.Button(str(round(elt, 2)).ljust(4, '0'), size=(1, 1),
                                 tooltip=name, key=name))
        layout.append(row)
    layout.append([sg.Button('Time', size=(1, 1),
                  tooltip='time difference between bloemen and cndfs execution',
                  key='time')])

    window_layout = [[sg.Column(layout),
                      sg.Column([[sg.Image('time_difference.png', key='image')]])],
                     [sg.Button('Exit')]]



    window = sg.Window('Correlation Table', window_layout)
    while True:
        event, _ = window.read()

        if event == sg.WIN_CLOSED or event == 'Exit':
            break
        elif event == 'time':
            window['image'].update('time_difference.png')
        else:
            feature1, feature2 = event.split('/')
            window['image'].update('scps/scp-%s-%s.png' % (feature1, feature2))
    window.close()
