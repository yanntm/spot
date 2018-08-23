// Copyright (C) 2018 Laboratoire de Recherche et Developpement
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

#pragma once

#include <spot/tl/formula.hh>
#include <spot/tl/parse.hh>


namespace spot
{
  namespace
  {
    inline size_t fnv_hash(const char* name)
    {
      if (name == nullptr)
        return 0;

      size_t res = fnv<size_t>::init;
      for (; *name != 0; ++name)
        {
          res ^= *name;
          res *= fnv<size_t>::prime;
        }
      return res;
    }
  }

  class rewriting_recipe;
  class rewriting_output;

  /// \addtogroup tl_rewriting
  /// @{

  /// \brief A match obtained after visiting a \ref formula with a
  /// \ref rewriting_pattern.
  ///
  /// A rewriting match is immutable, and may represent an invalid match with
  /// the static member \ref nomatch.
  ///
  /// In some places, a \a name_hash (of type \ref size_t) may be accepted
  /// instead of a \a name (of type \ref char*). This is used to allow storing
  /// FNV hashes of strings instead of strings themselves, therefore reducing
  /// allocations and speeding up string comparisons.
  class SPOT_API rewriting_match final
  {
  public:
    /// The type of a stored value.
    using value_type = size_t;

  private:
    struct inner;

    std::shared_ptr<const inner> ptr_;

    struct inner final
    {
      enum class matchk
      {
        Empty, Value, Formula, Scope, Output
      };

      matchk kind_;
      mutable std::shared_ptr<const inner> prev_;
      rewriting_recipe const* root_recipe_;

      size_t name_hash_;

      union
      {
        std::shared_ptr<rewriting_output> output_;
        std::shared_ptr<const inner> inner_;
        value_type value_;
        formula formula_;
      };

      inner(std::shared_ptr<const inner> p, size_t n,
            rewriting_match::value_type v)
        : kind_(matchk::Value), prev_(p), root_recipe_(p->root_recipe_)
        , name_hash_(n)
        , value_(v)
      {
        SPOT_ASSERT(p->root_recipe_);
      }
      
      inner(std::shared_ptr<const inner> p, size_t n, formula f)
        : kind_(matchk::Formula), prev_(p), root_recipe_(p->root_recipe_)
        , name_hash_(n)
        , formula_(f)
      {
        SPOT_ASSERT(p->root_recipe_);
      }
      
      inner(std::shared_ptr<const inner> p, size_t n,
            rewriting_match i)
        : kind_(matchk::Scope), prev_(p), root_recipe_(p->root_recipe_)
        , name_hash_(n)
        , inner_(i.ptr_)
      {
        SPOT_ASSERT(p->root_recipe_);
      }

      inner(std::shared_ptr<const inner> p,
            std::shared_ptr<rewriting_output> r)
        : kind_(matchk::Output), prev_(p), root_recipe_(p->root_recipe_)
        , output_(r)
      {
        SPOT_ASSERT(p->root_recipe_);
      }

      inner()
        : kind_(matchk::Empty), prev_(nullptr), root_recipe_(nullptr)
        , output_(nullptr)
      {}

      inner(const rewriting_recipe& recipe)
        : kind_(matchk::Empty), prev_(nullptr), root_recipe_(&recipe)
        , output_(nullptr)
      {}

      inner(const inner& value)
        : kind_(value.kind_), prev_(value.prev_)
        , root_recipe_(value.root_recipe_)
        , output_(nullptr)
      {
        switch (kind_)
          {
          case matchk::Empty:
            break;
          case matchk::Value:
            name_hash_ = value.name_hash_;
            value_ = value.value_;
            break;
          case matchk::Formula:
            name_hash_ = value.name_hash_;
            formula_ = value.formula_;
            break;
          case matchk::Scope:
            name_hash_ = value.name_hash_;
            inner_ = value.inner_;
            break;
          case matchk::Output:
            output_ = value.output_;
            break;
          }
      }

