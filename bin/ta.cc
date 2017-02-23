// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et Développement de l'Epita.
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

#include "common_sys.hh"

#include <spot/zgastwa/zgastwa.hh>
#include <spot/twaalgos/dot.hh>

#include <utap2tchk-predef.hh>
#include <base/labelset.hh>
#include <semantics/semantics.hh>
#include <semantics/zg.hh>
#include <syntax/model.hh>
#include <syntax/system_builder.hh>

using namespace utap2tchk;

// TAKEN FROM TCHECKER
#ifndef TCHECKER_UPPAAL
/*
 * Function to build a system
*/
extern void build_model(syntax::system_t& system);

static syntax::extern_system_builder_t<build_model> tchecker_system_builder;
#else
static uppaal::system_builder_t tchecker_system_builder;
#endif

int main(int argc, char* argv[])
{
  argc = argc;
  argv = argv;
  labels_t label;
  syntax::model_t* model =
       syntax::model_factory_t::instance().new_model(tchecker_system_builder,
                                                     label);
  //auto a = spot::zg_as_twa(spot::make_bdd_dict(), model);
  auto a = std::make_shared<spot::zg_as_twa>(spot::make_bdd_dict(), *model);
  spot::print_dot(std::cout, a);
  return 0;
}
