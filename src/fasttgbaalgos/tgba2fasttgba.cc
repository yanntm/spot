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

#include "tgba2fasttgba.hh"
#include "tgbaalgos/reachiter.hh"

namespace spot
{
  namespace
  {
    /// This internal class acts like a converter from a TGBA to a
    /// FASTTGBA. It walk all transitions and states of the original
    /// tgba and create the associated fasttgba.
    class converter_bfs : public tgba_reachable_iterator_breadth_first
    {
    public:
      converter_bfs(const tgba* a)
	: tgba_reachable_iterator_breadth_first(a)
      {
	assert (a != 0);
      }

      void
      start()
      {
	std::cout  << "start" << std::endl;
      }

      void
      end()
      {
	std::cout  << "end" << std::endl;
      }

      void
      process_state(const state* , int , tgba_succ_iterator* )
      {
	std::cout  << "Process state" << std::endl;
      }

      void
      process_link(const state* , int ,
		   const state* , int ,
		   const tgba_succ_iterator* )
      {
	std::cout  << "Process Link" << std::endl;
      }

      const fasttgba*
      result ()
      {
	return result_;
      }

    private:
      const spot::fasttgba *result_;
    };
  }

  const fasttgba*
  tgba_2_fasttgba(const spot::tgba* a)
  {
    converter_bfs cb_(a);
    cb_.run();
    return cb_.result();
  }

}
