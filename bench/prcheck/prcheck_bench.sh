#!/usr/bin/env sh
# Copyright (C) 2017 Laboratoire de Recherche et Développement de
# l'Epita (LRDE).
#
# This file is part of Spot, a model checking library.
#
# Spot is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# Spot is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

randNb=1000
genNb=3

# Generate formulas
genltl --dac-patterns --eh-patterns --hkrss-patterns --p-patterns \
  --sb-patterns --and-fg=2..$genNb --and-gf=2..$genNb --ccj-alpha=2..$genNb \
  --ccj-beta-prime=2..$genNb --fxg-or=2..$genNb  --gh-q=2..$genNb \
  --gh-r=2..$genNb --go-theta=2..$genNb --gxf-and=2..$genNb \
  --ms-example=2..$genNb --ms-phi-h=2..$genNb \
  --ms-phi-s=2..$genNb  --or-fg=2..$genNb --or-gf=2..$genNb --r-left=2..$genNb \
  --r-right=2..$genNb  --rv-counter=2..$genNb --tv-f1=2..$genNb  \
  --tv-f2=2..$genNb --tv-g1=2..$genNb  --tv-g2=2..$genNb --tv-uu=2..$genNb \
  --u-left=2..$genNb --u-right=2..$genNb --neg --pos \
  | ltlfilt --unique > patterns

shuf patterns > tmp
mv tmp patterns

randltl -n -1 a b c | ltlfilt --unique | ltlfilt --safety \
  | ltlfilt --guarantee -n $randNb > tmp
randltl -n -1 a b c | ltlfilt --unique | ltlfilt -v --guarantee \
  | ltlfilt --safety -n $randNb >> tmp
randltl -n -1 a b c | ltlfilt --unique | ltlfilt -v --safety \
  | ltlfilt --guarantee -n $randNb >> tmp
randltl -n -1 a b c | ltlfilt --unique | ltlfilt -v --safety \
  | ltlfilt -v --guarantee | ltlfilt --obligation -n $randNb >> tmp
randltl -n -1 a b c | ltlfilt --unique | ltlfilt -v --obligation \
  | ltlfilt --recurrence -n $randNb >> tmp
randltl -n -1 a b c | ltlfilt --unique | ltlfilt -v --obligation \
  | ltlfilt --persistence -n $randNb >> tmp
randltl -n -1 a b c | ltlfilt --unique | ltlfilt -v --recurrence \
  | ltlfilt -v --persistence -n $randNb >> tmp

cat tmp | ltlfilt --neg > neg_tmp
cat neg_tmp >> tmp
cat tmp | ltlfilt --unique > random
rm tmp neg_tmp

shuf random > tmp
mv tmp random
rm tmp

# Run tests
RUN="../../tests/run"
make
$RUN ./prcheck_process.py
