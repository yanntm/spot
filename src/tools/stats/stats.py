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
import json
import argparse
import sys

# For latex output
def latex_escape_char(ch):
  if ch in '#$%&_{}':
    return '\\' + ch
  elif ch in '~^':
    return '\\' + ch + '{}'
  elif ch == '\\':
    return '\\textbackslash'
  else:
    return ch

def latex_escape(x):
  if type(x) == str:
    return ''.join(latex_escape_char(ch) for ch in x)
  return map(latex_escape, x)

def rot(x):
  if type(x) == str:
    return '\\rot{' + x + '}'
  return map(rot, x)
	
def format_tool(data):
  for i in range(0, len(data['tool'])):
    # Remove any leading directory, and trailing %...
    name = data["tool"][i]
    name = name[name.rfind('/', 0, name.find(' ')) + 1:]
    data["tool"][i] = latex_escape(name[0:name.find('%')])
#---------------------------

#====================================================================
#                            --cumul                                #
#====================================================================
def only_sfields(result, sfields_idx):
  # Keep only the results of secondaries fields
  l = []
  for i in sfields_idx:
    l.append((result[i]))
  return l

def rm_duplicates(l):
  # Removes duplicates in a list
  seen = {}
  result = []
  for e in l:
    if e in seen: continue
    seen[e] = 1
    result.append(e)
  return result

def sum_array(arr1, arr2):
  # Builds a new list by summing the integers pairwise
  l = [0] * len(arr1)
  for i in range(0, len(arr1)):
    l[i] = arr1[i] + arr2[i]
  return l

def args_tuple(args_idx, row):
  # Builds a tuple of args from row
  t = ()
  for i in range(0, len(args_idx)):
    t += (row[args_idx[i]],)
  return t

def parse_sum(data, rfields_rev_idx, uniq_fields, toignore):
  # Builds a dictionnary with the sum of the results
  d = {}
  for row in data['results']:
    if args_tuple(uniq_fields, row) not in toignore:
      t = args_tuple(rfields_rev_idx, row)
      if t not in d:
       d[t] = (row, 1)
      else:
        i = d[t][1] + 1
        newrow = sum_array(row, d[t][0]) 
        d[t] = (newrow, i)
  for key in d:
    l = []
    for e in d[key][0]:
      l.append([e, ''])
    d[key] = (l, d[key][1])
  return d

def list_of_dictionnaries(n):
  # Builds a list of n dictionnaries
  l = []
  for i in range(0, n):
    l.append({})
  return l

def parse_sum_type(data, mc, rfields_rev_idx, uniq_fields, toignore):
  # Builds a dictionnary with the sum of the results
  l = list_of_dictionnaries(len(mc[1])) # because [{}] * n doesn't work
  for row in data['results']:
    if args_tuple(uniq_fields, row) not in toignore:
      t = args_tuple(rfields_rev_idx, row)
      d = l[row[mc[0]]]
      if t not in d:
       d[t] = (row, 1)
      else:
        i = d[t][1] + 1
        newrow = sum_array(row, d[t][0]) 
        d[t] = (newrow, i)
  for d in l:
    for key in d:
      tmp = []
      for e in d[key][0]:
        tmp.append([e, ''])
      d[key] = (tmp, d[key][1])
  return l

def field2input(data, rfields):
  # Transform a common field into an input
  di = data['inputs']
  df = data['fields']
  inputs = []
  noinputs = []
  for e in rfields:
    if df.index(e[0]) in di:
      inputs.append(e)
    else:
      noinputs.append(e)
  for field in noinputs:
    f = field[0]
    f_idx = df.index(f)
    data[f] = []
    seen = {}
    for row in data['results']:
      if row[f_idx] in seen: continue
      seen[row[f_idx]] = 1
      data[f].append(str(row[f_idx]))
    di.append(f_idx)

def minmax(d, sec_fields, df, forall):
  # Marks minimum, maximum...
  if forall:
    sec_fields = [(e, forall) for e in df]
  for field in sec_fields: # for all optional attributs
    if field[1]:
      field_idx = df.index(field[0])
      d2 = {}
      for key in d: # create d2
        t = tuple(key[1:])
        d2[t] = []
      for key in d: # fill in d2
        t = tuple(key[1:])
        d2[t].append(d[key][0][field_idx])
      for key in d2: # compute d2
        tmp = d2[key][:]
        d2[key] = []
        if 'min' in field[1]:
          d2[key].append(min(tmp))
        if 'max' in field[1]:
          d2[key].append(max(tmp))
      for key in d: # change d thanks to d2
        t = tuple(key[1:])
        attr = ''
        if '%' in field[1]:
          attr += '%'
        if '/' in field[1]:
          attr += '/'
        nblist = [e[0] for e in d2[t]]
        if d[key][0][field_idx][0] in nblist:
          if 'min' in field[1] and 'max' in field[1]:
            if d[key][0][field_idx][0] == d2[t][0][0]:
              attr += 'min'
            else:
              attr += 'max'
          else:
            if 'max' in field[1]:
              attr += 'max'
            if 'min' in field[1]:
              attr += 'min'
        d[key][0][field_idx][1] += attr

def combine(n, arrays, t, d):
  # Generates combinations from elements of each array
  if n > -1:
    for e in arrays[n]:
      t[n] = e
      d[tuple(t)] = 1
      combine(n-1, arrays, t, d)

