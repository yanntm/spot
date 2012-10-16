## Copyright (C) 2012 Laboratoire de Recherche et de Developpement Epita.
##
## This file is part of Spot, a model checking library.
##
## Spot is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## Spot is distributed in the hope that it will be useful, but WITHOUT
## ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
## or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
## License for more details.
##
## You should have received a copy of the GNU General Public License
## along with Spot; see the file COPYING.  If not, write to the Free
## Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
## 02111-1307, USA.

tlcheckdir = $(pkgincludedir)/iface/tlcheck

AM_CPPFLAGS = -I$(srcdir)/../../src $(BUDDY_CPPFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS)
LDADD = ../../src/libspot.la ../../iface/dve2/libspotdve2.la

noinst_PROGRAMS =  tlcheck

tlcheck_SOURCES = tlcheck.cc

#noinst_SCRIPTS = defs

# dist_noinst_DATA = \
#   allbenchs.txt \
#   models/anderson.4.dve \
#   models/at.4.dve \
#   models/bakery.4.dve \
#   models/bopdp.3.dve \
#   models/brp2.3.dve \
#   models/elevator.4.dve \
#   models/elevator2.3.dve \
#   models/gear.2.dve \
#   models/leader-election.3.dve \
#   models/lifts.4.dve \
#   models/production-cell.3.dve \
#   models/sorter.2.dve \
#   models/telephony.2.dve


CLEANFILES =  *.dve2C  *.dve.cpp