      ~inner()
      {
        // prev_.~shared_ptr();

        switch (kind_)
          {
          case matchk::Output:
            output_.~shared_ptr();
            break;
          case matchk::Formula:
            formula_.~formula();
            break;
          case matchk::Scope:
            inner_.~shared_ptr();
            break;
          case matchk::Value:
          case matchk::Empty:
            break;
          }
      }
    };

    rewriting_match(const inner ptr) : ptr_(std::make_shared<inner>(ptr))
    {}
    rewriting_match(std::shared_ptr<const inner> ptr) : ptr_(ptr)
    {}

  public:
    /// \brief Create a new empty (and therefore invalid) rewriting match.
    ///
    /// If you wish to create a root (but valid) rewriting match, use
    /// \ref rewriting_match(const rewriting_recipe&).
    rewriting_match()
      : ptr_(std::make_shared<inner>())
    {}

    /// \brief Create a new root rewriting match, given the \a recipe that led
    /// to its creation.
    rewriting_match(const rewriting_recipe& recipe)
      : ptr_(std::make_shared<inner>(recipe))
    {}

    /// Copy a rewriting match.
    rewriting_match& operator=(const rewriting_match& other)
    {
      ptr_ = other.ptr_;

      return *this;
    }


    /// \brief Return a new \ref rewriting_match that inherits the current
    /// match, and also contains a \a value identified by the given \a name.
    inline rewriting_match
    with_value(const char* name, rewriting_match::value_type value) const
    {
      return with_value(fnv_hash(name), value);
    }

    /// \brief Return a new \ref rewriting_match that inherits the current
    /// match, and also contains a \a value identified by the given
    /// \a name_hash.
    inline rewriting_match
    with_value(size_t name_hash, rewriting_match::value_type value) const
    {
      return rewriting_match(inner(ptr_, name_hash, value));
    }

    /// \brief Return a new \ref rewriting_match that inherits the current
    /// match, and also contains a \ref formula \a f identified by the
    /// given \a name.
    inline rewriting_match
    with_formula(const char* name, formula f) const
    {
      return with_formula(fnv_hash(name), f);
    }

    /// \brief Return a new \ref rewriting_match that inherits the current
    /// match, and also contains a \ref formula \a f identified by the given
    /// \a name_hash.
    inline rewriting_match
    with_formula(size_t name_hash, formula f) const
    {
      return rewriting_match(inner(ptr_, name_hash, f));
    }

    /// \brief Return a new \ref rewriting_match that inherits the current match
    /// and also indicates that, should a \ref formula completely match a
    /// pattern, the given \a output should be used to rewrite it.
    inline rewriting_match
    with_output(std::shared_ptr<rewriting_output> output) const
    {
      return rewriting_match(inner(ptr_, output));
    }

    /// \brief Return a new \ref rewriting_match that inherits the current
    /// match, and also exists within its own scope named \a name.
    inline rewriting_match
    with_scope(const char* name, rewriting_match inner_match) const
    {
      return with_scope(fnv_hash(name), inner_match);
    }

    /// \brief Return a new \ref rewriting_match that inherits the current
    /// match, and also exists in its own scope with name hash \a name_hash.
    inline rewriting_match
    with_scope(size_t name_hash, rewriting_match inner_match) const
    {
      return rewriting_match(inner(ptr_, name_hash, inner_match));
    }

    /// \brief Return a new \ref rewriting_match that merges all the data 
    /// contained in the given matches.
    rewriting_match with_children(std::vector<rewriting_match>&& children) const
    {
      rewriting_match merged = *this;

      while (!children.empty())
        {
          // for each child, find either their root or the first parent that
          // references the current match, cut it, and add it to 'merged'
          auto child = children.back();
          auto ptr = child.ptr_.get();

          if (child.ptr_.use_count() > 2)
            // should be two references: the one in 'child', and the one in
            // the vector
            throw std::runtime_error("Unable to merge children that have "
                                     "multiple references to themselves.");

          while (ptr->prev_ && ptr->prev_ != ptr_)
            ptr = ptr->prev_.get();

          ptr->prev_ = merged.ptr_;
          merged = child;
          
          children.pop_back();
        }
    
      return merged;
    }