def generate_combi(fields, data):
  d_fields = {}
  size = 1
  i = 0
  for e in fields:
    d_fields[i] = data[e]
    size *= len(data[e])
    i += 1
  len_df = len(d_fields)
  l = []
  for i in range(0, len_df):
    l.append(range(0, len(d_fields[i])))
  combi = {}
  combine(len_df-1, l, [0] * len_df, combi)
  l = []
  for e in combi.keys():
    l.append(e)
  l.sort()
  return l

def create_blacklist(data, df, di, ref_fields, ufields):
  # Marks the elements that are not in common to the inputs
  rfields_str = [e[0] for e in ref_fields]
  di_str = [df[i] for i in di]
  uniq_fields_str = [e for e in di_str if e not in rfields_str]
  uniq_fields = [df.index(e) for e in uniq_fields_str]
  for e in ufields:
    uniq_fields.append(e)
  combi = 1
  for e in rfields_str:
    combi *= len(data[e])
  d = {}
  for row in data['results']:
    t = args_tuple(uniq_fields, row)
    if t not in d:
      d[t] = 1
    else:
      d[t] += 1
  blacklist = {}
  for e in d:
    if d[e] != combi:
      blacklist[e] = 1
  return (blacklist, uniq_fields)

def sub_print_cumul(data, nm_dec, rf_rev, d, dc, maxcline, n, t, prev_t, i):
  if n > -1: # n: index of the ref field
    i += 1 # number of iteration of the function
    b1 = True # because there is '&' in less after a multirow
    b2 = True # to avoid printing \\\\ where there is no results
    # rf_rev[n] -> [0]: name of the field [1]: rot's angle
    for e in data[rf_rev[n][0]]:
      if e in data[rf_rev[len(t)-1][0]] and len(t) > 1:
        print('\\hline')
      t[n] = data[rf_rev[n][0]].index(e)
      if n != 0:
        print('&' * i, end='')
        if tuple(t[1:]) in dc:
          nbcases = dc[tuple(t[1:])]        
          if n == 1 and len(nbcases) == 1:
            s = '\\begin{tabular}{l}' + latex_escape(e) + '\\\\'
            s += str(nbcases[0]) + ' cases\end{tabular}'
          else:
            s = latex_escape(e) 
          print("\multirow{1}{*}{\\rotatebox{" 
            + rf_rev[n][1] + "}{~~%s~~}}" % s)
      else:
        if tuple(t) in d:
          if b1 and len(t) > 2:
            print('&' * (i-1), end=' ')
            b1 = False
          else:
            print('&' * i, end=' ')
          nbcases = d[tuple(t)][1]
          eachcases = True
          if len(dc[tuple(t[1:])]) == 1:
            eachcases = False
          if rf_rev[n][1] != '0': 
            s = '\\rotatebox{' + rf_rev[n][1] + "}{"
            if eachcases or len(t) == 1:
              s += latex_escape(e + ' (' + str(nbcases) + ' cases)') + '}'
            else:
              s += latex_escape(e) + '}'
          else:
            if eachcases or len(t) == 1:
              s = latex_escape(e + ' (' + str(nbcases) + ' cases)')
            else:
              s = latex_escape(e)
          print(s, end=' ')
          t_res = d[tuple(t)] 
          idx = -1
          for x in t_res[0]:
            idx += 1
            nb = x[0]
            if '/' in x[1]:
              nb /= t_res[1]
            if isinstance(nb, int):
              nb = str(nb)
            else:
              if nm_dec[idx % len(nm_dec)]:
                s = "{0:." + str(nm_dec[idx % len(nm_dec)]) + "f}"
                nb = s.format(nb)
              else:
                nb = "{0:.0f}".format(nb)
            if '%' in x[1]:
              nb += '\\%'
            if 'min' in x[1]:
                nb = '{\\min}' + nb
            if 'max' in x[1]:
                nb = '{\\max}' + nb
            print('&', nb, end=' ')
      sub_print_cumul(data, nm_dec, rf_rev, d, dc, maxcline, n-1, t, prev_t, i)
      if n == 0 and b2:
        print('\\\\')
      if len(t) > 2 and len(t)-1 != i:
        print('\\cline{' + str(len(t)-n) + '-'
            + str(maxcline) + '}')

def core_print_cumul(data, nm_dec, rfields_rev, d, dc, rfields, 
                     ntype, sfields, len_sf, len_rf, slim):
  for e in rfields:
    s = '{ ' + latex_escape(e[0]) + ' }'
    if slim:
      s = '\\rot{' + s + '}' 
    print(s, end=' & ')
  for j in range(0, ntype):
    for i in range(0, len_sf):
      s  = '{ ' + latex_escape(sfields[i][0]) + ' }'
      if slim:
        s = '\\rot{' + s + '}'
      if i != len_sf-1:
        print(s, end=' & ')
      else:
        if j != ntype-1:
          print(s, end=' & ')
        else:
          print(s, end=' \\\\\n')
  if not slim:
    print('\\hline')
  n = len_rf-1
  l = [0] * (n + 1)
  maxcline = len_sf * ntype + len_rf
  sub_print_cumul(data, nm_dec, rfields_rev, d, dc, 
                  maxcline, n, l, [-1] * (n + 1), -1)

