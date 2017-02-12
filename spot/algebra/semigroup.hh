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

#include <deque>
#include <iostream>
#include <map>
#include <vector>

#include <spot/algebra/indexmap.hh>

namespace spot
{

  /// \brief The class for finite semigroups.
  ///
  /// This is an abstract class. It provides basic operations on the monoid
  /// elements, stores its generators but does nothing more.
  template<class Elem>
  class SPOT_API fsg
  {
  public:
    // useful typedef
    using element_t = Elem;
  protected:
    /// the generators
    std::vector<Elem> generators_;

    /// the product between elements of the semigroup
    virtual Elem _product(const Elem&, const Elem&) const = 0;
  public:
    /// a constructor
    explicit fsg(std::vector<Elem>&& gens)
    : generators_(std::move(gens))
    {}
    /// virtual destructor for inheritance
    virtual ~fsg() {}

    /// accessor to generators
    const std::vector<Elem>&
    generators() const
    {
      return generators_;
    }

    /// The semigroup interface
    ///   product of two elements
    ///   idempotent power of an element
    ///   whether an element is idempotent
    Elem
    product(const Elem& lhs, const Elem& rhs) const
    {
      return _product(lhs, rhs);
    }
    Elem
    idem_power(const Elem& e) const
    {
      Elem res = e;
      while (!is_idempotent(res))
        res = product(res, e);
      return res;
    }
    bool
    is_idempotent(const Elem& e) const
    {
      return product(e, e) == e;
    }

    /// pretty printing
    virtual void print(std::ostream&) const = 0;
  };

  /// A typedef to represent a morphism from the free monoid to an arbitrary
  /// semigroup. Such a morphism is characterized by the images of the letters
  /// the alphabet.
  template<class Alphabet, class Elem>
  using morphism_t = std::map<Alphabet, Elem, less_than<Alphabet>>;

  /// \brief A class to represent a word (i.e. a finite sequence) of elements.
  ///
  /// This class implements the military order on words, instead of the
  /// lexicographical order.
  template<class Elem>
  class word : public std::vector<Elem>
  {
  public:
    /// constructor from initializer list
    word(std::initializer_list<Elem> init)
    : std::vector<Elem>(init)
    {}
    /// constructor
    template<class ... Args>
    explicit word(Args&&... args)
    : std::vector<Elem>(std::forward<Args>(args)...)
    {}

    bool operator<(const word& o) const
    {
      if (this->size() < o.size())
        return true;
      if (this->size() > o.size())
        return false;
      return std::operator<(*this, o);
    }
    bool operator<=(const word& o) const
    {
      if (this->size() < o.size())
        return true;
      if (this->size() > o.size())
        return false;
      return std::operator<=(*this, o);
    }
    bool operator>(const word& o) const
    {
      return !(*this <= o);
    }
    bool operator>=(const word& o) const
    {
      return !(*this < o);
    }
  };

  /// a pretty printer
  template<class Elem>
  std::ostream& operator<<(std::ostream& os, const word<Elem>& w)
  {
    os << '{';
    for (const auto& e : w)
      os << e << ',';
    os << '}';
    return os;
  }
  /// a pretty printer for words
  template
  std::ostream& operator<<<unsigned>(std::ostream& os, const word<unsigned>& w);

  /// \brief A semigroup stored as word over its generators.
  ///
  /// This is an efficient way to compute and store a finite semigroup. This
  /// class implements the ideas exposed in
  ///   @inproceedings{pin:hal-00143949,
  ///     title = {{Algorithms for computing finite semigroups}},
  ///     author = {Pin, Jean-{\'E}ric and Delcroix-Froidure, V{\'e}ronique},
  ///     booktitle = {{Foundations of Computational Mathematics}},
  ///     year = {1997},
  ///     editor = {F. Cucker and M. Shub},
  ///     publisher = {{Springer Verlag}},
  ///     pages = {112-126},
  ///     address = {Rio de Janeiro, Brazil},
  ///     url = {https://hal.archives-ouvertes.fr/hal-00143949}
  ///   }
  /// Elements of the semigroup are stored as words over the generators. More
  /// precisely, an element e of the semigroup is represented by the smallest
  /// (for the military order) word over its generators whose product equals to
  /// e. Those are called 'reduced word'. The implementation computes a
  /// confluent rewriting system over words, and the Cayley graph of the
  /// semigroup.
  template<class Elem, class Alphabet>
  class smart_fsg : public fsg<word<Elem>>
  {
  public:
    using base_t = Elem;
    using alphabet_t = Alphabet;
  protected:
    /// all the elements of the semigroup, as reduced words
    std::vector<word<Elem>> elems_;
    /// NB The rewriting rules are not explicitely stored, but they are used to
    /// build the Cayley graph.
    /// The (right) Cayley graph of the semigroup. Maps a reduced word and a
    /// letter to a reduced word.
    std::map<word<Elem>, std::map<Elem, word<Elem>>> cayley_;
    /// the morphism from the free monoid
    /// Strictly speaking, this is not necessary in this class, but it is
    /// convenient to store it here.
    morphism_t<Alphabet, Elem> morphism_;

