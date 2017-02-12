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

#include <spot/algebra/wilke.hh>

namespace spot
{

  // a class to represent linked pairs
  template<class Elem>
  class linked_pair
  {
    word<Elem> prefix_;
    word<Elem> power_;
  public:
    linked_pair(): prefix_(), power_() {}
    explicit linked_pair(word<Elem>&& s, word<Elem>&& e)
    : prefix_(s)
    , power_(e)
    {}

    const word<Elem>& prefix() const { return prefix_; }
    const word<Elem>& power() const { return power_; }

    bool operator==(const linked_pair& o) const
    {
      return prefix_ == o.prefix_ && power_ == o.power_;
    }
    bool operator<(const linked_pair& o) const
    {
      return prefix_ == o.prefix_ ? power_ < o.power_ : prefix_ < o.prefix_;
    }
  };

  template<class Elem>
  std::ostream&
  operator<<(std::ostream& os, const linked_pair<Elem>& lp)
  {
    os << lp.prefix() << ".(" << lp.power() << ")^w";
    return os;
  }

  template<class Elem, class FSG>
  struct canon_omega_helper
  {
    static_assert(std::is_same<Elem, typename FSG::base_t>::value,
        "incompatible types");
    const FSG* fsg;

    canon_omega_helper(const FSG* f): fsg(f) {}

    linked_pair<Elem>
    power(const word<Elem>& e) const
    {
      word<Elem> idem = fsg->idem_power(e);
      return make_pair(idem, idem);
    }
    linked_pair<Elem>
    mixed_prod(const word<Elem>& s, const linked_pair<Elem>& p) const
    {
      return make_pair(fsg->product(s, p.prefix()), p.power());
    }

    linked_pair<Elem>
    make_pair(const word<Elem>& s, const word<Elem>& e) const
    {
      word<Elem> mins = s;
      word<Elem> mine = e;
      word<Elem> currents = s;
      word<Elem> currente = e;
      for (const auto& l : e)
        {
          currents = fsg->product(currents, word<Elem>(1, l));
          currente.push_back(currente[0]);
          currente.erase(currente.begin());
          if (currents < mins)
            {
              mins = currents;
              mine = fsg->product(word<Elem>(0), currente);
            }
        }
      return linked_pair<Elem>(std::move(mins), std::move(mine));
    }
  };

  template<class Elem, class FSG>
  class canon_omega : public canon_omega_helper<Elem, FSG>
  {
  public:
    canon_omega(const FSG* fsg): canon_omega_helper<Elem, FSG>(fsg) {}

    linked_pair<Elem>
    operator()(const word<Elem>& e) const
    {
      return this->power(e);
    }
  };
  template<class Elem, class FSG>
  class canon_mixed : public canon_omega_helper<Elem, FSG>
  {
  public:
    canon_mixed(const FSG* fsg): canon_omega_helper<Elem, FSG>(fsg) {}

    linked_pair<Elem>
    operator()(const word<Elem>& s, const linked_pair<Elem>& p) const
    {
      return this->mixed_prod(s, p);
    }
  };

  template<class Elem, class Alphabet>
  class smart_canon :
    public smart_wilke<cache_fsg<Elem, Alphabet>,
                       linked_pair<unsigned>,
                       linked_pair<Elem>>
  {
  protected:
    using omega_t = linked_pair<unsigned>;
    using true_omega_t = linked_pair<Elem>;

  public:
    template<class ... Args>
    explicit smart_canon(Args&&... args)
    : smart_wilke<cache_fsg<Elem, Alphabet>, omega_t, true_omega_t>(
        canon_omega<unsigned, smart_canon>(this),
        canon_mixed<unsigned, smart_canon>(this),
        std::forward<Args>(args)...)
    {}

    true_omega_t get_original_omega(const omega_t& o) const override
    {
      using fsg_t = cache_fsg<Elem, Alphabet>;
      return linked_pair<Elem>(this->fsg_t::get_original(o.prefix()),
                               this->fsg_t::get_original(o.power()));
    }
  };

}