def print_cumul_slim(filename, nm, nm_dec, data, mc, 
                     rfields, rfields_rev, sfields, d, dc):
  ntype = len(mc[1])
  len_sf = len(sfields)
  len_rf = len(rfields)
  len_f =  len_sf + len_rf
  print(r'''\section*{\texttt{%s}}
\subsection*{Cumulative summary}''' % filename)
  print("\setlongtables")
  if not ntype:
    print("\\begin{longtable}{" + 'l' * len_rf, end='')
    for e in sfields:
      nb = nm[e[0]]
      print('n{' + str(nb[0]) + '}{' + str(nb[1]) + '}', end='')
    print('}')
    ntype = 1
  else:
    print("\\begin{longtable}{" + 'l' * len_rf, end='')
    for i in range(0, ntype):
      for e in sfields:
        nb = nm[e[0]]
        print('n{' + str(nb[0]) + '}{' + str(nb[1]) + '}', end='')
    print('}')
    maxcline = len_sf * ntype + len_rf
    print('\multicolumn{'+ str(len_rf) +'}{c}{}', end=' & ')
    for i in range(0, ntype):
      print('\multicolumn{' + str(len_sf) +'}{c}{' + str(mc[1][i]) +'}', end='')
      if i != ntype-1:
        print(' & ', end='')
  print(' \\\\')
  core_print_cumul(data, nm_dec, rfields_rev, d, dc, rfields, 
                   ntype, sfields, len_sf, len_rf, True)
  print("\\end{longtable}")

def print_cumul(filename, nm, nm_dec, data, mc, 
                rfields, rfields_rev, sfields, d, dc):
  ntype = len(mc[1])
  len_sf = len(sfields)
  len_rf = len(rfields)
  len_f =  len_sf + len_rf
  print(r'''\section*{\texttt{%s}}
\subsection*{Cumulative summary}''' % filename)
  print("\setlongtables")
  if not ntype:
    print("\\begin{longtable}{" + '|l' * len_rf, end='')
    for e in sfields:
      nb = nm[e[0]]
      print('|n{' + str(nb[0]) + '}{' + str(nb[1]) + '}', end='')
    print('|}\n\\hline')
    ntype = 1
  else:
    print("\\begin{longtable}{" + '|l' * len_rf, end='')
    for i in range(0, ntype):
      for e in sfields:
        nb = nm[e[0]]
        print('|n{' + str(nb[0]) + '}{' + str(nb[1]) + '}', end='')
    print('|}')
    maxcline = len_sf * ntype + len_rf
    print('\\cline{'+ str(len_rf+1) + '-' + str(maxcline) +'}')
    print('\multicolumn{'+ str(len_rf) +'}{c|}{}', end=' & ')
    for i in range(0, ntype):
      print('\multicolumn{' + str(len_sf) +'}{c|}{' + str(mc[1][i]) +'}', end='')
      if i != ntype-1:
        print(' & ', end='')
    print(' \\\\\n\\hline')
  core_print_cumul(data, nm_dec, rfields_rev, d, dc, rfields, 
                   ntype, sfields, len_sf, len_rf, False)
  print('\\hline')
  print("\\end{longtable}")

def numprint(d, sec_fields, sfields_idx, df):
  # Find the length of the integer part of the max for each field
  nsfields_idx = [sfields_idx.index(e) for e in sfields_idx]
  nd = {}
  for i in range(0, len(sfields_idx)):
    nd[sfields_idx[i]] = [0, 0]
    maxlen = 0
    for key in d:
      nb = d[key][0][nsfields_idx[i]][0]
      if not isinstance(nb, int):
        nd[sfields_idx[i]][1] = 2
      len_s = len(str(int(nb)))
      if len_s > maxlen:
        maxlen = len_s
    nd[sfields_idx[i]][0] = maxlen
  tmp = nd.copy()
  nd = {}
  for e in tmp:
    nd[df[e]] = tmp[e]
  for e in sec_fields: # field's number of decimal if specified
    if '.' in e[1]:
      s =  e[1][e[1].index('.'):]
      decimal = ''
      for c in s:
        try:
          decimal += str(int(c))
        except: continue
      nd[e[0]][1] = int(decimal)
  return nd