    /// Computes the product (as a reduced word) of two arbitrary words.
    /// This method relies on the Cayley graph
    word<Elem>
    _product(const word<Elem>& lhs, const word<Elem>& rhs) const override
    {
      word<Elem> res;
      for (const auto& l : lhs)
        res = cayley_.at(res).at(l);
      for (const auto& l : rhs)
        res = cayley_.at(res).at(l);
      return res;
    }
  public:
    /// a default constructor
    explicit smart_fsg(): fsg<word<Elem>>({}) {}
    /// actual constructor
    template<class ElemProd>
    explicit smart_fsg(const morphism_t<Alphabet, Elem>& m,
                       ElemProd real_prod)
    : fsg<word<Elem>>({})
    {
      std::map<Elem, word<Elem>> seen;
      std::deque<Elem> todo;
      for (const auto& e : m)
        {
          morphism_[e.first] = e.second;

          if (seen.find(e.second) == seen.end())
            {
              word<Elem> ge = {e.second};
              this->generators_.push_back(ge);
              elems_.push_back(ge);
              cayley_[word<Elem>()][e.second] = ge;
              seen[e.second] = ge;
              todo.push_back(e.second);
            }
        }

      while (!todo.empty())
        {
          auto u = todo.front();
          todo.pop_front();
          auto uword = seen.at(u);
          for (const auto& e : this->generators_)
            {
              SPOT_ASSERT(e.size() == 1);
              Elem p = real_prod(u, e[0]);
              auto pit = seen.find(p);
              if (pit == seen.end())
                {
                  word<Elem> pword = uword;
                  pword.push_back(e[0]);
                  seen[p] = pword;
                  elems_.push_back(pword);
                  cayley_[uword][e[0]] = pword;
                  todo.push_back(p);
                }
              else
                {
                  cayley_[uword][e[0]] = pit->second;
                }
            }
        }
    }

    virtual void print(std::ostream& os) const override
    {
      os << "\t|";
      for (const auto& jt : cayley_.begin()->second)
        {
          os << jt.first << ",\t";
        }
      os << '\n';
      for (const auto& it : cayley_)
        {
          os << it.first << "\t|";
          for (const auto& jt : it.second)
            {
              os << jt.second << '\t';
            }
          os << '\n';
        }
    }

    bool operator==(const smart_fsg& o) const
    {
      return elems_ == o.elems_
        && cayley_ == o.cayley_
        && morphism_ == o.morphism_;
    }

    /// Iteration interface.
    using iterator = typename std::vector<word<Elem>>::const_iterator;
    iterator begin() const { return elems_.begin(); }
    iterator end() const { return elems_.end(); }
    size_t size() const { return elems_.size(); }
  };

  // TODO code is duplicated between smart_fsg and cache_fsg
  template<class Elem, class Alphabet>
  class cache_fsg : public smart_fsg<unsigned, Alphabet>
  {
  protected:
    std::vector<Elem> cache_;

    word<Elem> get_original(const word<unsigned>& w) const
    {
      word<Elem> res;
      for (const auto& l : w)
        res.push_back(cache_[l]);
      return res;
    }

  public:
    template<class ElemProd>
    explicit cache_fsg(const morphism_t<Alphabet, Elem>& morphism,
                       ElemProd real_prod)
    : smart_fsg<unsigned, Alphabet>()
    {
      std::map<Elem, unsigned> genmap;
      std::vector<Elem> gens;
      for (const auto& e : morphism)
        {
          if (genmap.find(e.second) == genmap.end())
            {
              unsigned tmp = genmap.size();
              genmap[e.second] = tmp;
              this->morphism_[e.first] = genmap[e.second];
              gens.push_back(e.second);
            }
          else
            {
              this->morphism_[e.first] = genmap[e.second];
            }
        }

      std::map<Elem, word<unsigned>> seen;
      std::deque<Elem> todo;
      for (unsigned i = 0; i != gens.size(); ++i)
        {
          word<unsigned> ge(1, i);
          this->generators_.push_back(ge);
          this->elems_.push_back(ge);
          this->cayley_[word<unsigned>(0)][i] = ge;
          seen[gens[i]] = ge;
          todo.push_back(gens[i]);
          cache_.push_back(gens[i]);
        }

      while (!todo.empty())
        {
          auto u = todo.front();
          todo.pop_front();
          auto uword = seen.at(u);
          for (unsigned i = 0; i != gens.size(); ++i)
            {
              Elem p = real_prod(u, gens[i]);
              auto pit = seen.find(p);
              if (pit == seen.end())
                {
                  word<unsigned> pword = uword;
                  pword.push_back(i);
                  seen[p] = pword;
                  this->elems_.push_back(pword);
                  this->cayley_[uword][i] = pword;
                  todo.push_back(p);
                }
              else
                {
                  this->cayley_[uword][i] = pit->second;
                }
            }
        }
    }
  };

}

