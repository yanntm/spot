## Copyright (C) 2009, 2010, 2011, 2012 Laboratoire de Recherche et
## Développement de l'Epita (LRDE).
## Copyright (C) 2003, 2004, 2005, 2006 Laboratoire d'Informatique de
## Paris 6 (LIP6), département Systèmes Répartis Coopératifs (SRC),
## Université Pierre et Marie Curie.
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

AM_CPPFLAGS = -I$(srcdir)/.. -I.. $(BUDDY_CPPFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS)
LDADD = ../libspot.la

# These are the most used test programs, and they are also useful
# to run manually outside the test suite.  Always build them.
noinst_PROGRAMS =

check_SCRIPTS = defs
# Keep this sorted alphabetically.
check_PROGRAMS = tgba_converter formula_product

# Keep this sorted alphabetically.
tgba_converter_SOURCES = tgba_converter.cc
formula_product_SOURCES = formula_product.cc

# Keep this sorted by STRENGTH.  Test basic things first,
# because such failures will be easier to diagnose and fix.
# TESTS =

# EXTRA_DIST = $(TESTS)

# distclean-local:
# 	rm -rf $(TESTS:.test=.dir)
