// Copyright (C) 2014 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
// Copyright (C) 2003, 2004 Laboratoire d'Informatique de Paris 6 (LIP6),
// département Systèmes Répartis Coopératifs (SRC), Université Pierre
// et Marie Curie.
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

#include <fstream>
#include <vector>
#include "tgbaalgos/fas.hh"
#include "tgbaparse/public.hh"
#include "tgbaalgos/dotty.hh"
#include "tgbaalgos/dottydec.hh"
#include "graph/bidigraph.hh"
#include "tgba/bdddict.hh"

namespace spot
{
  namespace
  {
    class fas_dotty_decorator : public dotty_decorator
    {
    public:
      std::string
      link_decl(const tgba*, const state* in_s, int, const state* out_s, int,
                                 const tgba_succ_iterator*,
                                 const std::string&)
      {
        if (is_fas)
          {
            if ((*is_fas)(in_s, out_s))
              return "[color=\"red\"]";
          }
        return "[]";
      }

      static fas_dotty_decorator* instance()
      {
        static fas_dotty_decorator d;
        return &d;
      }

      static spot::fas* is_fas;
    };
  }

  spot::fas* spot::fas_dotty_decorator::is_fas = nullptr;
}

int main(int argc, char* argv[])
{
  argc = argc;
  if (argc <= 1)
    {
      std::cerr << "./fas [spot_file]" << std::endl;
      return 1;
    }

  spot::tgba_parse_error_list pel;
  spot::bdd_dict* dict = new spot::bdd_dict();
  spot::tgba* g = spot::tgba_parse(argv[1], pel, dict);
  if (spot::format_tgba_parse_errors(std::cerr, argv[1], pel))
    {
      delete dict;
      return 2;
    }

  spot::fas* is_fas = new spot::fas(g);

  spot::fas_dotty_decorator::is_fas = is_fas;
  spot::dotty_reachable(std::cout, g, false,
                        spot::fas_dotty_decorator::instance());

  delete is_fas;
  delete g;
  delete dict;
}
