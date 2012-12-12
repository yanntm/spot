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

#include <string>
#include "tgba2fasttgba.hh"
#include "fasttgba/fasttgbaexplicit.hh"
#include "tgbaalgos/reachiter.hh"
#include "ltlast/formula.hh"
#include "ltlast/atomic_prop.hh"
#include "tgba/bdddict.hh"

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
	//assert (a != 0);
      }

      void
      start()
      {
	// Here this method is used to grab main values needed
	// to construct a fasttgba

	// First grab Atomic propositions
	bdd_dict* aps = aut_->get_dict();
	std::map<const ltl::formula*, int> var_map = aps->var_map;
	std::map<const ltl::formula*, int>::iterator sii =
	  var_map.begin();
	std::map<const ltl::formula*, int>::iterator end =
	  var_map.end();

	std::vector<std::string> ap_dict ;
	for(; sii != end; ++sii)
	  {
	    const ltl::formula *f = (*sii).first;
	    std::cout << ((const ltl::atomic_prop*)f)->name() << std::endl;
	    ap_dict.push_back(((const ltl::atomic_prop*)f)->name());
	  }


	// Here initialize the fasttgba
	result_ =
	  new fasttgbaexplicit (aut_->number_of_acceptance_conditions(), ap_dict);
      }

      void
      end()
      {
	std::cout  << "end" << std::endl;
      }

      void
      process_state(const state* , int s , tgba_succ_iterator* )
      {
	std::cout  << "Process state : " << s << std::endl;
      }

      void
      process_link(const state* , int src,
		   const state* , int dst,
		   const tgba_succ_iterator* )
      {
	std::cout  << "Process Link " << src << " -> " << dst << std::endl;
      }

      const fasttgba*
      result ()
      {
	return result_;
      }

    private:
      const fasttgbaexplicit *result_;
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
