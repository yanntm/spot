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

#include <iostream>

#include <spot/algebra/semigroup.hh>
#include <spot/algebra/indexmap.hh>

namespace spot
{

  /// \brief A class to represent a Wilke algebra.
  ///
  /// A Wilke algebra extends a finite semigroup with a finite set of omega
  /// elements (OElem), together with an omega power (that maps idempotents on
  /// omega elements) and a mixed product between finite and omega elements.
  template<class FSG, class OElem>
  class wilke : public FSG
  {
  public:
    // useful typedefs
    using omega_t = OElem;
    using fsg_t = FSG;

  protected:
    using FElem = typename FSG::element_t;

    static_assert(std::is_base_of<fsg<FElem>, FSG>::value,
      "Trying to make a Wilke algebra from something that is not a semigroup");

    virtual OElem _mixed_product(const FElem&, const OElem&) const = 0;
    virtual OElem _omega(const FElem&) const = 0;

  public:
    // constructor
    template<class ... Args>
    explicit wilke(Args&&... args)
    : FSG(std::forward<Args>(args)...)
    {}

    virtual void print(std::ostream&) const override = 0;

    OElem omega(const FElem& f) const {
      return _omega(f);
    }
    OElem mixed_product(const FElem& f, const OElem& o) const {
      return _mixed_product(f, o);
    }
  };

  template<class U, class V>
  class indirection
  {
  public:
    virtual V get_original_omega(const U&) const = 0;
  };
  template<class U>
  class indirection<U, U>
  {
  public:
    virtual U get_original_omega(const U& u) const
    {
      return u;
    }
  };

  template<class FSG, class OElem, class TO = OElem>
  class smart_wilke : public wilke<FSG, OElem>, public indirection<OElem, TO>
  {
  public:
    using base_t = typename FSG::base_t;
    using alphabet_t = typename FSG::alphabet_t;
    using omega_t = OElem;
    using true_omega_t = TO;

    static_assert(std::is_base_of<smart_fsg<base_t, alphabet_t>, FSG>::value,
        "Building a smart Wilke out of a non smart semigroup");

  protected:
    std::vector<omega_t> omegas_;
    std::map<word<base_t>, omega_t> omega_powers_;
    std::map<base_t, std::map<omega_t, omega_t>> omega_cayley_;

    omega_t
    _omega(const word<base_t>& e) const override
    {
      auto re = this->_reduce(e);
      if (re != e)
        {
          std::cerr << "unreduced word given, is it normal?" << std::endl;
          return _omega(re);
        }
      return omega_powers_.at(e);
    }

    omega_t
    _mixed_product(const word<base_t>& e, const omega_t& o) const override
    {
      auto res = o;
      for (auto it = e.rbegin(); it != e.rend(); ++it)
        res = omega_cayley_.at(*it).at(res);
      return res;
    }

  public:
    template<class Omega, class Mixed, class ... Args>
    explicit smart_wilke(Omega om_power, Mixed mp, Args&&... args)
    : wilke<FSG, OElem>(std::forward<Args>(args)...)
    {
      std::map<omega_t, omega_t> seen;
      std::deque<omega_t> todo;
      for (const auto& m : *this)
        {
          omega_t lp = om_power(m);
          omega_powers_[m] = lp;
          if (seen.find(lp) == seen.end())
            {
              seen[lp] = lp;
              omegas_.push_back(lp);
              todo.push_back(lp);
            }
        }

      // we need the left cayley of the mixed product
      while (!todo.empty())
        {
          auto u = todo.front();
          todo.pop_front();
          for (const auto& e : this->generators_)
            {
              omega_t lp = mp(e, u);
              auto pit = seen.find(lp);
              if (pit == seen.end())
                {
                  seen[lp] = lp;
                  omegas_.push_back(lp);
                  omega_cayley_[e[0]][u] = lp;
                  todo.push_back(lp);
                }
              else
                {
                  omega_cayley_[e[0]][u] = pit->second;
                }
            }
        }
    }

    void print(std::ostream& os) const override
    {
      FSG::print(os);
      os << std::endl;
      for (const auto& i : *this)
        {
          os << '(' << i << ")^w = " << this->omega(i);
          os << " and ";
          for (const auto& g : this->generators_)
            {
              os << g << ".(" << i << ")^w = ";
              os << this->mixed_product(g, this->omega(i));
              os << ", ";
            }
          os << '\n';
        }
      os << std::endl;
    }

    bool
    operator==(const smart_wilke& w) const
    {
      return this->wilke<FSG, OElem>::operator==(w)
        && omegas_ == w.omegas_
        && omega_powers_ == w.omega_powers_
        && omega_cayley_ == w.omega_cayley_;
    }
  };

}