def process_file_cumul(filename, arg, slim):
  data = json.load(open(filename))
  di = data['inputs']
  df = data['fields']
  args = arg.split(':')
  args1 = args[0].split(',')
  # extract fields after a *
  limit_cumul = False
  ufields = []
  l = ','.join(args1)
  if '*' in l:
    limit_cumul = True
    l = l.split('*')
    for i in range(0, len(args1)):
      args1[i] = args1[i].replace('*','')
    l = l[1][1:].split('@')[0]
    if l:
      ufields = l.split(',')
  ufields = [df.index(e) for e in ufields]
  # reference fields
  ref_fields = []
  for e in args1:
    if '@' in e:
      f = e.split('@')
      if ';' in f[1]:
        f2 = f[1].split(';')
        ref_fields.append((f[0],f2[0],f2[1])) # (field, angle, type)
      else:
        ref_fields.append((f[0],f[1],''))
    else:
      if ';' in e:
        f = e.split(';')
        ref_fields.append((f[0],'0',f[1]))
      else:
        ref_fields.append((e, '0',''))
  field2input(data, ref_fields)
  # multicol
  btype = False 
  multicols = [[e[0],e[2]] for e in ref_fields if e[2]]
  ntype = 1
  mc = [0, []]
  if multicols:
    btype = True
    mc = multicols[0] # the first for the moment
    mc_str = mc[1]
    mc_idx = df.index(mc_str)
    mc_content = data[mc_str]
    mc = (mc_idx, mc_content)
    ntype = len(mc[1])
  # secondary fields
  sec_fields = [(e, '') for e in df if df.index(e) not in di]
  sfields_idx = [df.index(e[0]) for e in sec_fields]
  forall = ''
  if len(args) == 2 and args[1]:
    if args[1][0] == '[':
      forall = args[1][1:len(args[1])-1]
    else:
      args2 = args[1].split(',')
      sec_fields = []
      for e in args2:
        if '[' in e:
          btw_bracket = e[e.index('[')+1:len(e)-1]
          e = e.replace('[' + btw_bracket + ']', '')
          sec_fields.append((e, btw_bracket))
        else:
          sec_fields.append((e,''))
      sfields_idx = [df.index(e[0]) for e in sec_fields]
  # reversing rfields for the sum
  rfields_rev = ref_fields[:]
  rfields_rev.reverse()
  rfields_rev_idx = [df.index(e[0]) for e in rfields_rev]
  # limit to elements in common
  blacklist = [[],[]]
  if limit_cumul:
    blacklist = create_blacklist(data, df, di, ref_fields, ufields)
  # sum
  if not btype:
    l = [parse_sum(data, rfields_rev_idx, blacklist[1], blacklist[0])]
  else:
    l = parse_sum_type(data, mc, rfields_rev_idx, blacklist[1], blacklist[0]) 
  # mark min,max ...
  for d in l:
    minmax(d, sec_fields, df, forall)
    for key in d: # we keep only the fields to print
      result = only_sfields(d[key][0], sfields_idx)
      d[key] = (result, d[key][1])
  d = l[0]
  # merge dictionaries if needed for vsep
  for i in range(1, len(l)):
    d2 = l[i]
    for key in d2:
      d_res = d[key][0]
      d2_res = d2[key][0]
      for e in d2_res:
        d_res.append(e)
  # numprint
  nm = numprint(d, sec_fields, sfields_idx, df)
  nm_idx = {}
  l = [e[0] for e in sec_fields]
  for e in nm:
    nm_idx[l.index(e)] = nm[e][1]
  # better cases printing
  dc = {}
  for e in d:
    dc[e[1:]] = []
  for e in d:
    t = e[1:]
    cases = d[e][1]
    if cases not in dc[t]:
      dc[t].append(cases)
  # print
  if slim:
    print_cumul_slim(filename, nm, nm_idx, data, mc, ref_fields, rfields_rev, sec_fields, d, dc)
  else:
    print_cumul(filename, nm, nm_idx, data, mc, ref_fields, rfields_rev, sec_fields, d, dc) 
#====================================================================
#====================================================================




#====================================================================
#                     --cross/--crosscases                          #
#====================================================================
def t_inputs(i_nocol, result):
  # Builds a tuple of inputs from the result array
  t = ()
  for i in range(0, len(i_nocol)):
    t += (result[i_nocol[i]],)
  return t

def idx2str(data, arr):
  # Replaces the indexes in the list by its corresponding string
  inputs = data['inputs']
  for index in inputs:
    curr_input = data['fields'][index]
    arr[index] = data[curr_input][arr[index]]
  return arr

def compute_crosscases(data, col, pred, v1, v2, cases):
  # cases=false: Number of times the predicate is verified
  # cases=true: List of results where the predicate is verified
  fields = data['fields']
  col_idx = fields.index(col)
  i_nocol = [e for e in data['inputs'] if e != col_idx]
  for e in fields:
    pred = pred.replace(e, str(fields.index(e)))
  csv = []
  csv.append(fields * 2)
  d1 = {}
  d2 = {}
  for r in data['results']:
    if r[col_idx] == v1:
      d1[t_inputs(i_nocol, r)] = r
    if r[col_idx] == v2:
      d2[t_inputs(i_nocol, r)] = r
  i = 0
  for k in d1:
    if k in d2:
      x = d1[k]
      y = d2[k]
      if eval(pred):
        i += 1
        if cases:
          csv.append(idx2str(data,x) + idx2str(data,y))
  if cases:
    return csv
  else:
    return i

#--------------------------------------------------------------------
def print_crosscases(csv):
  for row in csv:
    for i in range(0, len(row)):
      if i != len(row)-1:
        print(row[i], end=',')
      else:
        print(row[i])

def process_file_crosscases(filename, arg):
  data = json.load(open(filename))
  args = arg.split(',')
  col = args[0]
  pred = args[1].replace('"','')
  v1 = data[col].index(args[2])
  v2 = data[col].index(args[3])
  res = compute_crosscases(data, col, pred, v1, v2, True)
  print_crosscases(res)
