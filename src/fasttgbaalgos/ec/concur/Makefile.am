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

AM_CPPFLAGS = -I$(srcdir)/../../.. -I .. -I$(top_builddir)/src \
	 -Wall -Werror -std=gnu99 -lpthread -g -DENABLE_TRACE
AM_CXXFLAGS = $(WARNING_CXXFLAGS)

concurdir = $(pkgincludedir)/fasttgbaalgos/ec/concur

concur_HEADERS =    \
   MurmurHash3.h    \
   atomics.h	    \
   context.h	    \
   dynamic-array.h  \
   fast_hash.h	    \
   feedback.h	    \
   hashtable.h	    \
   internal.h	    \
   provider.h	    \
   queue.h	    \
   runtime.h	    \
   unionfind.h	    \
   user.h	    \
   uf.hh

#   fast_set.h
#   util.h
#   unix.h
#   git_version.h
#  table.h
#    balloc.h

  # MurmurHash3.h \
  # atomics.h \
  # balloc.h \
  # config.h \
  # context.h \
  # dynamic-array.h \
  # fast_hash.h \
  # hashtable.h \
  # internal.h \
  # provider.h \
  # runtime.h \
  # unionfind.h \
  # user.h

noinst_LTLIBRARIES = libconcur.la
libconcur_la_SOURCES = \
   MurmurHash3.c         \
   dynamic-array.c	 \
   fast_hash.c		 \
   hashtable.c		 \
   hre_context.c	 \
   hre_malloc.c		 \
   hre_runtime.c	 \
   unionfind.c

#    hre_queue.c
#    fast_set.c
#    hre_messaging.c
#    hre_pthread.c
#    hre_timer.c
#   util.c
#   hre_feedback.c
#   hre_unix.c
#    hre_utils.c
#   hre_reduce.c
#    balloc.c
#    hre_main.c
  # MurmurHash3.c \
  # balloc.c \
  # dynamic-array.c \
  # fast_hash.c \
  # hashtable.c \
  # hre_context.c \
  # hre_malloc.c \
  # hre_reduce.c \
  # hre_runtime.c \
  # unionfind.c
