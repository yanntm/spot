// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et Développement de l'Epita
// (LRDE).
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 3 of the License, or (at your option) any later
// version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <functional>
#include <set>

#include <spot/algebra/wilke.hh>

namespace spot
{

  /// A language Wilke recognizes a language of infinite works.
  /// Compared to a Wilke algebra, it distinguishes some of its omega
  /// elements.
  template<class Wilke, class Alphabet>
  class language_wilke : public Wilke
  {
  public:
    // allow unqualified search of typedef in template-dependent base class
    using fsg_t = typename Wilke::fsg_t;
    using element_t = typename Wilke::element_t;
    using omega_t = typename Wilke::omega_t;

    static_assert(std::is_base_of<wilke<fsg_t, omega_t>, Wilke>::value,
        "Not building on a Wilke algebra");

  protected:
    virtual bool _is_accepting(const omega_t&) const = 0;

  public:
    template<class ... Args>
    explicit language_wilke(Args&&... args)
    : Wilke(std::forward<Args>(args)...)
    {}

    bool is_accepting(const omega_t& o) const { return _is_accepting(o); }

  };

  template<class T>
  class partition : public std::vector<std::vector<T>>
  {
  public:
    partition(): std::vector<std::vector<T>>() {}

    unsigned cellof(const T& t) const
    {
      SPOT_ASSERT(!this->empty());
      for (unsigned i = 0; i != this->size(); ++i)
        {
          for (const auto& elem : (*this)[i])
            if (t == elem)
              return i;
        }
      SPOT_ASSERT(false);
      throw std::runtime_error("not in partition");
    }
    bool are_equiv(const T& lhs, const T& rhs) const
    {
      for (const auto& cell : *this)
        {
          bool lhsfound = false;
          bool rhsfound = false;
          for (const auto& elem : cell)
          {
            if (elem == lhs)
              lhsfound = true;
            if (elem == rhs)
              rhsfound = true;
          }
          if (lhsfound != rhsfound)
            return false;
          if (lhsfound && rhsfound)
            return true;
        }
      throw std::runtime_error("not in partition");
    }

    void print(std::ostream& os) const
    {
      os << '{';
      for (const auto& cell : *this)
        {
          os << '{';
          for (const auto& e : cell)
            {
              os << e << ',';
            }
          os << '}';
        }
      os << '}';
    }
  };

  template<class Wilke>
  class smart_language : public Wilke
  {
    using fsg_t = typename Wilke::fsg_t;
    using element_t = typename Wilke::element_t;
    using base_t = typename Wilke::base_t;
    using omega_t = typename Wilke::omega_t;
    using true_omega_t = typename Wilke::true_omega_t;
    using alphabet_t = typename Wilke::alphabet_t;

    static_assert(
      std::is_base_of<smart_wilke<fsg_t, omega_t, true_omega_t>, Wilke>::value,
      "Building a smart language wilke out of a non smart Wilke");

  protected:
    /// use a std::map in general.
    /// TODO use a std::vector if the index type is integral and unsigned.
    // using boolmap_t = typename
    //   std::conditional<std::is_integral<omega_t>::value &&
    //                       std::is_unsigned<omega_t>::value,
    //                    std::vector<bool>,
    //                    std::map<omega_t, bool>>::type;
    using boolmap_t = std::map<omega_t, bool>;
    boolmap_t accepting_;

    virtual bool _is_accepting(const omega_t& o) const
    {
      return accepting_.at(o);
    }
  public:
    template<class Acc, class ... Args>
    explicit smart_language(Acc accept, Args&&... args)
    : Wilke(std::forward<Args>(args)...)
    {
      for (const auto& o : this->omegas_)
        accepting_[o] = accept(this->get_original_omega(o));
    }

    bool is_accepting(const omega_t& o) const
    {
      return _is_accepting(o);
    }

    virtual void print(std::ostream& os) const override
    {
      Wilke::print(os);

      os << "accepting omegas are ";
      for (const auto& o : this->omegas_)
        {
          if (is_accepting(o))
            os << o << ',';
        }
      os << std::endl;
    }

    bool
    operator==(const smart_language& other) const
    {
      bool res = this->Wilke::operator==(other);
      if (!res)
        return false;
      for (const auto& o : this->omegas_)
        {
          res &= (this->is_accepting(o) == other.is_accepting(o));
        }
      return res;
    }