#--------------------------------------------------------------------
def sub_print_cross(d, field, attr):
  for e in field:
    print(latex_escape(e), end='')
    i = -1
    for x in d[e]:
      n = ''
      if 'max' in attr and 'max' in x[1]:
        n += '{\max}'
      if 'min' in attr and 'min' in x[1]:
        n += '{\min}'
      n += str(x[0])
      if i != len(d):
        print(' & ', end='')
      print(n, end='')
      i += 1
    print(' \\\\')

def header_print_cross(filename, pred):
  print(r'''\section*{\texttt{%s}}
\subsection*{Cross comparison}''' % filename)
  print('predicate:', pred)
  print("\setlongtables")
 
def print_cross(filename, field, d, cases, attr, pred):
  header_print_cross(filename, pred)
  print("\\begin{longtable}{|l|", end='')
  for i in range(0, len(d)):
    print('n{' + str(cases[i]) + '}{0}|', end='')
  print('}\n\\cline{2-' + str(len(field)+1) + '}')
  print('\\multicolumn{1}{l|}{} & {' 
    + '} & {'.join(latex_escape(e) for e in field) + '}' + ' \\\\\n\\hline')
  sub_print_cross(d, field, attr)
  print('\\hline')
  print('\\end{longtable}')

def print_cross_slim(filename, field, d, cases, attr, pred):
  header_print_cross(filename, pred)
  print("\\begin{longtable}{l", end='')
  for i in range(0, len(d)):
    print('n{' + str(cases[i]) + '}{0}', end='')
  print('}')
  print('\\multicolumn{1}{l}{} & ' 
    + ' & '.join(rot('{' + latex_escape(e) + '}') for e in field) + ' \\\\')
  sub_print_cross(d, field, attr)
  print('\\end{longtable}')

def minmax_cross(d):
  # Marks the mininum and/or maximum
  for i in range(0, len(d)):
    l = []
    for e in d:
      l.append(d[e][i][0])
    nbMax = max(l)
    nbMin = min(l)
    for e in d:
      n = d[e][i]
      if n[0] == nbMax:
        n[1] += 'max'
      if n[0] == nbMin:
        n[1] += 'min'

def process_file_cross(filename, arg, slim): 
  data = json.load(open(filename))
  if ':' in arg:
    args = arg.split(':')
    args1 = args[0].split(',')
    args2 = args[1]
    attr = args2[1:len(args2)-1]
  else:
    args1 = arg.split(',')
    attr = ''
  col = args1[0]
  pred = args1[1].replace('"','')
  d = {}
  for e1 in data[col]:
    l = []
    for e2 in data[col]:
      v1 = data[col].index(e1)
      v2 = data[col].index(e2)
      l.append([compute_crosscases(data, col, pred, v1, v2, False), ''])
    d[e1] = l
  cases = [0] * len(d)
  for i in range(0, len(d)):
    t = []
    for e in d:
      t.append(d[e][i][0])
    cases[i] = len(str(max(t)))
  minmax_cross(d)
  if slim:
    print_cross_slim(filename, data[col], d, cases, attr, pred)
  else:
    print_cross(filename, data[col], d, cases, attr, pred)
#====================================================================
#====================================================================




#====================================================================
#                            --ybar                                #
#====================================================================
def print_ybar(filename, d, col_content, sec_fields, stack):
  nbcases = d[0][1]
  plots = '\n'
  for i in range(0, len(d)):
    cases = d[i][1]
    coords = d[i][0] 
    plots += '\\addplot coordinates {'
    n = 0
    for e in coords:
      n += 1
      x = str(n)
      y = str(round(e / cases))
      plots += '\n  (' + x + ',' + y + ')'
    plots += '\n};\n'
  fields = ','.join([e[0] for e in sec_fields])
  legend = ','.join(col_content)
  ybar = 'ybar'
  if stack:
    ybar += ' stacked'
  xtick = ','.join(str(i+1) for i in range(0, len(sec_fields)))
  print(r'''\section*{\texttt{%s}}
\subsection*{Bar graph: average comparison}''' % filename) 
  s = r'''\begin{tikzpicture}
\begin{axis}
[
width=24cm,
height=12cm,
title=Number of cases: ''' + str(nbcases) + ''',
''' + ybar + ''',
ymin = 0,
xmajorgrids,
ymajorgrids,
xtick = {''' + xtick +'''},
xticklabels = {''' + latex_escape(fields) + '''},
x tick label style={rotate=45},
legend entries = {'''+ latex_escape(legend) + r'''},
legend style ={at={(0.01,0.99)}, anchor=north west},
nodes near coords=\rotatebox{90}{\pgfmathprintnumber\pgfplotspointmeta},
]''' + plots + '''
\end{axis}
\end{tikzpicture}'''
  print(s)

def ybar_blacklist(data, col, di, di_str):
  combi = len(data[col])
  d = {} 
  for row in data['results']:
    t = args_tuple(di, row)
    if t not in d:
      d[t] = 1
    else:
      d[t] += 1
  blacklist = {}
  for e in d:
    if d[e] != combi:
      blacklist[e] = 1
  return blacklist

