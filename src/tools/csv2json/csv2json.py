#!/usr/bin/env python3
## -*- coding: utf-8 -*-
## Copyright (C) 2013 Laboratoire de Recherche et Développement de
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

import argparse

def indent(string, offset):
  print(' ' * offset + string)

def indent_s(string, offset):
  print(' ' * offset + string, end='')

def stringify(s):
  s = s.lstrip()
  s = s.rstrip()
  s = s.replace('"','')
  return '"' + s + '"'

def rm_duplicates(l): 
  seen = {}
  result = []
  for e in l:
    if e in seen: continue
    seen[e] = 1
    result.append(e)
  return result

def print_col(name, c_array, c_index, string, inputs, rm):
  name = stringify(name)
  indent((name + ': ['), 2)
  l = []
  for i in range(1, len(c_array)):
    s = c_array[i][c_index]
    if string:   
      l.append(stringify(s))
    else:
      l.append(s)
  l = rm_duplicates(l)
  t = range(0, len(l))
  keep = inputs[name.replace('"','')]
  if keep:
    if name.replace('"','') in rm:
      l = [stringify(e) for e in l if e.replace('"','') not in str(keep[0])]
    else:
      l = [stringify(e) for e in l if e.replace('"','') in str(keep[0])]
  for i in range(0, len(l)):
    indent_s(l[i], 4)
    if (i != len(l)-1):
      print(',')
  print()
  indent('],',2)

def print_row(name, array, string):
  name = stringify(name)
  indent((name + ': ['),2)
  for i in range(0, len(array)):
    s = array[i]
    if string:
      indent_s(stringify(s), 4)
    else:
      indent_s(s,4)
    if (i != len(array)-1):
      print(',')
  print()
  indent("],",2)

def format_results(mtx):
  l = []
  for i in range(0, len(mtx)):
    s = '[ '
    for j in range(0, len(mtx[0])):
      s += mtx[i][j].strip()
      if j != len(mtx[0])-1:
        s += ', '
    s += ' ]'
    l.append(s)
  return l

def print_results(array):
  array = format_results(array)
  indent('"results": [',2)
  for i in range(0, len(array)):
    s = array[i]
    indent_s(s.strip(), 4)
    if (i != len(array)-1):
      print(',')
  print()
  indent("]",2)

def arr_from_col(data, index):
  l = []
  for i in range(1, len(data)):
    s = data[i][index]
    l.append(s)
  return rm_duplicates(l)

def build_inputs(data, inputs, inputs_idx, rm):
  fields = [e.replace('"','') for e in data[0]]
  fields = [e.strip() for e in fields]
  d = {}
  for i in range(0, len(inputs_idx)):
    key = inputs_idx[i]
    l = arr_from_col(data, inputs_idx[i])
    keep = inputs[fields[key]]
    if keep:
      if fields[inputs_idx[i]] in rm:
        value = [e for e in l if e.replace('"','') not in str(keep[0])]
      else:
        value = [e for e in l if e.replace('"','') in str(keep[0])] 
    else:
      value = l
    d[key] = value
  return d

def format_arg(s, data):
  fields = [e.replace('"','') for e in data[0]]
  fields = [e.strip() for e in fields]
  args = s.split(',')
  inputs = {}
  for e in args:
    if '[' in e or '{' in e:
      if '[' in e:
        idx = e.index('[')
      else:
        idx = e.index('{')
      try:
        key = fields[int(e[:idx])]
      except:
        key = e[:idx]
      if '[' in e:
        inputs[key] = (e[idx+1:len(e)-1].split(';'), 'k')
      else:
        inputs[key] = (e[idx+1:len(e)-1].split(';'), 'r') 
    else:
      try:
        key = fields[int(e)]
      except:
        key = e
      inputs[key] = ([], '')
  # keep or delete items inside
  rm = [] # delete if in rm, keep else
  for e in inputs:
    if inputs[e][1] == 'r':
      rm.append(e)
    inputs[e] = inputs[e][0]
  return (inputs, rm)

def get_indexes(inputs, data):
  fields = [e.replace('"','') for e in data[0]]
  fields = [e.strip() for e in fields]
  l = []
  for e in inputs.keys():
    try:
      l.append(fields.index(e))
    except:
      l.append(int(e))
  return l
 
def process_file(filename, arg):
  import csv
  with open (filename) as f:
    data = []
    for row in csv.reader(f):
      data.append(row)
    f_arg = format_arg(arg, data)
    inputs = f_arg[0]
    rm = f_arg[1]
    inputs_idx = get_indexes(inputs, data)
    nbinput = len(inputs_idx)
    if len(data[0]) != len(data[1]):
      raise Exception("fields' length different from results' length")
    print('{')
    # Inputs
    for i in range(0, nbinput):
      idx = inputs_idx[i]
      print_col(data[0][idx], data, idx, True, inputs, rm)
    # Fields
    fields = []
    for i in range(0, len(data[0])):
      fields.append(data[0][i])
    print_row("fields", fields, True)
    # inputs
    s = ''
    s += '"inputs": [ '
    for i in range(0, nbinput):
      s += str(inputs_idx[i])
      if i != nbinput-1:
        s += ', '
    s += ' ],'
    indent(s, 2)
    # Results
    results = []
    inputs_cols = build_inputs(data, inputs, inputs_idx, rm)
    for i in range(1, len(data)):
      l = []
      for j in range(0, len(fields)):
        if j in inputs_idx:
          field = fields[j].replace('"','').strip() 
          arr = inputs_cols[j]
          if data[i][j] in arr:
            index = arr.index(data[i][j])
            if inputs[field]:
              if field in rm:
                if data[i][j] not in inputs[field][0]:
                  l.append(str(index))
              else:
                if data[i][j] in inputs[field][0]:
                  l.append(str(index))
            else:
              l.append(str(index))
        else:
          l.append(data[i][j])
      if len(l) == len(fields):
        results.append(l)
    print_results(results)
    print('}')

def main():
  p = argparse.ArgumentParser(
    description="Convert an ltlcross' CSV output into a JSON file",
    epilog="Report bugs to <spot@lrde.epita.fr>.")
  p.add_argument('filename',
                 help="name of the CSV file to read",
                 default='')
  p.add_argument('-i',
                 help="names or index of inputs",
                 default='')
  args = p.parse_args()
  if args.i:
    process_file(args.filename, args.i)
  else:
    print('You forgot to write the inputs. Use -i option.')

main()
