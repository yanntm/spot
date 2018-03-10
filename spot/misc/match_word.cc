// -*- coding: utf-8 -*-
// Copyright (C) 2011-2018 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
// Copyright (C) 2003, 2004, 2006 Laboratoire d'Informatique de Paris
// 6 (LIP6), département Systèmes Répartis Coopératifs (SRC),
// Université Pierre et Marie Curie.
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

#include <config.h>
#include <spot/misc/match_word.hh>

namespace spot
{
  namespace
  {
    // recursive function called by get_vector_formula to recursively fill one
    // set of the result vector.
    // This function makes sure we start with a vector containing only single
    // formulas.
    void
    rec_fill_set(std::set<formula>& set, formula f)
    {
      if (!f.is_literal())
        {
          for (auto iter_f = f.begin(); iter_f != f.end(); ++iter_f)
            rec_fill_set(set, *iter_f);
        }
      else if (f.kind() != op::Not)
        set.insert(f);
    }

    // creates the vector that will be manipulated from the twa_word
    // is_prefix is used to proccess the prefix or the suffix at will
    std::vector<std::set<formula>>
    get_vector_formula(twa_word_ptr w, bool is_prefix)
    {
      std::vector<std::set<formula>> result;
      std::list<bdd> list_bdd = is_prefix ? w->prefix : w->cycle;
      for (auto iter_list = list_bdd.begin();
          iter_list != list_bdd.end(); ++iter_list)
        {
          std::set<formula> set;
          rec_fill_set(set, bdd_to_formula(*iter_list, w->get_dict()));
          result.push_back(set);
        }
      return result;
    }

    //function called on cycle and prefix
    auto
    handle_until_loop(std::vector<std::set<formula>>& vec, const formula& f,
        op operation, const formula& f0, const formula& f1)
    {
      auto iter_mark = vec.end();
      //loop around vec
      //we suppose the test for aUb
      for (auto iter_vec = vec.begin(); iter_vec != vec.end(); ++iter_vec)
        {
          if (iter_vec->find(f1) != iter_vec->end())
            {
              if ((operation == op::U || operation == op::W)
                  || iter_mark->find(f0) != iter_mark->end())
                {
                  //if found b, loop from the mark to the current
                  //position and insert
                  for (; iter_mark != vec.end()
                      && iter_mark != iter_vec; ++iter_mark)
                    iter_mark->insert(f);
                  iter_vec->insert(f);
                }
              //reset the mark
              iter_mark = vec.end();
            }
          else if (iter_vec->find(f0) != iter_vec->end())
            {
              //if found a for the first time, set a var to remember
              //if found for another time a, nothing
              if (iter_mark == vec.end())
                iter_mark = iter_vec;
            }
          else
            {
              //if different from a and b, go reset var to vec.end()
              if (iter_mark != vec.end())
                iter_mark = vec.end();
            }
        }
      //test case if condition circles around the vec
      return iter_mark;
    }

    //function to handle U, W, R, M operators
    //just pass the operation by argument
    void
    handle_until(std::vector<std::set<formula>>& prefix,
        std::vector<std::set<formula>>& cycle, const formula& f, op operation)
    {
      formula f0;
      formula f1;
      if (operation == op::R || operation == op::M)
        {
          f0 = f[1];
          f1 = f[0];
        }
      else
        {
          f0 = f[0];
          f1 = f[1];
        }
      auto mark_cycle = handle_until_loop(cycle, f, operation, f0, f1);
      if (mark_cycle != cycle.end())
        {
          if (cycle.begin()->find(f) != cycle.begin()->end())
            {
              for (; mark_cycle != cycle.end(); ++mark_cycle)
                mark_cycle->insert(f);
            }
          else if ((operation == op::W || operation == op::R)
              && cycle.begin()->find(f0) != cycle.begin()->end())
            {
              for (; mark_cycle != cycle.end(); ++mark_cycle)
                mark_cycle->insert(f);
            }
        }
      auto mark_pref = handle_until_loop(prefix, f, operation, f0, f1);
      if (mark_pref != prefix.end())
        {
          if (cycle.begin()->find(f) != cycle.begin()->end())
            {
              for (; mark_pref != prefix.end(); ++mark_pref)
                mark_pref->insert(f);
            }
        }
    }


    // Function to handle general LTL operator : a -> Ga
    void
    handle_generally(std::vector<std::set<formula>>& prefix,
        std::vector<std::set<formula>>& cycle, const formula& f)
    {
      auto iter_cycle = cycle.begin();
      for (; iter_cycle != cycle.end()
          && iter_cycle->find(f[0]) != iter_cycle->end(); ++iter_cycle)
        continue;
      // if all letters in the cycle don't validate Gf, then there are no letters
      // to mark
      if (iter_cycle == cycle.end())
        return;
      for (iter_cycle = cycle.begin(); iter_cycle != cycle.end(); ++iter_cycle)
        iter_cycle->insert(f);
      // from the end of the prefix, while we have f[0], we can mark them with
      // f
      for (auto riter_pref = prefix.rbegin(); riter_pref != prefix.rend()
          && riter_pref->find(f[0]) != riter_pref->end(); ++riter_pref)
        riter_pref->insert(f);
    }