def process_file_ybar(filename, arg, stack):
  data = json.load(open(filename))
  di = data['inputs']
  df = data['fields']
  dr = data['results']
  args = arg.split(':')
  col = args[0]
  col_i = df.index(col)
  # secondary fields
  sec_fields = [(e, '') for e in df if df.index(e) not in di]
  sfields_idx = [df.index(e[0]) for e in sec_fields]
  forall = ''
  if len(args) == 2 and args[1]:
    if args[1][0] == '[':
      forall = args[1][1:len(args[1])-1] # inside brackets
    else:
      args2 = args[1].split(',')
      sec_fields = []
      for e in args2:
        if '[' in e:
          btw_bracket = e[e.index('[')+1:len(e)-1]
          e = e.replace('[' + btw_bracket + ']', '')
          sec_fields.append((e, btw_bracket))
        else:
          sec_fields.append((e,''))
      sfields_idx = [df.index(e[0]) for e in sec_fields]
  di.remove(col_i)
  di_str = [df[e] for e in di]
  blacklist = ybar_blacklist(data, col, di, di_str)
  # sum
  d = {}
  for row in dr:
    key = row[col_i]
    t = args_tuple(di, row)
    if t not in blacklist:
      if key not in d:
        d[key] = [row, 1]
      else:
        i = d[key][1] + 1 # number of cases
        newrow = sum_array(row, d[key][0])
        d[key] = [newrow, i]
  for e in d:
    tmp = d[e][0]
    l = []
    for i in sfields_idx:
      l.append(tmp[i])
    d[e][0] = l
  # print
  print_ybar(filename, d, data[col], sec_fields, stack)
#====================================================================
#====================================================================




#====================================================================
#                           --scatter                               #
#====================================================================
def print_scatter(filename, d, colf, tool1, tool2, cold, max_cold):
  legend = '' # legend entries
  plots = '' # plots
  for e in d:
    if e != -2 and len(d[e]):
      if colf:
        legend += colf + ' = ' + str(e) + ','
      plot = '\n\\addplot+[only marks, black] coordinates{'
      for i in range(0, len(d[e])):
        a = str(d[e][i][0]) 
        b = str(d[e][i][1])
        plot += '\n(' + a + ', ' + b + ')'
      plot += '\n};'
      plots += plot
  if len(d[-2]):
    legend += 'disagreement'
    plot = '\n\\addplot+[only marks] coordinates{'
    for i in range(0, len(d[-2])):
      a = str(d[e][i][0])
      b = str(d[e][i][1])
      plot += '\n(' + a + ', ' + b + ')'
    plot += '\n};'
    plots += plot
 
  s = r'''\section*{\texttt{''' + filename + r'''}}
\subsection*{Tool vs. Tool Scatter}

\begin{tikzpicture}
\begin{axis}%
[
width=24cm,
height=12cm,
enlarge x limits = false,
enlarge y limits = false,
title = {''' + tool1 + ' vs. ' + tool2 + ''' scatter on ''' + cold +'''},
%axis x line = bottom,
%axis y line = left,
legend entries = {''' + legend + '''},
legend style ={at={(0.01,0.99)}, anchor=north west},
grid = major,
xlabel = {''' + cold + ' from ' + tool1 + '''},
ylabel = {''' + cold + ' from ' + tool2 + r'''}
]''' + plots + r'''
\addplot[mark=none] expression[domain=0:''' + str(max_cold + 0.5) + r''']{x};
\end{axis}
\end{tikzpicture}'''
  print(s)

def inputs_tuples(results, inputs, col_idx):
  # Produces a list of tuples of inputs without col
  l = []
  for row in results:
    t = ()
    for i in inputs:
      t += (row[i],)
    l.append(t)
  return l

def process_file_scatter(filename, arg):
  data = json.load(open(filename))
  args = arg.split(',')
  col = args[0]
  tool1 = args[1]
  tool2 = args[2]
  cold = args[3]
  cold_idx = data['fields'].index(cold)
  col_idx = data['fields'].index(col)
  t1_idx = data[col].index(tool1)
  t2_idx = data[col].index(tool2)
  inputs = data['inputs']
  inputs.remove(col_idx)
  if len(args) == 5:
    colf = args[4]
    colf_idx = data['fields'].index(colf)
    colf_items = []
    seen = {}
    for row in data['results']:
      if row[colf_idx] in seen: continue
      seen[row[colf_idx]] = 1
      colf_items.append(row[colf_idx])
  else:
    colf = ''
    colf_items = [0]
  max_cold = 0 # For expression [domain=0:?]
  t_coords = {}
  inputs_t = inputs_tuples(data['results'], inputs, col_idx)
  for e in inputs_t:
    t_coords[e] = [0, 0, -1] # [x,y,item_idx], -2:disagreement 
  for row in data['results']:
    t = ()
    for i in inputs:
      t += (row[i],)
    v = row[cold_idx]
    if row[col_idx] == t1_idx:
      t_coords[t][0] = v
      if len(args) == 5:
        if t_coords[t][2] == -1: 
          t_coords[t][2] = row[colf_idx]
        else:
          if t_coords[t][2] != row[colf_idx]:
            color = -2
    if row[col_idx] == t2_idx:
      t_coords[t][1] = v
      if len(args) == 5:
        if t_coords[t][2] == -1: 
          t_coords[t][2] = row[colf_idx]
        else:
          if t_coords[t][2] != row[colf_idx]:
            color = -2
  max_cold = max(max(t_coords.values()))
  plots = { -1:[], -2:[] }
  for e in colf_items:
    plots[e] = []
  for e in t_coords:
    item = t_coords[e][2]
    a = t_coords[e][0]
    b = t_coords[e][1]
    plots[item].append([a, b])
  print_scatter(filename, plots, colf, tool1, tool2, cold, max_cold)
