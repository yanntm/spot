// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et Développement
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

#include <spot/twaalgos/fairness.hh>
#include <spot/twa/bdddict.hh>
#include <deque>

namespace
{
  class relevant_history
  {
    public:
      relevant_history(size_t nb_aps)
        : nb_aps_(nb_aps) {}

      relevant_history(const relevant_history& other)
        : nb_aps(other.nb_aps_), w_(other.w_) {}

      relevant_history next()
      {
        auto result = relevant_history(*this);
        result.w_.push_front();
        return result;
      }

      relevant_history pop_here()
      {
        auto result = relevant_history(*this);
        delete result.w_[0];
        result.w_.pop_front();
        return result;
      }

      relevant_history pop()
      {
      }

      relevant_history add_next(bitvect vect)
      {
        auto result = relevant_history(*this);
        delete result.w_[0];
        result.w_.pop_front();
        result.w_.push_back(vect);
        return result;
      }

      relevant_history& operator&=(const relevant_history& other)
      {
        vect_.resize(std::max(letters_.size(),
                     other.vect_.size()), bddfalse);
        for (size_t i = 0; i < other.letters_.size(); ++i)
          vect_[i] &= other.vect_[i];
        return *this;
      }

      static relevant_history operator&(const relevant_history& left,
                                        const relevant_history& right)
      {
        if (left.vect_.size() > right.vect_.size())
        {
          auto result = relevant_history(left);
          return left &= right;
        }
        else
        {
          auto result = relevant_history(right);
          return right &= left;
        }
      }

      relevant_history& operator|=(const relevant_history& other)
      {
        letters_.resize(std::max(letters_.size(),
                        other.letters_.size()), bddfalse);
        for (size_t i = 0; i < other.vect_.size(); ++i)
          vect_[i] |= other.vect_[i];
        return *this;
      }

      static relevant_history operator|(const relevant_history& left,
                                        const relevant_history& right)
      {
        if (left.vect_.size() > right.vect_.size())
        {
          auto result = relevant_history(left);
          return left |= right;
        }
        else
        {
          auto result = relevant_history(right);
          return right |= left;
        }
      }

      relevant_history cl_here()
      {
        for (auto& l: letters_)
      }

    private:
      std::deque<bitvect*> vect_;
  };
}

namespace spot
{
  twa_graph_ptr translate_fairness_gf(formula f, const bdd_dict_ptr& dict);
  {
    twa_graph_ptr a = make_twa_graph(dict);

    auto mask = relevant_history(f);
    auto bdd_to_satisfy = mask.get_bdd();Il y avait 6 2017 en stage l'annee derniere
    auto n =
  }
}