    /// \brief Return an invalid \ref rewriting_match, which represents a
    /// failure for a \ref rewriting_pattern to match a \ref formula.
    static rewriting_match nomatch() { return rewriting_match(); }

    /// Return whether the \ref rewriting_match is valid.
    inline operator bool() const
    {
      return ptr_->kind_ != inner::matchk::Empty
          || ptr_->root_recipe_ != nullptr;
    }


    /// \brief Return a reference to the \ref rewriting_recipe for which this
    /// \ref rewriting_match was created.
    inline const rewriting_recipe& recipe() const
    {
      if (!ptr_->root_recipe_)
        throw std::runtime_error("Unable to get a recipe from an invalid match.");
      
      return *ptr_->root_recipe_;
    }

    /// \brief Attempt to get the \ref rewriting_match that was in the scope
    /// having the given \a name.
    ///
    /// \return a pair whose first element is \c true if the scope was found,
    /// and \c false otherwise.
    inline std::pair<bool, rewriting_match>
    try_get_scope(const char* name, size_t max_depth = SIZE_MAX) const
    {
      return try_get_scope(fnv_hash(name), max_depth);
    }

    /// \brief Attempt to get the \ref scope with the given \a name_hash.
    ///
    /// \return a pair whose first element is \c true if the scope was found,
    /// and \c false otherwise.
    std::pair<bool, rewriting_match>
    try_get_scope(size_t name_hash, size_t max_depth = SIZE_MAX) const
    {
      if (SPOT_UNLIKELY(!max_depth))
        max_depth = SIZE_MAX;
      
      const inner* ptr = ptr_.get();

      while (ptr != nullptr && ptr->kind_ != inner::matchk::Empty &&
             max_depth--)
        {
          if (ptr->kind_ == inner::matchk::Scope
              && ptr->name_hash_ == name_hash)
            {
              // found match
              return std::make_pair(true, rewriting_match(ptr->inner_));
            }
          
          ptr = ptr->prev_.get();
        }
      
      return std::make_pair(false, rewriting_match::nomatch());
    }

    /// \brief Attempt to get the \ref rewriting_output that should be used to
    /// create a new \ref formula.
    ///
    /// \return a pair whose first element is \c true if the output was found,
    /// and \c false otherwise.
    std::pair<bool, std::shared_ptr<rewriting_output>>
    try_get_output(size_t max_depth = SIZE_MAX) const
    {
      if (SPOT_UNLIKELY(!max_depth))
        max_depth = SIZE_MAX;

      const inner* ptr = ptr_.get();

      while (ptr != nullptr && ptr->kind_ != inner::matchk::Empty &&
             max_depth--)
        {
          if (ptr->kind_ == inner::matchk::Output)
            {
              // found match
              return std::make_pair(true, ptr->output_);
            }

          ptr = ptr->prev_.get();
        }
      
      return std::make_pair(false, nullptr);
    }

    /// \brief Attempt to get the \ref formula that was recognized to have the
    /// given \a name.
    ///
    /// \return a pair whose first element is \c true if the formula was found,
    /// and \c false otherwise.
    inline std::pair<bool, spot::formula>
    try_get_formula(const char* name, size_t max_depth = SIZE_MAX) const
    {
      return try_get_formula(fnv_hash(name), max_depth);
    }

    /// \brief Attempt to get the \ref formula that was recognized to have the
    /// given \a name_hash.
    ///
    /// \return a pair whose first element is \c true if the formula was found,
    /// and \c false otherwise.
    std::pair<bool, spot::formula>
    try_get_formula(size_t name_hash, size_t max_depth = SIZE_MAX) const
    {
      if (SPOT_UNLIKELY(!max_depth))
        max_depth = SIZE_MAX;
      
      const inner* ptr = ptr_.get();

      while (ptr != nullptr && ptr->kind_ != inner::matchk::Empty &&
             max_depth--)
        {
          if (ptr->kind_ == inner::matchk::Formula &&
              ptr->name_hash_ == name_hash)
            {
              // found match
              return std::make_pair(true, ptr->formula_);
            }
          
          ptr = ptr->prev_.get();
        }
      
      return std::make_pair(false, nullptr);
    }