#====================================================================
#====================================================================




#====================================================================
#                         --line/--cactus                           #
#====================================================================
def print_line(filename, d, tools, inputs_s, col, cold, sort_seq):
  if not sort_seq: # subsection
    subsection = 'Line comparison'
  else:
    subsection = 'Cactus comparison'
  xlabel = '' # x axis
  for i in range(0, len(inputs_s)):
    xlabel += latex_escape(inputs_s[i])
    if i != len(inputs_s)-1:
      xlabel += ' * '
  legend = '' # legend entries
  for i in range(0, len(tools)):
    legend += latex_escape(tools[i])
    if i != len(tools)-1:
      legend += ', '
  plots = '' # plots
  for e in d:
    plot = '\n\\addplot+[mark=none] coordinates{'
    for i in range(0, len(d[e])):
      if sort_seq:
        index = sort_seq[i]
      else:
        index = i
      plot += '\n(' + str(i) + ', ' + str(d[e][index]) + ')'
    plot += '\n};'
    plots += plot
  s = r'''\section*{\texttt{''' + filename + r'''}}
\subsection*{'''+ subsection +r'''}

\begin{tikzpicture}
\begin{axis}
[
width=24cm,
height=12cm,
enlarge x limits = false,
enlarge y limits = false,
title = {''' + col + ' vs. ' + col + ''' lines on ''' + cold +'''},
axis x line = bottom,
axis y line = left,
legend entries = {''' + legend + '''},
legend style ={at={(0.01,0.99)}, anchor=north west},
xlabel = {''' + xlabel + r'''},
ylabel = {''' + cold + r'''}
]''' + plots + r''' 
\end{axis}
\end{tikzpicture}'''
  print(s)

def process_file_line(filename, arg):
  # For line
  data = json.load(open(filename))
  args = arg.split(',')
  col = args[0]
  cold = args[1]
  col_i = data['fields'].index(col)
  cold_i = data['fields'].index(cold)
  inputs = data['inputs']
  inputs.remove(col_i)
  inputs_s = [data['fields'][e] for e in inputs]
  d = {}
  for i in range(0, len(data[col])):
      d[i] = []
  for row in data['results']:
    d[row[col_i]].append(row[cold_i])
  print_line(filename, d, data[col], inputs_s, col, cold, [])

def process_file_cactus(filename, arg):
  # For cactus
  data = json.load(open(filename))
  args = arg.split(',')
  col = args[0]
  ref = args[1]
  cold = args[2]
  col_i = data['fields'].index(col)
  cold_i = data['fields'].index(cold)
  ref_i = data[col].index(ref)
  inputs = data['inputs']
  inputs.remove(col_i)
  inputs_s = [data['fields'][e] for e in inputs]
  d = {}
  for i in range(0, len(data[col])):
      d[i] = []
  n = 0
  for row in data['results']:
    if row[col_i] == ref_i:
      d[ref_i].append([row[cold_i], n])
      n += 1
    else:
      d[row[col_i]].append(row[cold_i])
  ref_sorted = d[ref_i][:]
  ref_sorted.sort(key=lambda x: x[0])
  sort_seq = []
  for e in ref_sorted:
    sort_seq.append(e[1])
  l = []
  for e in d[ref_i]:
    l.append(e[0])
  d[ref_i] = l
  print_line(filename, d, data[col], inputs_s, col, cold, sort_seq)
#====================================================================
#====================================================================




#====================================================================
#                          --cactus2                                #
#====================================================================
def print_bettercactus(filename, plots_l, tools, inputs_s, col, cold):
  subsection = 'Cactus comparison'
  xlabel = '' # x axis
  for i in range(0, len(inputs_s)):
    xlabel += latex_escape(inputs_s[i])
    if i != len(inputs_s)-1:
      xlabel += ' * '
  legend = '' # legend entries
  for i in range(0, len(tools)):
    legend += latex_escape(tools[i])
    if i != len(tools)-1:
      legend += ', '
  plots = ''
  for e in plots_l:
    plot = '\n\\addplot+[mark=none] coordinates{'
    for i in range(0, len(e)):
      plot += '\n(' + str(i) + ', ' + str(e[i]) + ')'
    plot += '\n};'
    plots += plot
  s = r'''\section*{\texttt{''' + filename + r'''}}
\subsection*{'''+ subsection +r'''}

\begin{tikzpicture}
\begin{axis}
[
width=24cm,
height=12cm,
enlarge x limits = false,
enlarge y limits = false,
title = {''' + col + ' vs. ' + col + ''' lines on ''' + cold +'''},
axis x line = bottom,
axis y line = left,
legend entries = {''' + legend + '''},
legend style ={at={(0.01,0.99)}, anchor=north west},
xlabel = {''' + xlabel + r'''},
ylabel = {''' + cold + r'''}
]''' + plots + r''' 
\end{axis}
\end{tikzpicture}'''
  print(s)

