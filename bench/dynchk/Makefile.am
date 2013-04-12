## Copyright (C) 2005  Laboratoire d'Informatique de Paris 6 (LIP6),
## département Systèmes Répartis Coopératifs (SRC), Université Pierre
## et Marie Curie.
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


PML2TGBA = $(PERL) $(srcdir)/pml2tgba.pl

AM_CPPFLAGS = -I$(srcdir)/../../src $(BUDDY_CPPFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS)
LDADD = ../../src/libspot.la ../../iface/dve2/libspotdve2.la

dist_noinst_SCRIPTS = \
  pml2tgba.pl

noinst_PROGRAMS = \
  dynchk \
  gen

dynchk_SOURCES = dynchk.cc
gen_SOURCES = gen.cc

noinst_SCRIPTS = defs

dist_noinst_DATA = \
  models/cl3serv1.pml \
  models/cl3serv3.pml \
  models/eeaean1.pml \
  models/eeaean2.pml \
  algorithms

nodist_noinst_DATA = \
  models/cl3serv1.tgba \
  models/cl3serv1R.tgba \
  models/cl3serv3.tgba \
  models/cl3serv3R.tgba \
  models/eeaean1.tgba \
  models/eeaean1R.tgba \
  models/eeaean2.tgba \
  models/eeaean2R.tgba \
  dynchk \
  gen

models/cl3serv1.tgba: $(srcdir)/models/cl3serv1.pml
	$(mkdir_p) models
	cp $(srcdir)/models/cl3serv1.pml cl3serv1.pml
	$(PML2TGBA) cl3serv1.pml w1 s1 w2 s2 w3 s3 >$@
	rm cl3serv1.pml

models/cl3serv1R.tgba: $(srcdir)/models/cl3serv1.pml
	$(mkdir_p) models
	cp $(srcdir)/models/cl3serv1.pml cl3serv1.pml
	$(PML2TGBA) -r $(srcdir)/cl3serv1.pml w1 s1 w2 s2 w3 s3 >$@
	rm cl3serv1.pml

models/cl3serv3.tgba: $(srcdir)/models/cl3serv3.pml
	$(mkdir_p) models
	cp $(srcdir)/models/cl3serv3.pml cl3serv3.pml
	$(PML2TGBA) $(srcdir)/cl3serv3.pml w1 s1 w2 s2 w3 s3 >$@
	rm cl3serv3.pml

models/cl3serv3R.tgba: $(srcdir)/models/cl3serv3.pml
	$(mkdir_p) models
	cp $(srcdir)/models/cl3serv3.pml cl3serv3.pml
	$(PML2TGBA) -r $(srcdir)/cl3serv3.pml w1 s1 w2 s2 w3 s3 >$@
	rm cl3serv3.pml

models/eeaean1.tgba: $(srcdir)/models/eeaean1.pml
	$(mkdir_p) models
	cp $(srcdir)/models/eeaean1.pml eeaean1.pml
	$(PML2TGBA) $(srcdir)/eeaean1.pml \
	  noLeader zeroLeads oneLeads twoLeads threeLeads >$@
	rm eeaean1.pml

models/eeaean1R.tgba: $(srcdir)/models/eeaean1.pml
	$(mkdir_p) models
	cp $(srcdir)/models/eeaean1.pml eeaean1.pml
	$(PML2TGBA) -r $(srcdir)/eeaean1.pml \
	  noLeader zeroLeads oneLeads twoLeads threeLeads >$@
	rm  eeaean1.pml

models/eeaean2.tgba: $(srcdir)/models/eeaean2.pml
	$(mkdir_p) models
	cp $(srcdir)/models/eeaean2.pml eeaean2.pml
	$(PML2TGBA) $(srcdir)/eeaean2.pml \
	  noLeader zeroLeads oneLeads twoLeads threeLeads >$@
	rm eeaean2.pml

models/eeaean2R.tgba: $(srcdir)/models/eeaean2.pml
	$(mkdir_p) models
	cp $(srcdir)/models/eeaean2.pml eeaean2.pml
	$(PML2TGBA) -r $(srcdir)/eeaean2.pml \
	  noLeader zeroLeads oneLeads twoLeads threeLeads >$@
	rm eeaean2.pml


CLEANFILES = $(nodist_noinst_DATA)   *.dve2C  *.dve.cpp