    /// \brief Attempt to get the value that was recognized to have the
    /// given \a name.
    ///
    /// \return a pair whose first element is \c true if the value was found,
    /// and \c false otherwise.
    inline std::pair<bool, rewriting_match::value_type>
    try_get_value(const char* name, size_t max_depth = SIZE_MAX) const
    {
      return try_get_value(fnv_hash(name), max_depth);
    }

    /// \brief Attempt to get the value that was recognized to have the
    /// given \a name_hash.
    ///
    /// \return a pair whose first element is \c true if the value was found,
    /// and \c false otherwise.
    std::pair<bool, rewriting_match::value_type>
    try_get_value(size_t name_hash, size_t max_depth = SIZE_MAX) const
    {
      if (SPOT_UNLIKELY(!max_depth))
        max_depth = SIZE_MAX;
      
      const inner* ptr = ptr_.get();

      while (ptr != nullptr && ptr->kind_ != inner::matchk::Empty &&
             max_depth--)
        {
          if (ptr->kind_ == inner::matchk::Value &&
              ptr->name_hash_ == name_hash)
            {
              // found match
              return std::make_pair(true, ptr->value_);
            }
          
          ptr = ptr->prev_.get();
        }
      
      return std::make_pair(false, 0);
    }

    /// An iterator of all values or formulas in a given \ref rewriting_match.
    template<typename T>
    class SPOT_API rewriting_match_iterator final
    {
    private:
      const size_t name_hash_;
      size_t remaining_depth_;
      T value_;
      std::shared_ptr<const inner> match_;
      bool at_root_;

      inline void set_value();

    public:
      rewriting_match_iterator(size_t name_hash,
                               std::shared_ptr<const inner> match,
                               size_t remaining_depth)
        : name_hash_(name_hash)
        , remaining_depth_(remaining_depth ? remaining_depth : SIZE_MAX)
        , match_(match)
        , at_root_(true)
      {}

      inline bool operator==(const rewriting_match_iterator<T>& other) const
      {
        return match_ == other.match_
            || (match_ != nullptr && other.match_ != nullptr
                && match_->kind_ == inner::matchk::Empty
                && other.match_->kind_ == inner::matchk::Empty);
      }

      inline bool operator!=(const rewriting_match_iterator<T>& other) const
      {
        return !(*this == other);
      }

      rewriting_match_iterator operator++()
      {
        if (!match_)
          return *this;

        while (remaining_depth_--)
          {
            if (at_root_)
              at_root_ = false;
            else
              match_ = match_->prev_;

            if (!match_)
              return *this;

            switch (match_->kind_)
              {
              case inner::matchk::Formula:
              case inner::matchk::Value:
              case inner::matchk::Scope:
                if (match_->name_hash_ != name_hash_)
                  continue;
                
                set_value();
                return *this;
              
              default:
                break;
              }
          }

        match_ = nullptr;
        return *this;
      }

      inline T operator*() const
      {
        SPOT_ASSERT(match_);
        
        return value_;
      }

      inline bool at_end() const
      {
        return !match_;
      }

      inline T value() const
      {
        SPOT_ASSERT(match_);

        return value_;
      }
    };

    /// Return an iterator over all formulas identified by the given \a name.
    inline rewriting_match_iterator<formula>
    formulas(const char* name, size_t max_depth = SIZE_MAX) const
    {
      return rewriting_match_iterator<formula>(fnv_hash(name), ptr_, max_depth);
    }

    /// \brief Return an iterator over all formulas identified by the given
    /// \a name_hash.
    inline rewriting_match_iterator<formula>
    formulas(size_t name_hash, size_t max_depth = SIZE_MAX) const
    {
      return rewriting_match_iterator<formula>(name_hash, ptr_, max_depth);
    }

    /// Return an iterator over all values identified by the given \a name.
    inline rewriting_match_iterator<rewriting_match::value_type>
    values(const char* name, size_t max_depth = SIZE_MAX) const
    {
      return rewriting_match_iterator<rewriting_match::value_type>(fnv_hash(name),
                                                                   ptr_,
                                                                   max_depth);
    }

