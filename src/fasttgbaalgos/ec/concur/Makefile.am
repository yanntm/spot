## -*- coding: utf-8 -*-
## Copyright (C)  2013 Laboratoire de Recherche
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

AM_CPPFLAGS =  -Wall -Werror -std=gnu99
#AM_CXXFLAGS = $(WARNING_CXXFLAGS)

concurdir = $(pkgincludedir)/fasttgbaalgos/ec/concur

concur_HEADERS =    \
   MurmurHash3.h    \
   atomics.h	    \
   fast_hash.h	    \
   feedback.h	    \
   hashtable.h	    \
   runtime.h	    \
   unionfind.h	    \
   user.h	    \
   open_set.h	    \
   uf.hh            \
   openset.hh

noinst_LTLIBRARIES = libconcur.la
libconcur_la_SOURCES = \
   MurmurHash3.c         \
   fast_hash.c		 \
   hashtable.c		 \
   hre_malloc.c		 \
   unionfind.c           \
   open_set.c
