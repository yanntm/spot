// -*- coding: utf-8 -*-
// Copyright (C) 2015, 2016 Laboratoire de Recherche et Développement
// de l'Epita.
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.


#include <iostream>
#include <spot/twacube/cube.hh>

int main()
{
  std::vector<std::string> aps = {"a", "b", "c", "d", "e"};
  spot::cubeset cs(aps.size());
  spot::cube mc = cs.alloc();
  cs.set_true_var(mc, 0);
  cs.set_false_var(mc, 3);
  std::cout << "size : " << cs.size() << '\n';
  std::cout << "cube : " << cs.dump(mc, aps) << '\n';
  std::cout << "valid : " << cs.is_valid(mc) << '\n';
  std::cout << "intersect(c,c) : " << cs.intersect(mc, mc) << '\n';

  spot::cube mc1 = cs.alloc();
  cs.set_false_var(mc1, 0);
  cs.set_true_var(mc1, 1);
  std::cout << "size : " << cs.size() << '\n';
  std::cout << "cube : " << cs.dump(mc1, aps) << '\n';
  std::cout << "valid : " << cs.is_valid(mc1) << '\n';
  std::cout << "intersect(c1,c1) : " << cs.intersect(mc1, mc1) << '\n';
  std::cout << "intersect(c,c1) : " << cs.intersect(mc, mc1) << '\n';
  std::cout << "intersect(c1,c) : " << cs.intersect(mc1, mc) << '\n';

  spot::cube mc2 = cs.alloc();
  cs.set_true_var(mc2, 1);
  cs.set_true_var(mc2, 3);
  std::cout << "size : " << cs.size() << '\n';
  std::cout << "cube : " << cs.dump(mc2, aps) << '\n';
  std::cout << "valid : " << cs.is_valid(mc2) << '\n';
  std::cout << "intersect(c2,c1) : " << cs.intersect(mc1, mc2) << '\n';
  std::cout << "intersect(c2,c) : " << cs.intersect(mc, mc2) << '\n';

  cs.release(mc2);
  cs.release(mc1);
  cs.release(mc);
}