    /// Return an iterator over all values identified by the given \a name_hash.
    inline rewriting_match_iterator<rewriting_match::value_type>
    values(size_t name_hash, size_t max_depth = SIZE_MAX) const
    {
      return rewriting_match_iterator<rewriting_match::value_type>(name_hash,
                                                                   ptr_,
                                                                   max_depth);
    }

    /// Return an iterator over all scopes identified by the given \a name.
    inline rewriting_match_iterator<rewriting_match>
    scopes(const char* name, size_t max_depth = SIZE_MAX) const
    {
      return rewriting_match_iterator<rewriting_match>(fnv_hash(name), ptr_,
                                                       max_depth);
    }

    /// Return an iterator over all formulas identified by the given \a name.
    inline rewriting_match_iterator<rewriting_match>
    scopes(size_t name_hash, size_t max_depth = SIZE_MAX) const
    {
      return rewriting_match_iterator<rewriting_match>(name_hash, ptr_,
                                                       max_depth);
    }

    /// \brief Return an iterator that represents the end of iterators returned
    /// by \ref formula(const char*), \ref formulas(size_t), \ref
    /// values(const char*), and \ref values(size_t).
    template<typename T>
    inline rewriting_match_iterator<T> end() const
    {
      return rewriting_match_iterator<T>(0, nullptr, 0);
    }

    /// Dump the content of the match to an output stream.
    std::ostream& dump(std::ostream& os, unsigned depth = 0,
                       size_t max_depth = SIZE_MAX) const;
  };

  /// \brief A pattern that matches \ref formulas recursively in order to build
  /// a \ref rewriting_match, if a complete pattern is valid.
  class SPOT_API rewriting_pattern
  {
  public:
    /// \brief A function that maps an identifier to a \ref rewriting_pattern.
    using mapper
      = std::function<std::unique_ptr<rewriting_pattern>(const char*)>;
    
    /// \brief A function that returns whether a \ref rewriting_match is valid.
    using predicate
      = std::function<bool(const rewriting_match&)>;

    /// \brief Return a \ref rewriting_match constructed from its previous match
    /// and that contains all the data needed in the given formula.
    ///
    /// If said formula does not match the current pattern,
    /// \ref rewriting_match::nomatch should be returned.
    virtual rewriting_match
    match(const rewriting_match& previous, const formula& f) const = 0;

    /// \brief Attempt to merge the current pattern with the
    /// given pattern.
    ///
    /// On success, the given pattern may be used as the callee sees fit. It
    /// may be moved and/or uninitialized, although the caller will ensure
    /// it is deallocated.
    ///
    /// On failure, the given pattern shall not be modified.
    ///
    /// \return whether the pattern was modified.
    virtual bool try_merge(rewriting_pattern& tr) = 0;

    // Note: the following method could instead be 'traverse(function)',
    // but I doubt it needs to be *that* general; therefore for now we're using
    // a very specific overload.

    /// \brief Associate a \ref rewriting_output to this tree, keeping track
    /// of the current depth at the same time, relative to the current root
    /// or n-ary-matching parent.
    ///
    /// This method should be called recursively on all child node of this
    /// \ref rewriting_pattern.
    virtual void
    associate(std::shared_ptr<rewriting_output> output) = 0;

    /// Dump a representation of the pattern to a stream.
    virtual std::ostream&
    dump(std::ostream& os,
         unsigned depth = 0, bool is_unique_child = true) const = 0;

    /// Dump a representation of the pattern to a string, and return it.
    std::string to_string() const
    {
      std::ostringstream os;
      dump(os, 0, true);
      return os.str();
    }

    /// Destroy the pattern.
    virtual ~rewriting_pattern() = default;

    /// \brief Create a \ref rewriting_pattern that only matches if the given
    /// \a pattern does, and if the given \a predicate returns \c true.
    static std::unique_ptr<spot::rewriting_pattern>
    condition(std::unique_ptr<spot::rewriting_pattern> pattern,
              spot::rewriting_pattern::predicate predicate);
  };