    smart_language<smart_wilke<smart_fsg<base_t, alphabet_t>, unsigned>>
    syntactic_wilke() const
    {
      // First compute the syntactic congruence.
      // This is done by successive refinements of a partition of the finite
      // elements and of the omega elements.
      // Initially, separate accepting and rejecting omega elements.
      partition<omega_t> opart;
      opart.push_back({});
      opart.push_back({});
      for (const omega_t& o : this->omegas_)
        {
          if (this->is_accepting(o))
            opart[0].push_back(o);
          else
            opart[1].push_back(o);
        }

      // u \equiv v
      //    iff
      // for all x \in Sf^1. x.u acc <==> x.v acc
      //    iff
      // u \in P <==> v \in P and
      // for all a \in \Sigma. a.u \in P <==> a.v \in P and a.u \equiv a.v
      //
      // the case x = 1 is treated by the initialization
      decltype(opart) old_part;
      while (old_part != opart)
        {
          // debug
          std::cerr << "opart is ";
          opart.print(std::cerr);
          std::cerr << std::endl;

          old_part = opart;
          opart.clear();

          // for each cell
          for (const auto& cell : old_part)
            {
              // remember what elements in the cell have been treated
              std::vector<bool> treated(cell.size());
              // for each u in the cell
              for (unsigned i = 0; i != cell.size(); ++i)
                {
                  if (treated[i])
                    continue;

                  // u goes to a new class
                  const auto& u = cell[i];
                  opart.push_back({u});
                  treated[i] = true;

                  // for each v in the cell
                  for (unsigned j = 0; j != cell.size(); ++j)
                    {
                      if (treated[j])
                        continue;

                      const auto& v = cell[j];
                      bool same_class = true;
                      // for every generator (i.e. letter)
                      for (const auto& g : this->generators_)
                        {
                          auto gu = this->mixed_product(g, u);
                          auto gv = this->mixed_product(g, v);
                          if (this->is_accepting(gu) != this->is_accepting(gv)
                              || !old_part.are_equiv(gu, gv))
                            {
                              same_class = false;
                              break;
                            }
                        }

                      if (same_class)
                        {
                          // add v to the class of u
                          opart.rbegin()->push_back(v);
                          treated[j] = true;
                        }
                    }
                }
            }
        }

      // Do the same for finite elements.
      // Initially, separate elements whose omega powers are accepting or
      // rejecting.
      partition<element_t> fpart;
      fpart.push_back({});
      fpart.push_back({});
      for (const auto& e : *this)
        {
          if (is_accepting(this->omega(e)))
            fpart[0].push_back(e);
          else
            fpart[1].push_back(e);
        }

      // u \equiv v iff for all x,y \in Sf^1, z \in Sf
      //      xuyz^w acc  <==> xvyz^w acc
      //      x(uy)^w acc <==> x(vy)^w acc
      // Note that yz^w ranges over Sw when y ranges over Sf^1 and z over Sf.
      // We can thus rewrite as:
      // u \equiv v iff for all x,y \in Sf^1, z \in Sw
      //      xuz acc     <==> xvz acc      (1)
      //      x(uy)^w acc <==> x(uy)^w acc  (2)
      // From here, it is not hard to see that
      // u \equiv v iff u^w \equiv v^w and ua \equiv va for all a \in \Sigma.
      // Initialization handles the first condition.

      decltype(fpart) old_fpart;
      while (old_fpart != fpart)
        {
          // debug
          std::cerr << "fpart is ";
          fpart.print(std::cerr);
          std::cerr << std::endl;

          old_fpart = fpart;
          fpart.clear();

          // for each cell
          for (const auto& cell : old_fpart)
            {
              // remember what elements in the cell have been treated
              std::vector<bool> treated(cell.size());
              // for each u in the cell
              for (unsigned i = 0; i != cell.size(); ++i)
                {
                  if (treated[i])
                    continue;

                  const auto& u = cell[i];
                  fpart.push_back({u});
                  treated[i] = true;

                  // for each v in the cell
                  for (unsigned j = 0; j != cell.size(); ++j)
                    {
                      if (treated[j])
                        continue;

                      const auto& v = cell[j];
                      bool same_class = true;
                      for (const auto& g : this->generators_)
                        {
                          auto ug = this->product(u, g);
                          auto vg = this->product(v, g);
                          if (!old_fpart.are_equiv(ug, vg))
                            {
                              same_class = false;
                              break;
                            }
                        }

                      if (same_class)
                        {
                          // add v to the class of u
                          fpart.rbegin()->push_back(v);
                          treated[j] = true;
                        }
                    }
                }
            }
        }

      // Reorder fpart to respect the letters.
      old_fpart = fpart;
      std::vector<bool> done(fpart.size(), false);
      fpart.clear();
      morphism_t<alphabet_t, element_t> ord_morphism;
      for (const auto& it : this->morphism_)
        ord_morphism[it.first] = element_t(1, it.second);

      for (const auto& it : ord_morphism)
        {
          unsigned i = old_fpart.cellof(it.second);
          if (done[i])
            fpart.push_back({});
          else
            {
              fpart.push_back(old_fpart[i]);
              done[i] = true;
            }
        }
      for (unsigned i = 0; i != done.size(); ++i)
        {
          if (!done[i])
            fpart.push_back(old_fpart[i]);
        }
      // debug
      std::cerr << "reordered fpart is ";
      fpart.print(std::cerr);
      std::cerr << std::endl;

      // Build the quotient Wilke algebra.
      auto prod = [&fpart, this](const unsigned i, const unsigned j)
        {
          SPOT_ASSERT(i < fpart.size());
          SPOT_ASSERT(j < fpart.size());
          return fpart.cellof(this->product(fpart[i][0], fpart[j][0]));
        };
      auto toword = [&fpart](const word<unsigned>& w)
        {
          element_t ww;
          for (unsigned l : w)
          {
            SPOT_ASSERT(l < fpart.size());
            SPOT_ASSERT(fpart[l][0].size() == 1);
            ww.push_back(fpart[l][0][0]);
          }
          return ww;
        };
      std::function<unsigned(const word<unsigned>&)> power =
        [&, this](const word<unsigned>& w)
        {
          return opart.cellof(this->omega(toword(w)));
        };
      auto mix_prod = [&, this](const word<unsigned>& lhs, unsigned rhs)
        {
          return opart.cellof(this->mixed_product(toword(lhs), opart[rhs][0]));
        };
      auto acc = [&opart, this](unsigned i)
        {
          return this->is_accepting(opart[i][0]);
        };

      morphism_t<typename Wilke::alphabet_t, base_t> morphism;
      for (const auto& it : this->morphism_)
        {
          //SPOT_ASSERT(it.second < this->generators_.size());
          morphism[it.first] = fpart.cellof(element_t(1, it.second));
        }

      return
        smart_language<smart_wilke<smart_fsg<base_t, alphabet_t>, unsigned>>(
          acc,
          power,
          mix_prod,
          morphism,
          prod);
    }
  };
}
