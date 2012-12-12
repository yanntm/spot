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

#ifndef SPOT_FASTTGBA_FASTTGBAEXPLICIT_HH
# define SPOT_FASTTGBA_FASTTGBAEXPLICIT_HH

#include "fasttgba.hh"

namespace spot
{

  /// This class is the concrete implementation of the fasttgba
  /// interface. Moreover this interface provides all what is needed
  /// to build such an automaton
  class fasttgbaexplicit : public fasttgba
  {
  public:

    // The FASTTGBA interface

    fasttgbaexplicit(int num_acc, std::vector<std::string> aps);

    virtual ~fasttgbaexplicit();

    virtual faststate* get_init_state() const;

    virtual fasttgba_succ_iterator*
    succ_iter(const faststate* local_state) const;

    virtual
    std::vector<std::string> get_dict() const;

    virtual
    std::string format_state(const faststate* state) const;

    virtual std::string
    transition_annotation(const fasttgba_succ_iterator* t) const;

    virtual faststate* project_state(const faststate* s,
				     const fasttgba* t) const;
    virtual
    boost::dynamic_bitset<> all_acceptance_conditions() const;

    virtual
    unsigned int number_of_acceptance_conditions() const;

    virtual
    boost::dynamic_bitset<> neg_acceptance_conditions() const;

    // This part is for creating a new FASTTGBAEXPLICIT
  protected:
    boost::dynamic_bitset<> all_cond_;
    boost::dynamic_bitset<> all_cond_neg_;
    std::vector<std::string> aps_;
  };
}

#endif // SPOT_FASTTGBA_FASTTGBAEXPLICIT_HH
