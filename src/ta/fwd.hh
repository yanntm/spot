// -*- coding: utf-8 -*-
// Copyright (C) 2014 Laboratoire de Recherche et Développement de
// l'Epita (LRDE).
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

#ifndef SPOT_TA_FWD_HH
# define SPOT_TA_FWD_HH

#include <memory>

namespace spot
{
  class ta;
  typedef std::shared_ptr<ta> ta_ptr;
  typedef std::shared_ptr<const ta> const_ta_ptr;

  class ta_digraph;
  typedef std::shared_ptr<const ta_digraph> const_ta_digraph_ptr;
  typedef std::shared_ptr<ta_digraph> ta_digraph_ptr;

  class ta_product;
  typedef std::shared_ptr<const ta_product> const_ta_product_ptr;
  typedef std::shared_ptr<ta_product> ta_product_ptr;
}

#endif // SPOT_TA_FWD_HH