  /// \brief A output that defines how a formula should be transformed once it
  /// has fully matched a recipe.
  class SPOT_API rewriting_output
  {
  public:
    /// \brief A function that maps an identifier to a \ref rewriting_output.
    using mapper
      = std::function<std::unique_ptr<rewriting_output>(const char*)>;

  private:
    mutable size_t hash_;
  
  protected:
    /// Compute the hash of the output.
    virtual size_t compute_hash() const = 0;

  public:
    /// \brief Return a hash that identifies the output.
    ///
    /// Two outputs are equivalent if their hash is equal.
    size_t hash() const
    {
      // TODO: Why is the cached value wrong?
      return compute_hash();
      
      if (hash_ == 0)
        hash_ = compute_hash();
      
      return hash_;
    }

    /// Return whether the two compared outputs are equivalent.
    bool operator==(const rewriting_output& other) const
    {
      return other.hash() == hash();
    }

    /// Return whether the two compared outputs are different.
    bool operator!=(const rewriting_output& other) const
    {
      return other.hash() != hash();
    }

    /// \brief Create one or many new \ref formulas using the data stored in
    /// the given \a match.
    ///
    /// \return a vector of \ref formulas. If it is empty, the match should be
    /// considered invalid.
    ///
    /// \note Returning an empty vector does not lead to an allocation, even
    /// with "return {};", so there is no need to worry about performances.
    /// Do watch out about copies though, and try to only use move semantics.
    virtual std::vector<formula> create(const rewriting_match& match) const = 0;

    /// Dump a representation of the output to a stream.
    virtual
    std::ostream& dump(std::ostream& os, bool parens = false) const = 0;

    /// Dump a representation of the output to a string, and return it.
    std::string to_string() const
    {
      std::ostringstream os;
      dump(os, false);
      return os.str();
    }

    /// Destroy the output.
    virtual ~rewriting_output() = default;

    /// \brief Create a \ref rewriting_output that maps the result of the given
    /// \a output to a new value using the given \a map function.
    ///
    /// If \a map returns \c nullptr, the replacement will not be made.
    static std::unique_ptr<spot::rewriting_output>
    map(std::unique_ptr<spot::rewriting_output> output,
        std::function<spot::formula(spot::formula&)> map);
  };

  /// \brief A recipe for simplifying a formula.
  class SPOT_API rewriting_recipe final
  {
  public:
    /// The type of a vector of \ref rewriting_patterns.
    using patterns_type = std::vector<std::unique_ptr<rewriting_pattern>>;

  private:
    patterns_type patterns_;

  public:
    /// Move-construct a \ref rewriting_recipe.
    rewriting_recipe(rewriting_recipe&&) = default;

    /// Move-assign a \ref rewriting_recipe.
    rewriting_recipe& operator=(rewriting_recipe&&) = default;

    /// \brief Create a new rewriting recipe, given its unique
    /// \ref rewriting_pattern and associated \ref rewriting_output.
    rewriting_recipe(std::unique_ptr<rewriting_pattern>&& pattern,
                     std::unique_ptr<rewriting_output>&& output)
    {
      std::shared_ptr<rewriting_output> shared_output(std::move(output));

      pattern->associate(shared_output);

      patterns_.reserve(1);
      patterns_.push_back(std::move(pattern));
    }

    /// \brief Create a new rewriting recipe, given a vector of
    /// \ref rewriting_pattern's.
    rewriting_recipe(rewriting_recipe::patterns_type&& patterns)
      : patterns_(std::move(patterns))
    {
      if (patterns_.size() == 0)
        throw std::runtime_error("At least one pattern must be given.");
    }

#ifdef SWIGPYTHON
    rewriting_recipe()
    {}

    rewriting_recipe(rewriting_pattern* pattern, rewriting_output* output)
      : rewriting_recipe(std::unique_ptr<rewriting_pattern>(pattern),
                         std::unique_ptr<rewriting_output>(output))
    {}
#endif

