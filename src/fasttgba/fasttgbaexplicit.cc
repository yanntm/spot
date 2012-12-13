// Copyright (C) 2012 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
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


#include "fasttgbaexplicit.hh"

namespace spot
{

  fasttgbaexplicit::fasttgbaexplicit(int acc_num, std::vector<std::string> aps):
    all_cond_ (acc_num),
    all_cond_neg_ (acc_num)

  {
    num_acc_ = acc_num;

    // Allocate the bitset and fix all_cond to avoid
    // multiple computations
    all_cond_neg_ = ~all_cond_;
    aps_ = aps;
  }

  fasttgbaexplicit::~fasttgbaexplicit()
  {
  }

  faststate*
  fasttgbaexplicit::get_init_state() const
  {
    assert(false);
  }

  fasttgba_succ_iterator*
  fasttgbaexplicit::succ_iter(const faststate*) const
  {
    assert(false);
  }

  std::vector<std::string>
  fasttgbaexplicit::get_dict() const
  {
    return aps_;
  }

  std::string
  fasttgbaexplicit::format_state(const faststate*) const
  {
    assert(false);
  }


  std::string
  fasttgbaexplicit::transition_annotation(const fasttgba_succ_iterator*) const
  {
    assert(false);
  }

  faststate*
  fasttgbaexplicit::project_state(const faststate* ,
				  const fasttgba*) const
  {
    assert(false);
  }

  cube
  fasttgbaexplicit::all_acceptance_conditions() const
  {
    return all_cond_;
  }

  unsigned int
  fasttgbaexplicit::number_of_acceptance_conditions() const
  {
    return num_acc_;
  }

  cube
  fasttgbaexplicit::neg_acceptance_conditions() const
  {
    return all_cond_neg_;
  }

  bool
  fasttgbaexplicit::add_state(int s)
  {
    if (state_map_.find(s) == state_map_.end())
      {
	state_map_.insert(std::make_pair (s, fast_explicit_state(s)));
	return false;
      }
    return true;
  }

  void
  fasttgbaexplicit::add_transition(faststate*, faststate*,
				   cube, cube)
  {
    assert(false);
  }

}
