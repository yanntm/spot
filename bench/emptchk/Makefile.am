## Copyright (C) 2005  Laboratoire d'Informatique de Paris 6 (LIP6),
## département Systèmes Répartis Coopératifs (SRC), Université Pierre
## et Marie Curie.
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


PML2TGBA = $(PERL) $(srcdir)/pml2tgba.pl

noinst_SCRIPTS = defs
dist_noinst_SCRIPTS = \
  pml2tgba.pl \
  ltl-human.sh \
  ltl-random.sh \
  pml-clserv.sh \
  pml-eeaean.sh

dist_noinst_DATA = \
  models/cl3serv1.pml \
  models/cl3serv3.pml \
  models/clserv.ltl \
  models/eeaean1.pml \
  models/eeaean2.pml \
  models/eeaean.ltl \
  formulae.ltl \
  algorithms

nodist_noinst_DATA = \
  models/cl3serv1.tgba \
  models/cl3serv1R.tgba \
  models/cl3serv1fair.tgba \
  models/cl3serv1Rfair.tgba \
  models/cl3serv3.tgba \
  models/cl3serv3R.tgba \
  models/cl3serv3fair.tgba \
  models/cl3serv3Rfair.tgba \
  models/eeaean1.tgba \
  models/eeaean1R.tgba \
  models/eeaean2.tgba \
  models/eeaean2R.tgba

models/cl3serv1.tgba: $(srcdir)/models/cl3serv1.pml
	$(mkdir_p) models
	cp $(srcdir)/models/cl3serv1.pml cl3serv1.pml
	$(PML2TGBA) cl3serv1.pml w1 s1 >$@
	rm cl3serv1.pml

models/cl3serv1R.tgba: $(srcdir)/models/cl3serv1.pml
	$(mkdir_p) models
	cp $(srcdir)/models/cl3serv1.pml cl3serv1.pml
	$(PML2TGBA) -r $(srcdir)/cl3serv1.pml w1 s1 >$@
	rm cl3serv1.pml

models/cl3serv1fair.tgba: $(srcdir)/models/cl3serv1.pml
	$(mkdir_p) models
	cp $(srcdir)/models/cl3serv1.pml cl3serv1.pml
	$(PML2TGBA) -w $(srcdir)/cl3serv1.pml w1 s1 >$@
	rm cl3serv1.pml

models/cl3serv1Rfair.tgba: $(srcdir)/models/cl3serv1.pml
	$(mkdir_p) models
	cp $(srcdir)/models/cl3serv1.pml cl3serv1.pml
	$(PML2TGBA) -w -r $(srcdir)/cl3serv1.pml w1 s1 >$@
	rm cl3serv1.pml

models/cl3serv3.tgba: $(srcdir)/models/cl3serv3.pml
	$(mkdir_p) models
	cp $(srcdir)/models/cl3serv3.pml cl3serv3.pml
	$(PML2TGBA) $(srcdir)/cl3serv3.pml w1 s1 >$@
	rm cl3serv3.pml

models/cl3serv3R.tgba: $(srcdir)/models/cl3serv3.pml
	$(mkdir_p) models
	cp $(srcdir)/models/cl3serv3.pml cl3serv3.pml
	$(PML2TGBA) -r $(srcdir)/cl3serv3.pml w1 s1 >$@
	rm cl3serv3.pml

models/cl3serv3fair.tgba: $(srcdir)/models/cl3serv3.pml
	$(mkdir_p) models
	cp $(srcdir)/models/cl3serv3.pml cl3serv3.pml
	$(PML2TGBA) -w $(srcdir)/cl3serv3.pml w1 s1 >$@
	rm cl3serv3.pml

models/cl3serv3Rfair.tgba: $(srcdir)/models/cl3serv3.pml
	$(mkdir_p) models
	cp $(srcdir)/models/cl3serv3.pml cl3serv3.pml
	$(PML2TGBA) -w -r $(srcdir)/cl3serv3.pml w1 s1 >$@
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

CLEANFILES = $(nodist_noinst_DATA)
