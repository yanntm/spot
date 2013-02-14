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

#include <iostream>
#include <sstream>
#include "lbtt_dfs.hh"
#include "fasttgba/fasttgbaexplicit.hh"

namespace spot
{
  lbtt_dfs::lbtt_dfs(const fasttgba* a):
   generic_dfs(a), nb_states(0), nb_transitions(0)
  { }

  lbtt_dfs::~lbtt_dfs()
  {

  }


  bool
  lbtt_dfs::want_state(const fasttgba_state*) const
  {
    return true;
  }

  void
  lbtt_dfs::start()
  {
  }

  void
  lbtt_dfs::end()
  {
    oss << "-1" << std::endl;
    std::cout << nb_states << " "
	      << a_->number_of_acceptance_marks() << "t"
	      << std::endl;
    std::cout << oss.str();
  }

  void
  lbtt_dfs::process_state(const fasttgba_state*, int in,
			   fasttgba_succ_iterator*)
  {
    ++nb_states;
    if (in == 1)
      oss << (in - 1) << " 1" << std::endl;
    else
      {
	oss << "-1" << std::endl;
	oss << (in - 1) << " 0" << std::endl;
      }
  }

  void
  lbtt_dfs::process_link(const fasttgba_state* , int,
			  const fasttgba_state* , int out,
			  const fasttgba_succ_iterator* t)
  {
    ++nb_transitions;

    // Grab acceptances that are set
    markset m = t->current_acceptance_marks();
    std::ostringstream os;
    int i = 0;
    while (!m.empty())
      {
	mark one = m.one();
	m -= one;
	os << " " << i ;
	++i;
      }

    if (t->current_condition().dump().compare("1") == 0)
      oss << (out -1) // cause we number to 1
	  << os.str() << " -1 " << "t"
	  << std::endl;
    else
      oss << (out -1) // cause we number to 1
	  << os.str() << " -1 "
	  << ((t->current_condition().size() > 1) ? " & " : "" )
	  << t->current_condition().dump()
	  << std::endl;
  }
}