    // Function to handle eventual LTL operator : a -> Fa
    void
    handle_eventually(std::vector<std::set<formula>>& prefix,
        std::vector<std::set<formula>>& cycle, const formula& f)
    {
      auto iter_cycle = cycle.begin();
      for (; iter_cycle != cycle.end()
          && iter_cycle->find(f[0]) == iter_cycle->end(); ++iter_cycle)
        continue;
      // if there is at least one occurence of a the Fa is true for all the cycle
      if (iter_cycle != cycle.end())
        {
          for (iter_cycle = cycle.begin(); iter_cycle != cycle.end();
              ++iter_cycle)
            iter_cycle->insert(f);
          for (auto iter_pref = prefix.begin();
              iter_pref != prefix.end(); ++iter_pref)
            iter_pref->insert(f);
        }
      else
        {
          auto riter_pref = prefix.rbegin();
          for (; riter_pref != prefix.rend()
              && riter_pref->find(f[0]) == riter_pref->end(); ++riter_pref)
            continue;
          // if we find an occurence of a, all letters before it can be marked
          // with Fa, that's why we want to iterate in reverse order
          if (riter_pref != prefix.rend())
            {
              for (; riter_pref != prefix.rend(); ++riter_pref)
                riter_pref->insert(f);
            }
        }

    }

    //handle Not operator
    //loops through the word and insert values
    void
    handle_not(std::vector<std::set<formula>>& prefix,
        std::vector<std::set<formula>>& cycle, const formula& f)
    {
      //loop on the prefix
      for (auto iter_pref = prefix.begin(); iter_pref != prefix.end();
          ++iter_pref)
        {
          if (iter_pref->find(f[0]) != iter_pref->end())
            iter_pref->insert(f);
        }
      //loop on the cycle
      for (auto iter_cycle = cycle.begin(); iter_cycle != cycle.end();
          ++iter_cycle)
        {
          if (iter_cycle->find(f[0]) != iter_cycle->end())
            iter_cycle->insert(f);
        }
    }

    void
    handle_next(std::vector<std::set<formula>>& prefix,
        std::vector<std::set<formula>>& cycle, const formula& f)
    {
      if (cycle[0].find(f[0]) != cycle[0].end())
        cycle[cycle.size() - 1].insert(f);
      for (size_t index = 1; index < cycle.size(); ++index)
        {
          if (cycle[index].find(f[0]) != cycle[index].end())
            cycle[index - 1].insert(f);
        }
      if (prefix.size() > 0)
        {
          if (cycle[0].find(f[0]) != cycle[0].end())
            prefix[prefix.size() - 1].insert(f);
          for (size_t index = prefix.size() - 1; index > 0; --index)
            {
              if (cycle[index].find(f[0]) != cycle[index].end())
                cycle[index - 1].insert(f);
            }
        }
    }

    //Function that evaluates every sub formulas on the word.
    //It will call itself on all the subformulas and then evaluate its formula
    void
    rec_match(std::vector<std::set<formula>>& prefix,
        std::vector<std::set<formula>>& cycle, formula f)
    {
      //stop case for recursion
      if (f.is_leaf())
        return;

      //recursive calls
      for (auto child = f.begin(); child != f.end(); ++child)
        rec_match(prefix, cycle, *child);

      //evaluation
      switch (f.kind())
        {
        case op::Not:
          handle_not(prefix, cycle, f);
          break;

        case op::X:
          handle_next(prefix, cycle, f);
          break;

        case op::F:
          handle_eventually(prefix, cycle, f);
          break;

        case op::G:
          handle_generally(prefix, cycle, f);
          break;

        case op::U:
          handle_until(prefix, cycle, f, op::U);
          break;

        case op::W:
          handle_until(prefix, cycle, f, op::W);
          break;

        case op::M:
          handle_until(prefix, cycle, f, op::M);
          break;

        case op::R:
          handle_until(prefix, cycle, f, op::R);
          break;

        default:
          std::cerr << "Alert ! one operator isn't implemented" << std::endl;
        }
    }

  }
  //This function takes a formula and a word. Returns true if the word
  //corresponds to the formula.
  //It works by transforming the twa_word into two vectors of sets of formulas.
  //
  bool
  match_word(formula f, twa_word_ptr w)
  {
    // transformation of the twa_word into some datastructure easier to use.
    std::vector<std::set<formula>> prefix = get_vector_formula(w, true);
    std::vector<std::set<formula>> cycle = get_vector_formula(w, false);

    // runs the recursive function that will proccess the vectors.
    rec_match(prefix, cycle, f);

    // Returns true if first letter of the prefix is recognized or if
    // the prefix is empty, applies on first letter of the cycle.
    if (prefix.size() > 0)
      return prefix[0].end() != prefix[0].find(f);
    return cycle[0].end() != cycle[0].find(f);
  }
}