    /// Return all the inner \ref rewriting_pattern's that make up this recipe.
    const rewriting_recipe::patterns_type& patterns() const
    {
      return patterns_;
    }
  
  private:
    static formula rewrite_rec(formula child, const rewriting_recipe& recipe,
                               std::map<size_t, formula> processed)
    {
      // Make sure we don't visit the same formula twice
      auto id = child.id();
      auto search = processed.find(id);

      if (search != processed.end())
        return search->second;
      
      // TODO: What if a recursive call uses this formula? It will
      //       always have the wrong value.
      processed[id] = child;

      rewriting_match initial_match(recipe);
      formula f = child.map(rewrite_rec, recipe, processed);

      for (auto& trans: recipe.patterns_)
        {
          auto match = trans->match(initial_match, f);

#if TRACE
          if (match)
            match.dump(std::clog << "[MATCH]\n") << std::endl;
#endif

          if (!match)
            continue;
          
          auto output = match.try_get_output();

          if (!output.first)
            throw std::runtime_error("One of the given patterns does not "
                                     "return a rewriting output, and is "
                                     "therefore invalid.");

          std::vector<formula> res = output.second->create(match);

          if (res.size() == 0)
            continue;
          if (res.size() != 1)
            throw std::runtime_error("Multiple formulas were returned from an "
                                     "output.");
          
          f = res[0];
          break;
        }

      processed[id] = f;
      
      return f;
    }

  public:
    /// Rewrite and simplify the specified formula using this recipe.
    formula rewrite(const formula& f) const
    {
      std::map<size_t, formula> processed;

      return rewrite_rec(f, *this, processed);
    }

    /// Dump a representation of the recipe to a stream.
    std::ostream& dump(std::ostream& os) const
    {
      for (auto& tr: patterns_)
        tr->dump(os, 0, true);

      return os;
    }

    /// Extend a rewriting recipe by merging another one into it.
    void extend(rewriting_recipe&& other);

    /// \brief Extend a rewriting recipe with new rules that follow the syntax
    /// given in \ref parse_rewriting_recipe().
    void extend(const char* rules,
                rewriting_pattern::mapper pattern_mapper = nullptr,
                rewriting_output::mapper output_mapper   = nullptr,
                rewriting_pattern::predicate predicate   = nullptr)
      throw(parse_error);
  };

  template<>
  void rewriting_match::rewriting_match_iterator<formula>::set_value()
  {
    value_ = match_->formula_;
  }
  template<>
  void rewriting_match::rewriting_match_iterator<size_t>::set_value()
  {
    value_ = match_->value_;
  }
  template<>
  void rewriting_match::rewriting_match_iterator<rewriting_match>::set_value()
  {
    value_ = rewriting_match(match_->inner_);
  }

  /// \brief Parse a single \ref rewriting_pattern, using the syntax described
  /// in the documentation for \ref parse_rewriting_recipe().
  std::unique_ptr<rewriting_pattern> SPOT_API
  parse_rewriting_pattern(const char* pattern,
                          rewriting_pattern::mapper mapper = nullptr,
                          rewriting_pattern::predicate predicate = nullptr)
    throw(parse_error);

  /// \brief Parse a single \ref rewriting_output, using the syntax described
  /// in the documentation for \ref parse_rewriting_recipe().
  std::unique_ptr<rewriting_output> SPOT_API
  parse_rewriting_output(const char* replacement,
                         rewriting_output::mapper mapper = nullptr)
    throw(parse_error);