def process_file_bettercactus(filename, arg):
  data = json.load(open(filename))
  args = arg.split(',')
  col = args[0]
  ref = args[1]
  cold = args[2]
  col_i = data['fields'].index(col)
  cold_i = data['fields'].index(cold)
  ref_i = data[col].index(ref)
  inputs = data['inputs'][:]
  inputs.remove(col_i)
  inputs_s = [data['fields'][e] for e in inputs]
  d = {}
  for i in range(0, len(data[col])):
      d[i] = []
  n = 0
  for row in data['results']:
    if row[col_i] == ref_i:
      d[ref_i].append([row[cold_i], n])
      n += 1
    else:
      d[row[col_i]].append(row[cold_i])
  ref_sorted = d[ref_i][:]
  ref_sorted.sort(key=lambda x: x[0])
  sort_seq = []
  for e in ref_sorted:
    sort_seq.append(e[1])
  l = []
  for e in d[ref_i]:
    l.append(e[0])
  d[ref_i] = l
  # each array in plots corresponds to a tool
  plots = []
  for e in d:
    plot = []
    for i in range(0, len(d[e])):
      index = sort_seq[i]
      plot.append(d[e][index])
    plots.append(plot)
  # each array in correspond to a combination of inputs
  combs = []
  for i in range(0, len(plots[0])):
    comb = []
    for e in plots:
      comb.append(e[i])
    combs.append(comb)
  combs.sort()
  # each array in plots corresponds to a tool
  plots = []
  for i in range(0, len(combs[0])):
    plot = []
    for e in combs:
      plot.append(e[i])
    plots.append(plot)
  print_bettercactus(filename, plots, data[col], inputs_s, col, cold)
#====================================================================
#====================================================================




#==============================================================================
#                                   MAIN                                      #
#==============================================================================
def main():
  p = argparse.ArgumentParser(description="Generate some tables and graphs from json benches files",
                              epilog="Report bugs to <spot@lrde.epita.fr>")
  p.add_argument('filenames',
                 help="name of the JSON file to read",
                 nargs='+')
  p.add_argument('--only',
                 help="skip the preamble",
                 nargs='?')
  p.add_argument('--intro',
                 help="introductory text for the LaTeX output",
                 default='')
  p.add_argument('--cumul',
                 help="Ex: model@90,tool;type:states[minmax/]",
                 default='')
  p.add_argument('--cumulslim',
                 help="Ex: model@90,tool;type:[minmax/]",
                 default='')
  p.add_argument('--cross',
                 help='col,"x[states]>y[states]"',
                 default='')
  p.add_argument('--crosslite',
                 help='col,"x[states]>y[states]"',
                 default='')
  p.add_argument('--crosscases',
                 help='col,"pred",val1,val2',
                 default='')
  p.add_argument('--ybar',
                 help='input',
                 default='')
  p.add_argument('--ybarstack',
                 help='input',
                 default='')
  p.add_argument('--scatter',
                 help='col,tool1,tool2,cold,colf',
                 default='')
  p.add_argument('--line',
                 help='col,cold',
                 default='')
  p.add_argument('--cactus',
                 help='col,tool,cold',
                 default='')
  p.add_argument('--cactus2',
                 help='col,tool,cold',
                 default='')
 
  args = p.parse_args()

  preamble = r'''\documentclass[landscape]{article}
\usepackage{adjustbox}
\usepackage{tikz}
\usepackage{pgfplots}
\usepackage{geometry}
\geometry{ hmargin=0.5cm, vmargin=1.5cm }
\usepackage{array}
\usepackage[utf8]{inputenc}
\usepackage[T1]{fontenc}
\usepackage[frenchb]{babel}
\usepackage{multirow}
\usepackage{longtable}
\usepackage{colortbl}
\usepackage{color}
\usepackage[boldmath]{numprint}
\def\B{\npboldmath}
\def\min{\npboldmath\cellcolor{white}}
\def\max{\npboldmath\cellcolor{white}}


\newcolumntype{R}[2]{%
    >{\adjustbox{angle=#1,lap=\width-(#2)}\bgroup}%
    l%
    <{\egroup}%
}
\newcommand*\rot{\multicolumn{1}{R{45}{1em}}}

\begin{document}
'''
  fulltex = "--only" not in sys.argv and not args.crosscases 
  if len(sys.argv) > 3:
    if fulltex:
      print(preamble)
    if args.intro:
      print(args.intro)
    for filename in args.filenames:
      if args.cumul:
        process_file_cumul(filename, args.cumul, False)
      if args.cumulslim:
        process_file_cumul(filename, args.cumulslim, True)
      if args.cross:
        process_file_cross(filename, args.cross, False)
      if args.crosslite:
        process_file_cross(filename, args.crosslite, True)
      if args.crosscases:
        process_file_crosscases(filename, args.crosscases)
      if args.ybar:
        process_file_ybar(filename, args.ybar, False)
      if args.ybarstack:
        process_file_ybar(filename, args.ybarstack, True)
      if args.scatter:
        process_file_scatter(filename, args.scatter)
      if args.line:
        process_file_line(filename, args.line)
      if args.cactus:
        process_file_cactus(filename, args.cactus)
      if args.cactus2:
        process_file_bettercactus(filename, args.cactus2)
    if fulltex:
      print("\\end{document}")
  else:
    raise Exception('error: too few arguments')
#==============================================================================
#==============================================================================

main()
