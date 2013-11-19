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

#ifndef SPOT_FASTTGBAALGOS_EC_CONCUREC_CONCUR_EC_STAT_HH
# define SPOT_FASTTGBAALGOS_EC_CONCUREC_CONCUR_EC_STAT_HH

#include "fasttgbaalgos/ec/ec.hh"
#include <string>

namespace spot
{
  class concur_ec_stat : public ec
  {
  public:

    virtual bool has_counterexample() = 0;

    virtual std::string csv() = 0;

    virtual std::chrono::milliseconds::rep get_elapsed_time() = 0;

    virtual int nb_inserted() = 0;
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_CONCUREC_CONCUR_EC_STAT_HH