  /// \brief Parse a \ref rewriting_recipe that can then be used to
  /// simplify a \ref formula.
  ///
  /// Syntax
  /// ------
  ///
  /// The syntax is quite similar to the one recognized by \ref parse_formula,
  /// although some changes have been introduced.
  ///
  /// - Identifiers must all start with \c f, \c g, or \c h. Other identifiers
  ///   may start with \c e (for purely eventual formulas), \c u (for purely
  ///   universal formulas), \c q (for pure eventualities that are also purely
  ///   universal), \c b (for boolean formulas) or \c r (for SERE formulas).
  ///   Furthermore, more advanced identifiers may be enclosed in strings.
  /// - Alternative patterns can be specified using the 'or' binary operator;
  ///   for instance, 'Ff or Gf' will match both 'Ff' and 'Gf'.
  /// - Sequences of formulas in a n-ary operator (such as '&' and ';') can be
  ///   matched and output using two different syntaxes:
  ///   * The "binary-like" syntax, that only matches pairs of elements. For
  ///     example, 'Ff & Gg' matches both 'Fa & Gb' and 'Fa & Fb & Gc & Gd', but
  ///     not 'Fa & Fb & Gc'.
  ///   * The "repeating" syntax, that matches many elements having the same
  ///     pattern. For example, 'GFf | ..' matches both 'GFa' and 'GFa | GFb',
  ///     but not 'GFa | b'.
  /// - The pattern part of a recipe must be on the left-hand side of an '='
  ///   or '≡' symbol, whereas the output part of the recipe must be on the
  ///   right-hand side.
  /// - Multiple different recipes may be given in any order, as long as they
  ///   are separated by a '/' character. In this case, the recipes
  ///   will be merged into one single \ref rewriting_recipe.
  ///
  /// Additional arguments
  /// --------------------
  /// \param[in] pattern_mapper A function that maps a named pattern into a
  ///                           \ref rewriting_pattern. If \c nullptr is
  ///                           returned, the named pattern will be inserted
  ///                           as is.
  ///
  /// \param[in] output_mapper A function that maps a named output into a
  ///                          \ref rewriting_output. If \c nullptr is returned,
  ///                          the named output will be insereted as is.
  ///
  /// \param[in] predicate A predicate that must be satisfied for the pattern
  ///                      to be considered matching a given formula.
  ///
  /// Examples
  /// --------
  ///
  /// | Recipe                             | Input                                           | Output                        |
  /// | ---------------------------------- | ----------------------------------------------- | ----------------------------- |
  /// | XGFf = GFf                         | XFGa                                            | FGa                           |
  /// | G(f R g) = Gg                      | G(a R b)                                        | Gb                            |
  /// | FGf & .. = FG(f & ..)              | FGa & FGb & FGc                                 | FG(a & b & c)                 |
  /// | r[*0..j] []-> f = r[*1..j] []-> f  | { a[*0..23] } []-> c                            | { a[*1..23] } []-> c          |
  /// | G((GFg or f) \| ..) = (G(f \| ..)) | GF(g \| ..) \| G(a \| b \| c \| GF(d) \| GF(e)) | G(a \| b \| c) \| GF(d \| e)  |
  ///
  /// For more examples, please see tests/python/rewrite.py, which contains the
  /// implementation of the lt.pdf simplification rules as well as tests for
  /// this implementation.
  rewriting_recipe SPOT_API
  parse_rewriting_recipe(const char* rules,
                         rewriting_pattern::mapper pattern_mapper = nullptr,
                         rewriting_output::mapper output_mapper   = nullptr,
                         rewriting_pattern::predicate predicate   = nullptr)
    throw(parse_error);
  
  /// @}


#ifdef SWIGPYTHON
  // Since it's pretty hard to manipulate std::ostream from Python, the
  // following classes are defined here to allow a 'to_string' function
  // to be overriden instead of 'dump'. A fortunate side-effect of doing this
  // is that in Swig, only the 'python_*' classes will need to have directors
  // enabled, which avoids a significant performance loss for the base
  // rewriting_pattern and rewriting_output classes.

  struct SPOT_API python_rewriting_pattern : spot::rewriting_pattern
  {
    virtual std::string to_string(unsigned depth, bool is_unique_child) const = 0;

    std::ostream& dump(std::ostream& os, unsigned depth,
                       bool is_unique_child) const override
    {
      return os << to_string(depth, is_unique_child);
    }
  };

  struct SPOT_API python_rewriting_output : spot::rewriting_output
  {
    virtual std::string to_string(bool parens) const = 0;

    std::ostream& dump(std::ostream& os, bool parens) const override
    {
      return os << to_string(parens);
    }
  };
#endif
}
