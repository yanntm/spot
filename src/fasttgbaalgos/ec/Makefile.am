## -*- coding: utf-8 -*-
## Copyright (C)  2012 Laboratoire de Recherche
## et Développement de l'Epita (LRDE).
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

AM_CPPFLAGS = -I$(srcdir)/../.. -I.. -I../..
AM_CXXFLAGS = $(WARNING_CXXFLAGS)

ecdir = $(pkgincludedir)/fasttgbaalgos/ec

ec_HEADERS = \
  ec.hh \
  cou99.hh \
  cou99_uf.hh \
  cou99strength.hh \
  cou99strength_uf.hh \
  cou99_uf_swarm.hh \
  cou99_uf_shared.hh \
  gv04.hh \
  lazycheck.hh \
  root_stack.hh \
  stackscheck.hh \
  union_find.hh \
  union_find_shared.hh

noinst_LTLIBRARIES = libec.la
libec_la_SOURCES = \
  cou99.cc \
  cou99_uf.cc \
  cou99strength.cc \
  cou99strength_uf.cc \
  cou99_uf_swarm.cc \
  cou99_uf_shared.cc \
  gv04.cc \
  lazycheck.cc \
  stackscheck.cc \
  union_find.cc \
  union_find_shared.cc
