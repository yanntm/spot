// -*- coding: utf-8 -*-
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

#include "config.h"

#include <cstring>
#include <map>
#include <vector>
#include <spot/tl/contain.hh>
#include <spot/tl/rewrite.hh>

// Structure of this file:
// - Utilities
// - Built-in rewriting patterns
// - Built-in rewriting outputs
// - Parser class
// - Parsing logic (parse, try_parse_led, parse_nud)
// - Factories (explained with the parser)
// - Exported parse_foo functions

#ifdef TRACE
#define trace std::clog
#else
#define trace while (0) std::clog
#endif

#define CASE_END   \
    case '=':       \
    case '/'

#define CASE_CONST  \
    case op::ff:    \
    case op::tt:    \
    case op::eword

#define CASE_UNARY              \
    case op::Not:               \
    case op::X:                 \
    case op::F:                 \
    case op::G:                 \
    case op::Closure:           \
    case op::NegClosure:        \
    case op::NegClosureMarked

#define CASE_BINARY           \
    case op::Xor:             \
    case op::Implies:         \
    case op::Equiv:           \
    case op::U:               \
    case op::R:               \
    case op::W:               \
    case op::M:               \
    case op::EConcat:         \
    case op::EConcatMarked:   \
    case op::UConcat

#define CASE_NARY         \
    case op::Or:          \
    case op::OrRat:       \
    case op::And:         \
    case op::AndRat:      \
    case op::AndNLM:      \
    case op::Concat:      \
    case op::Fusion

#define CASE_STAR     \
    case op::Star:    \
    case op::FStar

namespace spot
{
  namespace
  {
    struct or_pattern;

    // Miscellaneous utils

    /// Write `n` times the character `c` to the given stream.
    inline void writen(std::ostream& os, char c, unsigned n)
    {
      std::fill_n(std::ostream_iterator<char>(os), n, c);
    }

    /// Check for bitwise equality.
    template<typename T>
    inline bool bitwise_eq(const T* a, const T* b)
    {
      return memcmp(a, b, sizeof(T));
    }

    using patterns = rewriting_recipe::patterns_type;

    /// Advance to the next state using any of the provided patterns.
    ///
    /// If any of the given patterns succeeds, it will be returned.
    /// Otherwise, `rewriting_match::nomatch()` will be returned.
    rewriting_match try_match(const patterns& tr,
                              const formula& f,
                              rewriting_match match)
    {
      for (auto& trans: tr)
        {
          auto newState = trans->match(match, f);

          if (newState)
            return newState;
        }

      return rewriting_match::nomatch();
    }

    /// Merge the given patterns together, which may reduce the size of the
    /// given vector.
    void merge_patterns(patterns& patterns);

    /// Append the vector `b` to the end of the vector `a`. Only `a` will remain
    /// valid after the operation.
    /// This function handles move semantics correctly and attempts to
    /// perform the smallest allocation possible.
    template<typename T>
    inline void append(std::vector<T>& a, std::vector<T>&& b)
    {
      if (a.size() > b.size()) {
        a.insert(a.end(), std::make_move_iterator(b.begin()),
                          std::make_move_iterator(b.end()));
      } else {
        b.insert(b.end(), std::make_move_iterator(a.begin()),
                          std::make_move_iterator(a.end()));

        a.swap(b);
      }

      b.clear();
    }


    /// Boundary of the minimum or maximum value of a bound unary node.
    class boundary final
    {
      bool has_value_;

      union
      {
        uint8_t value_;
        std::unique_ptr<char[]> name_;
      };

    public:
      /// Create a new boundary with a constant value.
      boundary(uint8_t value)
        : has_value_(true), value_(value)
      {}

      /// Create a new boundary with a variable value, and that can be refered
      /// to using the given name.
      boundary(std::unique_ptr<char[]> name)
        : has_value_(false), name_(std::move(name))
      {}

      boundary(boundary&& other) : has_value_(other.has_value_), name_(nullptr)
      {
        if (has_value_)
          value_ = other.value_;
        else
          name_ = std::move(other.name_);
      }

      boundary& operator=(boundary&& other)
      {
        this->~boundary();

        has_value_ = other.has_value_;

        if (has_value_)
          value_ = other.value_;
        else
          name_ = std::move(other.name_);
        
        return *this;
      }

      ~boundary()
      {
        if (!has_value_)
          name_.~unique_ptr();
      }

      /// Return whether the boundary has a constant value, in which case
      /// \ref value can be safely accessed, but \ref name_hash not.
      bool has_value() const { return has_value_; }

      /// Return whether the boundary has a constant value, in which case
      /// \a out_value will be set to it. Otherwise, \a out_name_hash will be set
      /// to said name.
      bool has_value(uint8_t& out_value, const char*& out_name) const
      {
        if (has_value_)
          out_value = value_;
        else
          out_name = name_.get();
        
        return has_value_;
      }

      /// Return the constant value.
      uint8_t value() const throw(std::runtime_error)
      {
        if (!has_value_)
          throw std::runtime_error("The value cannot be retrieved from "
                                  "a named boundary.");
        
        return value_;
      }

      /// Return the name that corresponds to this boundary.
      const char* name() const throw(std::runtime_error)
      {
        if (has_value_)
          throw std::runtime_error("The name hash cannot be retrieved from "
                                  "a constant boundary.");
        
        return name_.get();
      }

      bool is_max() const { return has_value_ && value_ == 255; }
      bool is_min() const { return has_value_ && value_ == 0; }

      std::ostream& dump(std::ostream& os) const
      {
        if (has_value_)
          if (value_ == 255 || value_ == 0)
            return os;
          else
            return os << (int)value_;
        else
          return os << name_.get();
      }
    };

    /// A pattern that accepts multiple patterns.
    struct or_pattern final : rewriting_pattern
    {
      patterns patterns_;

      or_pattern(patterns&& t) : patterns_(std::move(t))
      {
      }

      rewriting_match match(const rewriting_match& previous,
                            const formula& f) const override
      {
        return try_match(patterns_, f, previous);
      }

      bool try_merge(rewriting_pattern& other) override
      {
        or_pattern* orp = dynamic_cast<or_pattern*>(&other);

        append(patterns_, std::move(orp->patterns_));
        merge_patterns(patterns_);

        return true;
      }

      void associate(std::shared_ptr<rewriting_output> o) override
      {
        for (auto& pattern: patterns_)
          pattern->associate(o);
      }

      std::ostream& dump(std::ostream& os, unsigned depth,
                         bool is_unique_child) const override
      {
        is_unique_child = patterns_.size() == 1;
        
        if (!is_unique_child)
          {
            os << std::endl;
            depth += 2;
          }

        for (auto& t: patterns_)
          t->dump(os, depth, is_unique_child);

        return os;
      }
    };

    void merge_patterns(patterns& patterns)
    {
      size_t size = patterns.size();

      for (size_t i = 0; i < size; i++)
        {
          auto& l = patterns[i];
          auto* l_or = dynamic_cast<or_pattern*>(l.get());

          if (l_or)
            {
              auto& or_patterns = l_or->patterns_;

              size += or_patterns.size();

              patterns.erase(patterns.begin() + i);
              patterns.insert(patterns.begin() + i - 1,
                              std::make_move_iterator(or_patterns.begin()),
                              std::make_move_iterator(or_patterns.end()));

              i--;
              continue;
            }

          // Loop from end of vector to current position of i, so
          // if there are merges, we can immediately erase the last element
          // which is an O(1) operation.
          for (size_t j = size - 1; j > i; j--)
            {
              if (!l->try_merge(*patterns[j]))
                continue;

              patterns.erase(patterns.begin() + j);
              size--;
            }
        }
    }

    /// A pattern that accepts a constant formula.
    struct const_pattern final : rewriting_pattern
    {
      const op kind;
      std::shared_ptr<rewriting_output> output;

      const_pattern(op kind) : kind(kind), output(nullptr)
      {
        switch (kind)
          {
          CASE_CONST:
            break;
          default:
            throw std::runtime_error("Invalid kind given to const_pattern.");
          }
      }

      rewriting_match match(const rewriting_match& previous,
                            const formula& f) const override
      {
        if (f.kind() != kind)
          return rewriting_match::nomatch();

        return previous.with_output(output);
      }

      bool try_merge(rewriting_pattern& other) override
      {
        const_pattern* const_ = dynamic_cast<const_pattern*>(&other);

        return const_ && kind != const_->kind
                      && output->hash() == const_->output->hash();
      }

      void associate(std::shared_ptr<rewriting_output> o) override
      {
        output = o;
      }

      std::ostream& dump(std::ostream& os, unsigned depth,
                         bool is_unique_child) const override
      {
        if (!is_unique_child)
          writen(os, ' ', depth);

        switch (kind)
          {
          case op::ff:    return os << '0'   << std::endl;
          case op::tt:    return os << '1'   << std::endl;
          case op::eword: return os << "[*]" << std::endl;
          default:
            SPOT_UNREACHABLE();
          }
      }
    };

    /// A pattern that adds the current formula to the returned previous.
    struct named_pattern final : rewriting_pattern
    {
      const size_t name_hash;
      const char* name;
      const bool own_name;
      std::shared_ptr<rewriting_output> output;

      named_pattern(const char* n, bool own)
        : name_hash(fnv_hash(n)), name(n), own_name(own), output(nullptr)
      {
        if (!name_hash)
          throw std::runtime_error("Invalid name given to named_pattern.");
      }

      ~named_pattern()
      {
        output.~shared_ptr();

        if (own_name)
          delete[] name;
      }

      rewriting_match match(const rewriting_match& previous,
                            const formula& f) const override
      {
        // Check for eventuality / universality / etc for the formulas
        // for which it matters
        if (name[0] == 'e' && !f.is_eventual())
          return rewriting_match::nomatch();
        if (name[0] == 'u' && !f.is_universal())
          return rewriting_match::nomatch();
        if (name[0] == 's' && !(f.is_eventual() && f.is_universal()))
          return rewriting_match::nomatch();
        if (name[0] == 'q' && !(f.is_eventual() && f.is_universal()))
          return rewriting_match::nomatch();
        if (name[0] == 'b' && !f.is_boolean())
          return rewriting_match::nomatch();
        if (name[0] == 'r' && !f.is_sere_formula())
          return rewriting_match::nomatch();

        // Ensure two formulas with the same name are equal
        auto other = previous.try_get_formula(name_hash);

        if (other.first)
          {
            if (other.second != f)
              return rewriting_match::nomatch();
            
            // No need to add the formula and output, since we already did
            // before
            return previous;
          }
        else
          return previous.with_formula(name_hash, f)
                         .with_output(output);
      }

      bool try_merge(rewriting_pattern& other) override
      {
        named_pattern* n = dynamic_cast<named_pattern*>(&other);

        return n && output->hash() == n->output->hash();
      }

      void associate(std::shared_ptr<rewriting_output> o) override
      {
        output = o;
      }

      std::ostream& dump(std::ostream& os, unsigned depth,
                         bool is_unique_child) const override
      {
        if (!is_unique_child)
          writen(os, ' ', depth);

        if (!output)
          return os << name << std::endl;

        return output->dump(os << name << " ≡ ", false) << std::endl;
      }
    };

    /// A pattern from a unary node to its operand.
    struct unary_pattern final : rewriting_pattern
    {
      const op kind;
      patterns patterns_;

      unary_pattern(op o, patterns&& t) : kind(o), patterns_(std::move(t))
      {
        switch (o)
          {
          CASE_UNARY:
          CASE_STAR:
            break;
          
          default:
            throw std::runtime_error("Invalid operator given to "
                                     "unary_pattern.");
          }
      }

      rewriting_match match(const rewriting_match& previous,
                            const formula& f) const override
      {
        if (f.kind() != kind)
          switch (f.kind())
            {
            case op::NegClosure:
            case op::NegClosureMarked:
              if (kind == op::NegClosure)
                break;
              goto nomatch;
            
            nomatch:
            default:
              return rewriting_match::nomatch();
            }
        
        return try_match(patterns_, f[0], previous);
      }

      bool try_merge(rewriting_pattern& other) override
      {
        unary_pattern* un = dynamic_cast<unary_pattern*>(&other);

        if (!un || kind != un->kind)
          return false;

        append(patterns_, std::move(un->patterns_));
        merge_patterns(patterns_);

        return true;
      }

      void associate(std::shared_ptr<rewriting_output> o) override
      {
        for (auto& pattern: patterns_)
          pattern->associate(o);
      }

      std::ostream& dump(std::ostream& os, unsigned depth,
                         bool is_unique_child) const override
      {
        if (!is_unique_child)
          writen(os, ' ', depth);

        switch (kind)
          {
          case op::Not:
            os << '!';
            break;
          case op::X:
            os << 'X';
            break;
          case op::F:
            os << 'F';
            break;
          case op::G:
            os << 'G';
            break;
          case op::Closure:
          case op::NegClosure:
          case op::NegClosureMarked:
            os << "{}";
            break;
          
          default:
            SPOT_UNREACHABLE();
          }
        
        is_unique_child = patterns_.size() == 1;
        
        if (!is_unique_child)
          {
            os << std::endl;
            depth += 2;
          }

        for (auto& t: patterns_)
          t->dump(os, depth, is_unique_child);

        return os;
      }
    };

    /// A pattern from a binary node to its left and right operands.
    struct binary_pattern final : rewriting_pattern
    {
      const op kind;
      patterns left_patterns;
      std::vector<patterns> right_patterns;

      binary_pattern(op o, patterns&& l, patterns&& r)
        : kind(o), left_patterns(std::move(l))
      {
        right_patterns.reserve(1);
        right_patterns.push_back(std::move(r));

        switch (o)
          {
          CASE_BINARY:
          CASE_NARY:
            if (left_patterns.size() != right_patterns[0].size())
              throw std::runtime_error("Invalid number of patterns given to "
                                       "binary_pattern.");
            break;
          
          default:
            throw std::runtime_error("Invalid operator given to "
                                     "binary_pattern.");
          }
      }

      rewriting_match match(const rewriting_match& previous,
                            const formula& f) const override
      {
        if (f.kind() != kind)
            switch (f.kind())
            {
            case op::Concat:
            case op::EConcat:
            case op::EConcatMarked:
            case op::UConcat:
              if (kind == op::Concat)
                break;
              goto nomatch;
            
            case op::And:
            case op::AndNLM:
            case op::AndRat:
              if (kind == op::And)
                break;
              goto nomatch;
            
            case op::Or:
            case op::OrRat:
              if (kind == op::Or)
                break;
              goto nomatch;
            
            nomatch:
            default:
              return rewriting_match::nomatch();
            }
        
        bool commutes;
        
        switch (f.kind())
          {
          CASE_BINARY:
            commutes = false;
            goto binary;

          binary:
            {
              for (size_t i = 0; i < left_patterns.size(); i++)
                {
                  auto lmatch = left_patterns[i]->match(previous, f[0]);

                  if (!lmatch)
                    {
                      if (commutes)
                        {
                          lmatch = left_patterns[i]->match(previous, f[1]);

                          if (lmatch)
                            {
                              auto rmatch_ = try_match(right_patterns[i], f[0],
                                                       lmatch);

                              if (rmatch_)
                                return rmatch_;
                            }
                        }

                      continue;
                    }
                  
                  auto rmatch = try_match(right_patterns[i], f[1], lmatch);

                  if (!rmatch)
                    continue;
                  
                  return rmatch;
                }
              
              return rewriting_match::nomatch();
            }
          
          case op::Or:
          case op::OrRat:
          case op::And:
          case op::AndRat:
          case op::AndNLM:
            commutes = true;
            goto nary;

          case op::Concat:
          case op::Fusion:
            commutes = false;
            goto nary;

          nary:
            {
              size_t size = f.size();

              if (size == 2)
                goto binary;
              
              // Unfortunately C++ doesn't detect that f.begin()..f.end() is
              // an iterator of formulas, so we have to manually copy elements.
              std::vector<formula> tmp;

              tmp.reserve(f.size());
              
              for (auto child: f)
                tmp.push_back(child);

              std::vector<rewriting_match> matches;
              std::shared_ptr<rewriting_output> output;

              size_t pattern_count = left_patterns.size();

              for (size_t i = 0; i < size; i++)
              for (size_t pattern_n = 0; pattern_n < pattern_count; pattern_n++)
                {
                  auto lmatch = left_patterns[pattern_n]->match(previous, tmp[i]);

                  if (!lmatch)
                    continue;

                  for (auto& r_pattern: right_patterns[pattern_n])
                  for (size_t j = commutes ? 0 : i + 1; j < size; j++)
                    {
                      if (!commutes && j == i)
                        continue;
                      
                      auto rmatch = r_pattern->match(lmatch, tmp[j]);

                      if (!rmatch)
                        continue;
                      
                      // Both the left and right rules have succeeded, mark
                      // the whole operation as a success and try the next pair.
                      matches.push_back(rmatch);

                      // Note that we only push the right match, since it has
                      // all the data in the left match anyway

                      if (!output)
                        output = rmatch.try_get_output().second;

                      // We can also remove both items, as we don't want to
                      // duplicate them.
                      if (i == j - 1)
                        // Erase range directly to avoid moving content of
                        // vector twice when erasing
                        tmp.erase(tmp.begin() + i, tmp.begin() + i + 2);
                      else if (i == j + 1)
                        // Bis.
                        tmp.erase(tmp.begin() + j, tmp.begin() + j + 2);
                      else
                        {
                          tmp.erase(tmp.begin() + i);

                          if (j > i)
                            tmp.erase(tmp.begin() + j - 1);
                          else
                            tmp.erase(tmp.begin() + j);
                        }
                      
                      // We want to continue to the next items, that overrode
                      // the current ones, so we nullify the value of i and j
                      size -= 2;
                      i--;

                      goto next_left_iter;
                    }

                  next_left_iter: ;
                }

              return size > 0
                ? rewriting_match::nomatch() // not all children were matched,
                                             // and we consider this a failure
                : previous.with_children(std::move(matches))
                          .with_output(output);
            }
          
          default:
            SPOT_UNREACHABLE();
          }
      }

      bool try_merge(rewriting_pattern& other) override
      {
        binary_pattern* bin = dynamic_cast<binary_pattern*>(&other);

        if (!bin || kind != bin->kind)
          return false;
        
        size_t size = left_patterns.size() + bin->left_patterns.size();

        if (size < 2)
          return false;

        append(left_patterns, std::move(bin->left_patterns));
        append(right_patterns, std::move(bin->right_patterns));

        for (size_t i = 0; i < size; i++)
          {
            auto& l = left_patterns[i];

            // Loop from end of vector to current position of i, so
            // if there are merges, we can immediately erase the last element
            // which is an O(1) operation.
            for (size_t j = size - 1; j > i; j--)
              {
                if (!l->try_merge(*left_patterns[j]))
                  continue;
                
                append(right_patterns[i], std::move(right_patterns[j]));
                merge_patterns(right_patterns[i]);

                left_patterns.erase(left_patterns.begin() + j);
                right_patterns.erase(right_patterns.begin() + j);
                
                size--;
              }
          }

        return true;
      }

      void associate(std::shared_ptr<rewriting_output> o) override
      {
        for (auto& pattern: left_patterns)
          pattern->associate(o);

        for (auto& pattern_vector: right_patterns)
          for (auto& pattern: pattern_vector)
            pattern->associate(o);
      }

      std::ostream& dump(std::ostream& os, unsigned depth,
                         bool is_unique_child) const override
      {
        os << std::endl;

        if (is_unique_child)
          // Depth did not increase since we're a unique child, fix it
          depth += 2;

        writen(os, ' ', depth);

        unsigned len = 1;
          
        switch (kind)
          {
          case op::Xor:
            os << '^';
            break;
          case op::Implies:
            os << "==>";
            len = 3;
            break;
          case op::Equiv:
            os << "<=>";
            len = 3;
            break;
          case op::U:
            os << 'U';
            break;
          case op::R:
            os << 'R';
            break;
          case op::W:
            os << 'W';
            break;
          case op::M:
            os << 'M';
            break;
          case op::EConcat:
          case op::EConcatMarked:
            os << "<>->";
            break;
          case op::UConcat:
            os << "[]->";
            break;

          case op::Or:
          case op::OrRat:
            os << '|';
            break;
          case op::And:
          case op::AndRat:
          case op::AndNLM:
            os << '&';
            break;
          case op::Concat:
            os << ';';
            break;
          case op::Fusion:
            os << ':';
            break;
          
          default:
            SPOT_UNREACHABLE();
          }
        
        os << ' ';

        unsigned children_depth = depth + len + 1;
        size_t size = left_patterns.size();
        
        // Left children
        is_unique_child = left_patterns.size() == 1;

        if (!is_unique_child)
          {
            os << std::endl;
            depth = children_depth;
          }

        for (size_t i = 0; i < size; i++)
          {
            // Left child
            left_patterns[i]->dump(os, children_depth, is_unique_child);

            auto& right_trans = right_patterns[i];

            for (auto& trans: right_trans)
              {
                // Separator
                writen(os, ' ', depth + 1);
                os << '\\';
                writen(os, ' ', len);

                // Right child
                trans->dump(os, children_depth + 1, true);
              }
          }

        return os;
      }
    };

    /// A pattern from a star operator to its operand.
    ///
    /// The value of the minimum and maximum accepted values will be recorded.
    struct star_pattern final : rewriting_pattern
    {
      const op kind;
      const boundary min, max;
      patterns patterns_;

      star_pattern(op o, patterns&& t, boundary min, boundary max)
        : kind(o)
        , min(std::move(min)), max(std::move(max))
        , patterns_(std::move(t))
      {
        if (o != op::Star && o != op::FStar)
          throw std::runtime_error("Invalid operator given to star_pattern.");
      }

      rewriting_match match(const rewriting_match& previous,
                            const formula& f) const override
      {
        if (f.kind() != kind)
          return rewriting_match::nomatch();
        
        uint8_t min_value, max_value;
        const char* min_name = nullptr, *max_name = nullptr;

        if (min.has_value(min_value, min_name) && f.min() != min_value)
          return rewriting_match::nomatch();
        
        if (max.has_value(max_value, max_name) && f.max() != max_value)
          return rewriting_match::nomatch();
        
        rewriting_match res = try_match(patterns_, f[0], previous);

        if (min_name)
          res = res.with_value(min_name, f.min());
        if (max_name)
          res = res.with_value(max_name, f.max());

        return res;
      }

      bool try_merge(rewriting_pattern& other) override
      {
        star_pattern* s = dynamic_cast<star_pattern*>(&other);

        if (!s || s->kind != kind)
          return false;
        
        uint8_t our_min_value = 0,
                our_max_value = 0,
                their_min_value = 0,
                their_max_value = 0;
        const char *our_min_name = nullptr,
                   *our_max_name = nullptr,
                   *their_min_name = nullptr,
                   *their_max_name = nullptr;

        // Make sure the bounds are equal
        if (min.has_value(our_min_value, our_min_name) !=
            s->min.has_value(their_min_value, their_min_name) ||
            max.has_value(our_max_value, our_max_name) !=
            s->max.has_value(their_max_value, their_max_name))
          return false;
        
        if (our_min_value != their_min_value ||
            our_max_value != their_max_value ||
            our_min_name != their_min_name ||
            our_max_name != their_max_name)
          return false;

        // The bounds are exactly the same, we can keep going
        append(patterns_, std::move(s->patterns_));
        merge_patterns(patterns_);

        return true;
      }

      void associate(std::shared_ptr<rewriting_output> o) override
      {
        for (auto& pattern: patterns_)
          pattern->associate(o);
      }

      std::ostream& dump(std::ostream& os, unsigned depth,
                         bool is_unique_child) const override
      {
        if (!is_unique_child)
          writen(os, ' ', depth);

        switch (kind)
          {
          case op::Star:
            os << "[*";
            break;
          case op::FStar:
            os << "[*:";
            break;
          
          default:
            SPOT_UNREACHABLE();
          }
        
        if (min.is_min() && max.is_max())
          os << "]";
        else
          {
            min.dump(os) << "..";
            max.dump(os) << "]";
          }

        is_unique_child = patterns_.size() == 1;

        if (!is_unique_child)
          {
            os << std::endl;
            depth += 2;
          }
        
        for (auto& t: patterns_)
          t->dump(os, depth, is_unique_child);
        
        return os;
      }
    };

    /// A pattern from a n-ary operator to its children.
    struct nary_pattern final : rewriting_pattern
    {
      const op kind;
      patterns patterns_;

      nary_pattern(op o, patterns&& t) : kind(o), patterns_(std::move(t))
      {
        switch (o)
          {
          CASE_NARY:
            break;
          
          default:
            throw std::runtime_error("Invalid operator given to nary_pattern.");
          }
      }

      rewriting_match match(const rewriting_match& previous,
                            const formula& f) const override
      {
        if (f.kind() != kind)
          return rewriting_match::nomatch();
        
        std::vector<rewriting_match> matches;
        std::shared_ptr<rewriting_output> output;
        
        for (auto child: f)
          {
            auto match = try_match(patterns_, child, previous);

            if (!match)
              return rewriting_match::nomatch();
            if (!output)
              output = match.try_get_output().second;
            
            matches.push_back(match);
          }

        return previous.with_children(std::move(matches))
                       .with_output(std::move(output));
      }

      bool try_merge(rewriting_pattern& other) override
      {
        nary_pattern* n = dynamic_cast<nary_pattern*>(&other);

        if (!n || kind != n->kind)
          return false;

        append(patterns_, std::move(n->patterns_));
        merge_patterns(patterns_);

        return true;
      }

      void associate(std::shared_ptr<rewriting_output> o) override
      {
        for (auto& pattern: patterns_)
          pattern->associate(o);
      }

      std::ostream& dump(std::ostream& os, unsigned depth, bool is_unique_child) const override
      {
        if (!is_unique_child)
          writen(os, ' ', depth);

        switch (kind)
          {
          case op::Or:
          case op::OrRat:
            os << " | ..";
            break;
          case op::And:
          case op::AndRat:
          case op::AndNLM:
            os << " & ..";
            break;
          case op::Concat:
            os << " ; ..";
            break;
          case op::Fusion:
            os << " : ..";
            break;
          
          default:
            SPOT_UNREACHABLE();
          }
        
        is_unique_child = patterns_.size() == 1;

        if (!is_unique_child)
          {
            os << std::endl;
            depth += 2;
          }
        
        for (auto& t: patterns_)
          t->dump(os, depth, is_unique_child);
        
        return os;
      }
    };

    /// A pattern that must satisfy a condition.
    struct predicate_pattern final : rewriting_pattern
    {
      const rewriting_pattern::predicate predicate;
      const std::unique_ptr<rewriting_pattern> pattern;

      predicate_pattern(rewriting_pattern::predicate p,
                        std::unique_ptr<spot::rewriting_pattern> t)
        : predicate(p), pattern(std::move(t))
      {
        SPOT_ASSERT(pattern);
      }

      rewriting_match match(const rewriting_match& previous,
                            const formula& f) const override
      {
        rewriting_match match = pattern->match(previous, f);

        if (!match || !predicate(match))
          return rewriting_match::nomatch();
        
        return match;
      }

      bool try_merge(rewriting_pattern& other) override
      {
        predicate_pattern* p = dynamic_cast<predicate_pattern*>(&other);

        if (!p || bitwise_eq(&predicate, &p->predicate))
          return false;

        return pattern->try_merge(*p->pattern);
      }

      void associate(std::shared_ptr<rewriting_output> o) override
      {
        pattern->associate(o);
      }

      std::ostream& dump(std::ostream& os, unsigned depth,
                         bool is_unique_child) const override
      {
        return pattern->dump(os, depth, is_unique_child);
      }
    };

    /// A pattern that introduces a new scope.
    struct scope_pattern final : rewriting_pattern
    {
      const size_t scope_name;
      const std::unique_ptr<rewriting_pattern> pattern;

      scope_pattern(std::unique_ptr<spot::rewriting_pattern> t, size_t n)
        : scope_name(n), pattern(std::move(t))
      {
        SPOT_ASSERT(pattern);
      }

      rewriting_match match(const rewriting_match& previous,
                            const formula& f) const override
      {
        rewriting_match root = rewriting_match(previous.recipe());
        rewriting_match match = pattern->match(root, f);

        if (!match)
          return rewriting_match::nomatch();
        
        return previous.with_scope(scope_name, match);
      }

      bool try_merge(rewriting_pattern&) override
      {
        return false;
      }

      void associate(std::shared_ptr<rewriting_output> o) override
      {
        pattern->associate(o);
      }

      std::ostream& dump(std::ostream& os, unsigned depth,
                         bool is_unique_child) const override
      {
        return pattern->dump(os, depth, is_unique_child);
      }
    };


    /// A output that returns a constant.
    struct const_output final : rewriting_output
    {
      const op kind;

      const_output(op kind) : kind(kind)
      {
        switch (kind)
          {
          CASE_CONST:
            break;
          default:
            throw std::runtime_error("Invalid kind given to const_output.");
          }
      }

      std::vector<formula> create(const rewriting_match&) const override
      {
        switch (kind)
          {
          case op::ff:
            return { formula::ff() };
          case op::tt:
            return { formula::tt() };
          case op::eword:
            return { formula::eword() };
          default:
            SPOT_UNREACHABLE();
          }
      }

      std::ostream& dump(std::ostream& os, bool) const override
      {
        switch (kind)
          {
          case op::ff:
            return os << '0';
          case op::tt:
            return os << '1';
          case op::eword:
            return os << "[*0]";
          default:
            SPOT_UNREACHABLE();
          }
      }

      size_t compute_hash() const override
      {
        size_t hash = fnv<size_t>::init;

        hash ^= (size_t)kind;
        hash *= fnv<size_t>::prime;

        return hash;
      }
    };

    /// A output that restores a saved formula.
    struct named_output final : rewriting_output
    {
      const size_t name_hash;
      const char* name;
      const bool own_name;

      named_output(const char* n, bool own)
        : name_hash(fnv_hash(n)), name(n), own_name(own)
      {
        if (!name_hash)
          throw std::runtime_error("Invalid name given to named_output.");
      }

      ~named_output()
      {
        if (own_name)
          delete[] name;
      }

      std::vector<formula> create(const rewriting_match& match) const override
      {
        std::vector<formula> formulas;
        auto it = match.formulas(name_hash);

        while (++it != match.end<formula>())
          formulas.push_back(*it);

        trace << "[OUT] Found " << formulas.size() << " formula(s) named '"
              << name << "'." << std::endl;

        return formulas;
      }

      std::ostream& dump(std::ostream& os, bool) const override
      {
        return os << name;
      }

      size_t compute_hash() const override
      {
        return 0x47; // name does not matter
      }
    };

    /// A output that creates a unary formula.
    struct unary_output final : rewriting_output
    {
      const op kind;
      std::unique_ptr<rewriting_output> output;

      unary_output(op o, std::unique_ptr<rewriting_output> inner)
        : kind(o), output(std::move(inner))
      {
        switch (kind)
          {
          CASE_UNARY:
            break;
          default:
            throw std::runtime_error("Given operator is not a unary operator.");
          }
        
        if (!output)
          throw std::runtime_error("Operand output cannot be null.");
      }

      std::vector<formula> create(const rewriting_match& match) const override
      {
        std::vector<formula> formulas = output->create(match);

        for (size_t i = 0; i < formulas.size(); i++)
          formulas[i] = formula::unop(kind, std::move(formulas[i]));
        
        return formulas;
      }

      std::ostream& dump(std::ostream& os, bool) const override
      {
        switch (kind)
          {
          case op::Not:
            os << '!';
            break;
          case op::X:
            os << 'X';
            break;
          case op::F:
            os << 'F';
            break;
          case op::G:
            os << 'G';
            break;
          case op::Closure:
          case op::NegClosure:
          case op::NegClosureMarked:
            os << "{}";
            break;
          
          default:
            SPOT_UNREACHABLE();
          }
        
        return output->dump(os, true);
      }

      size_t compute_hash() const override
      {
        size_t hash = output->hash();

        hash ^= (size_t)kind;
        hash *= fnv<size_t>::prime;

        return hash;
      }
    };

    /// A output that creates a binary formula.
    struct binary_output final : rewriting_output
    {
      const op kind;
      std::unique_ptr<rewriting_output> left_output;
      std::unique_ptr<rewriting_output> right_output;

      binary_output(op o, std::unique_ptr<rewriting_output> l,
                          std::unique_ptr<rewriting_output> r)
        : kind(o), left_output(std::move(l)), right_output(std::move(r))
      {
        switch (o)
          {
          CASE_BINARY:
          CASE_NARY:
            break;
          default:
            throw std::runtime_error("Given operator is not a binary operator");
          }

        if (!left_output)
          throw std::runtime_error("Left operand output cannot be null.");
        if (!right_output)
          throw std::runtime_error("Right operand output cannot be null.");
      }

      std::vector<formula> create(const rewriting_match& match) const override
      {
        std::vector<formula> l = left_output->create(match);
        std::vector<formula> r = right_output->create(match);

        if (l.size() == 0 || r.size() == 0)
          return {};
        if (l.size() != r.size())
          throw std::runtime_error("Invalid left and right formulas "
                                   "returned.");

        switch (kind)
          {
          CASE_BINARY:
            for (size_t i = 0; i < l.size(); i++)
              l[i] = formula::binop(kind, std::move(l[i]), std::move(r[i]));
            
            return l;

          CASE_NARY:
            for (size_t i = 0; i < l.size(); i++)
              l[i] = formula::multop(kind, { std::move(l[i]), std::move(r[i]) });
            
            if (l.size() == 1)
              return l;
            else
              // When multiple different n-ary element match, we want to
              // merge them together into one at the end
              return { formula::multop(kind, std::move(l)) };

          default:
            SPOT_UNREACHABLE();
          }
      }

      std::ostream& dump(std::ostream& os, bool parens) const override
      {
        if (parens) os << "(";

        left_output->dump(os, true);

        switch (kind)
          {
          case op::Xor:
            os << " ^ ";
            break;
          case op::Implies:
            os << " ==> ";
            break;
          case op::Equiv:
            os << " <=> ";
            break;
          case op::U:
            os << " U ";
            break;
          case op::R:
            os << " R ";
            break;
          case op::W:
            os << " W ";
            break;
          case op::M:
            os << " M ";
            break;
          case op::EConcat:
          case op::EConcatMarked:
            os << " <>-> ";
            break;
          case op::UConcat:
            os << " []-> ";
            break;

          case op::Or:
          case op::OrRat:
            os << " | ";
            break;
          case op::And:
          case op::AndRat:
          case op::AndNLM:
            os << " & ";
            break;
          case op::Concat:
            os << " ; ";
            break;
          case op::Fusion:
            os << " : ";
            break;
          
          default:
            SPOT_UNREACHABLE();
          }

        right_output->dump(os, true);

        if (parens) os << ")";

        return os;
      }

      size_t compute_hash() const override
      {
        size_t hash = left_output->hash();

        hash ^= (size_t)kind;
        hash *= fnv<size_t>::prime;

        hash ^= right_output->hash();
        hash *= fnv<size_t>::prime;

        return hash;
      }
    };

    /// A output that creates a star formula.
    struct star_output final : rewriting_output
    {
      const op kind;
      const boundary min, max;

      std::unique_ptr<rewriting_output> inner;

      star_output(op o, std::unique_ptr<rewriting_output> i,
                  boundary min, boundary max)
        : kind(o)
        , min(std::move(min)), max(std::move(max))
        , inner(std::move(i))
      {
        switch (o)
          {
          CASE_STAR:
            break;
          default:
            throw std::runtime_error("Given operator is not a star operator");
          }

        if (!inner)
          throw std::runtime_error("Operand output cannot be null.");
      }

      std::vector<formula> create(const rewriting_match& match) const override
      {
        std::vector<formula> formulas = inner->create(match);
        
        uint8_t min_value, max_value;
        const char* name;

        if (!min.has_value(min_value, name))
          {
            auto min_value_pair = match.try_get_value(name);

            if (min_value_pair.first)
              min_value = (uint8_t)min_value_pair.second;
            else
              return {};
          }

        if (!max.has_value(max_value, name))
          {
            auto max_value_pair = match.try_get_value(name);

            if (max_value_pair.first)
              max_value = (uint8_t)max_value_pair.second;
            else
              return {};
          }

        for (size_t i = 0; i < formulas.size(); i++)
          formulas[i] = formula::bunop(kind, std::move(formulas[i]),
                                       min_value, max_value);
        
        return formulas;
      }

      std::ostream& dump(std::ostream& os, bool) const override
      {
        return inner->dump(os, true) << (kind == op::FStar ? "[:*]" : "[*]");
      }

      size_t compute_hash() const override
      {
        size_t hash = inner->hash();

        hash ^= (size_t)kind;
        hash *= fnv<size_t>::prime;

        return hash;
      }
    };

    /// An output that creates an n-ary formula.
    struct nary_output final : rewriting_output
    {
      const op kind;
      std::unique_ptr<rewriting_output> output;

      nary_output(op o, std::unique_ptr<rewriting_output> inner)
        : kind(o), output(std::move(inner))
      {
        switch (o)
          {
          CASE_NARY:
            break;
          default:
            throw std::runtime_error("Given operator is not an n-ary "
                                     "operator.");
          }

        if (!output)
          throw std::runtime_error("Operand output cannot be null.");
      }

      std::vector<formula> create(const rewriting_match& match) const override
      {
        std::vector<formula> formulas = output->create(match);

        if (formulas.empty())
          return formulas;
        
        return { formula::multop(kind, std::move(formulas)) };
      }

      std::ostream& dump(std::ostream& os, bool parens) const override
      {
        if (parens) os << "(";

        output->dump(os, parens);

        switch (kind)
          {
          case op::Or:
          case op::OrRat:
            os << " | ..";
            break;
          case op::And:
          case op::AndRat:
          case op::AndNLM:
            os << " & ..";
            break;
          case op::Concat:
            os << " ; ..";
            break;
          case op::Fusion:
            os << " : ..";
            break;
          
          default:
            SPOT_UNREACHABLE();
          }

        if (parens) os << ")";

        return os;
      }
  
      size_t compute_hash() const override
      {
        size_t hash = output->hash();

        hash ^= (size_t)kind;
        hash *= fnv<size_t>::prime;

        return hash;
      }
    };

    /// An output that maps a formula to another formula, given a function.
    struct mapping_output final : rewriting_output
    {
      const std::function<formula(formula&)> map;
      const std::unique_ptr<rewriting_output> inner;

      mapping_output(std::unique_ptr<rewriting_output> inner_output,
                    std::function<formula(formula&)> map_fn)
        : map(map_fn), inner(std::move(inner_output))
      {
      }

      std::vector<formula> create(const rewriting_match& match) const override
      {
        std::vector<formula> formulas = inner->create(match);

        for (size_t i = 0; i < formulas.size();)
          {
            formula h = map(formulas[i]);

            if (!h)
              formulas.erase(formulas.begin() + i);
            else
              formulas[i++] = h;
          }
        
        return formulas;
      }

      std::ostream& dump(std::ostream& os, bool parens) const override
      {
        return inner->dump(os, parens);
      }

      size_t compute_hash() const override
      {
        size_t hash = inner->hash();

        hash ^= 0x324589;
        hash *= fnv<size_t>::prime;

        return hash;
      }
    };

    /// An output that only finds matches in a specific scope.
    struct scope_output final : rewriting_output
    {
      const size_t scope_name;
      const std::unique_ptr<rewriting_output> inner;

      scope_output(std::unique_ptr<rewriting_output> inner_output,
                   size_t scope_name)
        : scope_name(scope_name), inner(std::move(inner_output))
      {
      }

      std::vector<formula> create(const rewriting_match& match) const override
      {
        auto scope = match.try_get_scope(scope_name);
        
        if (!scope.first)
          return {};
        
        return inner->create(scope.second);
      }

      std::ostream& dump(std::ostream& os, bool parens) const override
      {
        return inner->dump(os, parens);
      }

      size_t compute_hash() const override
      {
        size_t hash = inner->hash();

        hash ^= 0x64829302;
        hash *= fnv<size_t>::prime;

        return hash;
      }
    };


    // PARSING
    // The syntax for patterns and outputs is (almost) identical, but they're
    // represented by two completely different classes.
    // Thus, we use templates and a 'factory' to create them.
    // For instance, when a unary node 'F' is encountered, we'll return
    // `factory.unary(op::ff, parse(..))`, with factory a template that returns
    // a std::unique_ptr<T>, with T either rewriting_pattern or
    // rewriting_output.

    struct parser final
    {
    private:
      // Source string
      const char* src;
      // Current position
      size_t ofs;
      // Balance
      position p;
      // Errors
      parse_error_list errors;

    public:
      // Parentheses / closures balance
      int pbal, cbal;

      parser(const char* src) : src(src)
      {
        ofs = 0;
        pbal = cbal = 0;
      }

      /// Resets the data stored when parsing a single recipe.
      inline void reset_single_recipe_state()
      {
        pbal = cbal = 0;
      }

      inline const parse_error_list& error_list() const { return errors; }

      inline size_t offset() const       { return ofs; }
      inline const char* source() const  { return src; }
      inline char source(size_t i) const { return src[i]; }
      inline position& pos()             { return p; }

      /// Returns the current character.
      inline char current(int relative_offset = 0) const
      {
        return src[ofs + relative_offset];
      }

      /// Advances to the next character, and returns it.
      inline char next()
      {
        if (src[ofs] == 0)
          return 0;
        
        p.columns();
        return src[++ofs];
      }

      /// Returns the current character, and advanced by a given number of
      /// characters.
      inline char advance(int by = 1)
      {
        char c = src[ofs];

        if (c == 0 && by > 0)
          return c;

        ofs += by;

        if (src[ofs] == 0)
          return c;

        p.columns(by);

        return c;
      }

      /// Return whether the string at the current position matches the given
      /// string. If so, it will be skipped.
      inline bool match(const char* str, bool skip = true)
      {
        if (strncmp(src + ofs, str, strlen(str)) != 0)
          return false;
        
        if (skip)
          {
            auto len = strlen(str);

            ofs += len;
            p.columns(len);
          }
        return true;
      }

      /// Return whether the character at the current position matches the
      /// given character. If so, it will be skipped.
      inline bool match(char ch, bool skip = true)
      {
        if (src[ofs] != ch)
          return false;
        
        if (skip)
          {
            ofs++;
            p.columns();
          }
        return true;
      }

    private:
      template<typename T, typename... Args> inline static void
      append_string(std::string& s, T first, Args... args)
      {
        s += first;
        append_string(s, args...);
      }
      template<typename T> inline static void
      append_string(std::string& s, T first)
      {
        s += first;
      }

    public:
      template<typename... Args>
      inline void report(location l, Args... args)
      {
        std::string s;

        append_string(s, args...);

        errors.emplace_back(l, std::move(s));
      }
      inline void report(location l, std::string&& err)
      {
        errors.emplace_back(l, std::move(err));
      }

      inline void report_unexpected_eof()
      {
        errors.emplace_back(location(p), "Unexpected end-of-file.");
      }

      inline void check_balance(location loc)
      {
        if (pbal != 0)
          report(std::move(loc), "Unmatched parentheses in expression.");
        if (cbal != 0)
          report(std::move(loc), "Unmatched brackets in expression.");
      }

      /// Build the error message and return a parse_error.
      parse_error build_error() const
      {
        // Format errors in a friendly format (here's hoping there isn't more
        // than one line)
        std::ostringstream os;

        for (auto err: error_list())
          {
            auto l = err.first;

            os << ">>> " << source() << std::endl;

            for (unsigned i = 1; i < l.begin.column + 4; i++)
              os << ' ';

            os << '^';

            for (unsigned i = l.begin.column; i < l.end.column; i++)
              os << '^';

            os << std::endl << err.second << std::endl << std::endl;
          }

        return parse_error(os.str());
      }

      /// Skip the next whitespace characters, and return whether there is more
      /// input to process.
      bool skip_whitespace()
      {
        for (;;)
          switch (src[ofs])
            {
            case ' ':
              {
                ofs++;
                p.columns();
                break;
              }

            case '\n':
              {
                ofs++;
                p.lines();
                break;
              }

            case 0:
            CASE_END:
              return false;

            default:
              return true;
            }
      }
    };


    int
    parse_number(parser& parser)
    {
      char firstc = parser.current();

      if (firstc < '0' || firstc > '9')
        return -1;
      
      int n = firstc - '0';

      for (;;)
        {
          char c = parser.next();

          if (c >= '0' && c <= '9')
            {
              n = (n * 10) + (c - '0');
              continue;
            }
          
          return n;
        }
    }

    std::unique_ptr<char[]>
    parse_identifier(parser& parser)
    {
      parser.advance(-1);

      size_t start = parser.offset();

      for (;;)
        {
          char c = parser.next();

          if (c >= 'a' && c <= 'z') continue;
          if (c >= 'A' && c <= 'Z') continue;
          if (c >= '0' && c <= '9') continue;

          // not an identifier, return

          size_t len = parser.offset() - start;
          std::unique_ptr<char[]> s = std::make_unique<char[]>(len + 1);

          memcpy(s.get(), parser.source() + start, len);

          s[len] = 0;

          return s;
        }
    }

    std::unique_ptr<char[]>
    parse_quoted_identifier(parser& parser)
    {
      size_t start = parser.offset();

      for (;;)
        {
          char c = parser.advance();

          switch (c)
            {
            case 0:
            case '"':
              {
                size_t len = parser.offset() - start - 1;
                std::unique_ptr<char[]> s = std::make_unique<char[]>(len + 1);

                memcpy(s.get(), parser.source() + start, len);

                s[len] = 0;
                return s;
              }

            case '\n':
              parser.pos().lines();
              break;

            default:
                break;
            }
        }
    }

    boundary parse_boundary(parser& parser, uint8_t default_value)
    {
      // Either a number or an identifier
      int num = parse_number(parser);

      if (num != -1)
        return boundary(num);
      
      char c = parser.advance();

      if (c >= 'a' && c <= 'z')
        return boundary(parse_identifier(parser));

      if (c == '"')
        return boundary(parse_quoted_identifier(parser));
      
      parser.advance(-1);

      return boundary(default_value);
    }


    // PARSING:
    //    parse_rule      parses any expression (nud led*)
    //    parse_led       parses the right part of a binary expression
    //    parse_nud       parses a primary rule (prefix, literal or parens)
    //
    // Top down operator precedence parsing is used.

    template<typename T, typename F>
    std::unique_ptr<T> parse(F& factory, parser& parser, int rbp);

    template<typename T, typename F>
    std::unique_ptr<T> parse_nud(F& factory, parser& parser, int& lbp)
    {
      for (;;)
        {
          char c = parser.advance();

          lbp = 100;

          switch (c)
            {
            
              // UNARY NODES
            case '!':
              return factory.unary(op::Not, parse<T, F>(factory, parser, 100));
            case 'F':
              return factory.unary(op::F, parse<T, F>(factory, parser, 100));
            case 'G':
              return factory.unary(op::G, parse<T, F>(factory, parser, 100));
            case 'X':
              return factory.unary(op::X, parse<T, F>(factory, parser, 100));


              // CONSTANTS
            case '0':
              return factory.ff();
            case '1':
              return factory.tt();
            case '[':
              // Star formula, let the led parser take care of it
              parser.advance(-1);

              return factory.tt();


              // VARIABLES
            case 'f':
            case 'g':
            case 'h':
            case 'e':
            case 'u':
            case 's':
            case 'q':
            case 'b':
            case 'r':
              return factory.identifier(parse_identifier(parser));
            case '"':
              return factory.identifier(parse_quoted_identifier(parser));


              // CLOSURES
            case '{':
              {
                parser.cbal++;

                // Binding power must be lower than lowest binary binding power
                // (which is 10), but higher than equivalence binding power
                // (which is 0). Thus we choose 8.
                return factory.closure(op::Closure,
                                       parse<T, F>(factory, parser, 8));
              }


              // PARENTHESIZED
            case '(':
              {
                std::unique_ptr<char[]> scope_name;

                if (parser.match('<'))
                  {
                    scope_name = parse_identifier(parser);

                    if (!parser.match('>'))
                      parser.report(location(parser.pos()),
                                    "Invalid scope specifier.");
                  }

                parser.pbal++;

                // Binding power must be lower than lowest binary binding power
                // (which is 10), but higher than equivalence binding power
                // (which is 0). Thus we choose 8.
                auto result = parse<T, F>(factory, parser, 8);

                if (scope_name)
                  return factory.scoped(std::move(result),
                                        std::move(scope_name));
                else
                  return result;
              }


              // N-ARY INDICATOR
            case '.':
              {
                if (parser.current() != '.')
                  parser.report(location(parser.pos() - 1),
                                "Invalid n-ary indicator.");

                parser.advance();

                // Return an invalid node, which indicates a n-ary node
                return nullptr;
              }


              // UNKNOWN OR ERROR
            case 0:
            CASE_END:
              {
                parser.advance(-1);
                lbp = 0;

                parser.report_unexpected_eof();
                return nullptr;
              }

            default:
              parser.report(location(parser.pos() - 1),
                            "Unknown expression starting with '", c, "'.");
              parser.skip_whitespace();
              continue;
            }
        }

    }
    
    template<typename T, typename F>
    bool try_parse_led(F& factory, parser& parser, std::unique_ptr<T>& left,
                       int& lbp, int rbp)
    {
#define RETURN_BINARY(bp, len, op, min)                                 \
      {                                                                 \
        if (rbp < bp)                                                   \
          {                                                             \
            /* Success, continue */                                     \
            lbp = bp;                                                   \
            parser.advance(len);                                        \
            auto right = parse<T, F>(factory, parser, bp - min);        \
            if (!right)                                                 \
              /* We're in a n-ary node */                               \
              left = factory.mult(op, std::move(left));                 \
            else                                                        \
              left = factory.binary(op, std::move(left), std::move(right)); \
            return true;                                                \
          }                                                             \
        else                                                            \
          {                                                             \
            /* Failure, reset */                                        \
            parser.advance(-1);                                         \
            return false;                                               \
          }                                                             \
    }
#define RETURN_LEFT_BINARY(bp, len, op)  RETURN_BINARY(bp, len, op, 0)
#define RETURN_RIGHT_BINARY(bp, len, op) RETURN_BINARY(bp, len, op, 1)

      bool fail_silently = false;

      for (;;)
      switch (parser.advance())
        {
          // In increasing binding power...
        CASE_END:
          return false;

        case ')':
          {
            lbp = 0;
            parser.pbal--;

            return false;
          }
        case '}':
          {
            lbp = 0;
            parser.cbal--;

            return false;
          }

        case '[':
          {
            if (parser.match("]->", false))
              RETURN_RIGHT_BINARY(40, 4, op::UConcat);
            if (parser.match("]=>", false))
              RETURN_RIGHT_BINARY(40, 4, op::UConcat);
            
            auto kind = parser.match(':') ? op::FStar : op::Star;

            // Bound formula
            lbp = 0;
          
            if (parser.match("+]"))
              {
                left = factory.star(kind, std::move(left),
                                    boundary((uint8_t)1),
                                    boundary(formula::unbounded()));
                return true;
              }
            if (parser.match("*]"))
              {
                left = factory.star(kind, std::move(left),
                                    boundary((uint8_t)0),
                                    boundary(formula::unbounded()));
                return true;
              }

            if (!parser.match('*'))
              {
                parser.report(location(parser.pos()),
                              "Expected '*'.");
                return false;
              }

            auto min = parse_boundary(parser, 0);

            if (!parser.skip_whitespace())
              {
                parser.report_unexpected_eof();
                return false;
              }
            
            if (!parser.match(".."))
              {
                parser.report(location(parser.pos()),
                              "Expected '..'.");
                return false;
              }
            
            auto max = parse_boundary(parser, formula::unbounded());

            if (!parser.skip_whitespace())
              {
                parser.report_unexpected_eof();
                return false;
              }
            
            if (!parser.match(']'))
              {
                parser.report(location(parser.pos()),
                              "Expected ']'.");
                return false;
              }
            
            left = factory.star(kind, std::move(left),  
                                std::move(min), std::move(max));
            return true;
          }

        case '<':
          {
            if (parser.match("->", false))
              RETURN_RIGHT_BINARY(10, 3, op::Equiv);
            if (parser.match("=>", false))
              RETURN_RIGHT_BINARY(10, 3, op::Equiv);
            if (parser.match(">->", false))
              RETURN_RIGHT_BINARY(40, 4, op::EConcat);
            if (parser.match(">=>", false))
              RETURN_RIGHT_BINARY(40, 4, op::EConcat);

            goto invalid;
          }

        case ';':
          RETURN_LEFT_BINARY(20, 1, op::Concat);

        case ':':
          RETURN_LEFT_BINARY(30, 1, op::Fusion);

        case '-':
          {
            char current = parser.current();

            if (current == '>' ||
               (parser.current(1) == '>' && (current == '-' || current == '=')))
              RETURN_RIGHT_BINARY(40, 2 + (current != '>'), op::Implies);

            goto invalid;
          }

        case '^':
          RETURN_LEFT_BINARY(50, 1, op::Xor);

        case '|':
          RETURN_LEFT_BINARY(60, 1, op::Or);

        case '&':
          RETURN_LEFT_BINARY(70, 1 + (parser.current() == '&'), op::And);

        case 'o':
          {
            if (!std::is_same<T, rewriting_pattern>::value)
              goto invalid;
            if (parser.current() != 'r')
              goto invalid;

            RETURN_LEFT_BINARY(90, 2, op::OrRat);
          }

        case 'U':
          RETURN_RIGHT_BINARY(80, 1, op::U);
        case 'W':
          RETURN_RIGHT_BINARY(80, 1, op::W);
        case 'M':
          RETURN_RIGHT_BINARY(80, 1, op::M);
        case 'R':
          RETURN_RIGHT_BINARY(80, 1, op::R);


        case 0:
          parser.advance(-1);
          return false;

        invalid:
        default:
          {
            if (fail_silently)
              continue;

            parser.report(location(parser.pos()),
                          "Unknown operator '", parser.current(-1), "'.");
            fail_silently = true;

            break;
          }
        }
    }

    template<typename T, typename F>
    std::unique_ptr<T> parse(F& factory, parser& parser, int rbp)
    {
      // Trim left
      if (!parser.skip_whitespace())
        return nullptr;
      
      int lbp = 0;
      std::unique_ptr<T> left = parse_nud<T, F>(factory, parser, lbp);

      // Here we shouldn't compare 'rbp' to 'lbp', but instead 'rbp' to the
      // the current operator's 'lbp' (instead of the previous one).
      while (parser.skip_whitespace() &&
             try_parse_led<T, F>(factory, parser, left, lbp, rbp))
        ; // Do nothing, everything is taken care of by try_parse_led

      return left;
    }


    struct pattern_factory final
    {
      using unique_pattern = std::unique_ptr<rewriting_pattern>;

      rewriting_pattern::mapper mapper;

      pattern_factory(rewriting_pattern::mapper mapper) : mapper(mapper)
      {}

      inline unique_pattern ff() const
      {
        return std::make_unique<const_pattern>(op::ff);
      }

      inline unique_pattern tt() const
      {
        return std::make_unique<const_pattern>(op::tt);
      }

      inline unique_pattern eword() const
      {
        return std::make_unique<const_pattern>(op::eword);
      }

      inline unique_pattern
      identifier(std::unique_ptr<char[]> id)
      {
        if (mapper != nullptr)
          {
            auto pattern = mapper(id.get());

            if (pattern)
              return pattern;
          }
        
        return std::make_unique<named_pattern>(id.release(), true);
      }

      inline unique_pattern
      unary(op kind, std::unique_ptr<rewriting_pattern> operand) const
      {
        if (kind == op::Not)
          {
            // Not(Closure a) -> NegClosure a
            unary_pattern* un = dynamic_cast<unary_pattern*>(operand.get());

            if (un && un->kind == op::Closure)
              return std::make_unique<unary_pattern>(op::NegClosure,
                                                     std::move(un->patterns_));
          }

        patterns patterns_;
        patterns_.push_back(std::move(operand));

        return std::make_unique<unary_pattern>(kind, std::move(patterns_));
      }

      inline unique_pattern
      closure(op kind, std::unique_ptr<rewriting_pattern> inner) const
      {
        patterns patterns_;
        patterns_.push_back(std::move(inner));

        return std::make_unique<unary_pattern>(kind, std::move(patterns_));
      }

      inline unique_pattern
      star(op kind, std::unique_ptr<rewriting_pattern> operand,
           boundary min, boundary max) const
      {
        patterns patterns_;
        patterns_.push_back(std::move(operand));

        return std::make_unique<star_pattern>(kind, std::move(patterns_),
                                              std::move(min), std::move(max));
      }

      inline unique_pattern
      binary(op kind,
             std::unique_ptr<rewriting_pattern> left,
             std::unique_ptr<rewriting_pattern> right) const
      {
        if (kind == op::OrRat)
          {
            // 'or' syntax, where we directly merge the patterns
            if (left->try_merge(*right))
              return left;
            
            patterns patterns_;
            patterns_.push_back(std::move(left));
            patterns_.push_back(std::move(right));
            
            return std::make_unique<or_pattern>(std::move(patterns_));
          }
        
        patterns left_patterns;
        patterns right_patterns;
        
        left_patterns.push_back(std::move(left));
        right_patterns.push_back(std::move(right));
        
        return std::make_unique<binary_pattern>(kind,
                                                std::move(left_patterns),
                                                std::move(right_patterns));
      }

      inline unique_pattern
      mult(op kind, std::unique_ptr<rewriting_pattern> trans) const
      {
        patterns patterns_;
        patterns_.push_back(std::move(trans));

        return std::make_unique<nary_pattern>(kind, std::move(patterns_));
      }

      inline unique_pattern
      scoped(std::unique_ptr<rewriting_pattern> pattern,
             std::unique_ptr<char[]> name) const
      {
        return std::make_unique<scope_pattern>(std::move(pattern),
                                               fnv_hash(name.get()));
      }
    };


    struct output_factory final
    {
      using unique_output = std::unique_ptr<rewriting_output>;

      rewriting_output::mapper mapper;

      output_factory(rewriting_output::mapper mapper) : mapper(mapper)
      {}

      inline unique_output ff() const
      {
        return std::make_unique<const_output>(op::ff);
      }

      inline unique_output tt() const
      {
        return std::make_unique<const_output>(op::tt);
      }

      inline unique_output eword() const
      {
        return std::make_unique<const_output>(op::eword);
      }

      inline unique_output
      identifier(std::unique_ptr<char[]> id)
      {
        if (mapper != nullptr)
          {
            auto output = mapper(id.get());

            if (output)
              return output;
          }

        return std::make_unique<named_output>(id.release(), true);
      }

      inline unique_output
      unary(op kind, std::unique_ptr<rewriting_output> operand) const
      {
        if (kind == op::Not)
          {
            // Not(Closure a) -> NegClosure a
            unary_output* un = dynamic_cast<unary_output*>(operand.get());

            if (un && un->kind == op::Closure)
              return std::make_unique<unary_output>(op::NegClosure,
                                                    std::move(un->output));
          }
        
        return std::make_unique<unary_output>(kind, std::move(operand));
      }

      inline unique_output
      closure(op kind, std::unique_ptr<rewriting_output> inner) const
      {
        return std::make_unique<unary_output>(kind, std::move(inner));
      }

      inline unique_output
      star(op kind, std::unique_ptr<rewriting_output> operand,
           boundary min, boundary max) const
      {
        return std::make_unique<star_output>(kind, std::move(operand),
                                             std::move(min), std::move(max));
      }

      inline unique_output
      binary(op kind,
             std::unique_ptr<rewriting_output> left,
             std::unique_ptr<rewriting_output> right) const
      {
        return std::make_unique<binary_output>(kind,
                                               std::move(left),
                                               std::move(right));
      }

      inline unique_output
      mult(op kind, std::unique_ptr<rewriting_output> output) const
      {
        return std::make_unique<nary_output>(kind, std::move(output));
      }

      inline unique_output
      scoped(std::unique_ptr<rewriting_output> output,
             std::unique_ptr<char[]> name) const
      {
        return std::make_unique<scope_output>(std::move(output),
                                              fnv_hash(name.get()));
      }
    };
  }



  std::ostream& rewriting_match::dump(std::ostream& os, unsigned depth,
                                      size_t max_depth) const
  {
    if (SPOT_UNLIKELY(!max_depth))
      max_depth = SIZE_MAX;
  
    const inner* ptr = ptr_.get();

    if (!ptr || ptr->kind_ == inner::matchk::Empty)
      return os << "empty" << std::endl;

    while (ptr != nullptr && ptr->kind_ != inner::matchk::Empty && max_depth--)
      {
        if (depth)
          {
            for (unsigned i = 1; i < depth; i++)
              os << ' ';

            os << "\\ ";
          }
        else
          os << ' ';

        switch (ptr->kind_)
          {
          case inner::matchk::Value:
            os << "value <" << ptr->name_hash_ << "> " << ptr->value_;
            break;
          case inner::matchk::Formula:
            ptr->formula_.dump(os << "formula <" << ptr->name_hash_ << "> ");
            break;
          case inner::matchk::Scope:
            {
              os << "scope <" << ptr->name_hash_ << "> [" << std::endl;

              rewriting_match(ptr->inner_).dump(os, depth + 2, max_depth);
              
              for (unsigned i = 0; i < depth; i++)
                os << ' ';

              os << ']';
              break;
            }
          case inner::matchk::Output:
            ptr->output_->dump(os << "output < ", false) << " >";
            break;

          default:
            SPOT_UNREACHABLE();
          }
        
        os << std::endl;

        depth += 3;
        ptr = ptr->prev_.get();
      }
    
    return os;
  }


  std::unique_ptr<rewriting_output>
  rewriting_output::map(std::unique_ptr<rewriting_output> output,
                        std::function<formula(formula&)> map)
  {
    return std::make_unique<mapping_output>(std::move(output), map);
  }

  std::unique_ptr<spot::rewriting_pattern>
  rewriting_pattern::condition(std::unique_ptr<rewriting_pattern> pat,
                               std::function<bool(const rewriting_match&)> p)
  {
    return std::make_unique<predicate_pattern>(p, std::move(pat));
  }

  void rewriting_recipe::extend(rewriting_recipe&& other)
  {
    append(patterns_, std::move(other.patterns_));
    merge_patterns(patterns_);
  }

  void rewriting_recipe::extend(const char* rules,
                                rewriting_pattern::mapper pattern_mapper,
                                rewriting_output::mapper output_mapper,
                                rewriting_pattern::predicate predicate)
    throw(parse_error)
  {
    extend(
      parse_rewriting_recipe(rules, pattern_mapper, output_mapper, predicate)
    );
  }

  std::unique_ptr<rewriting_pattern>
  parse_rewriting_pattern(const char* pattern,
                          rewriting_pattern::mapper mapper,
                          rewriting_pattern::predicate predicate)
    throw(parse_error)
  {
    parser parser(pattern);
    pattern_factory factory(mapper);

    if (!parser.skip_whitespace())
      throw std::invalid_argument("Given string cannot be empty.");

    auto start_position = parser.pos();
    auto pat = parse<rewriting_pattern,pattern_factory>(factory, parser, 0);

    if (!pat)
      // Error encountered and already reported, return.
      throw parser.build_error();

    location loc(start_position, parser.pos());

    parser.check_balance(loc);

    if (parser.error_list().size() != 0)
      throw parser.build_error();
    
    if (predicate != nullptr)
      return rewriting_pattern::condition(std::move(pat), predicate);

    return pat;
  }

  std::unique_ptr<rewriting_output>
  parse_rewriting_output(const char* replacement,
                         rewriting_output::mapper mapper)
    throw(parse_error)
  {
    parser parser(replacement);
    output_factory factory(mapper);

    if (!parser.skip_whitespace())
      throw std::invalid_argument("Given string cannot be empty.");

    auto start_position = parser.pos();
    auto output = parse<rewriting_output, output_factory>(factory, parser, 0);

    if (!output)
      // Error encountered and already reported, return.
      throw parser.build_error();

    location loc(start_position, parser.pos());

    parser.check_balance(loc);

    if (parser.error_list().size() != 0)
      throw parser.build_error();
  
    return output;
  }

  rewriting_recipe
  parse_rewriting_recipe(const char* rules,
                         rewriting_pattern::mapper pattern_mapper,
                         rewriting_output::mapper output_mapper,
                         rewriting_pattern::predicate predicate)
    throw(parse_error)
  {
    parser parser(rules);
    rewriting_recipe::patterns_type patterns;

    for (;;)
      {
        parser.skip_whitespace();

        // Parse left hand side (input pattern)
        pattern_factory pfactory(pattern_mapper);

        auto pattern_start = parser.pos();

        auto pattern = parse<rewriting_pattern,
                             pattern_factory>(pfactory, parser, 0);

        if (!pattern)
          break;
        
        parser.advance();

        // Check balance
        location pattern_location(pattern_start, parser.pos() - 1);

        parser.check_balance(pattern_location);
        parser.reset_single_recipe_state();

        if (!parser.skip_whitespace())
          {
            parser.report(location(parser.pos()),
                          "Expected output string.");
            break;
          }
        
        // Parse right hand side (output)
        output_factory ofactory(output_mapper);

        auto output_start = parser.pos();
        auto output = parse<rewriting_output,
                            output_factory>(ofactory, parser, 0);

        if (!output)
          break;
        
        std::shared_ptr<rewriting_output> shared_output(std::move(output));

        // Make sure the template doesn't use unknown identifiers.
        position output_end = parser.pos();

        if (parser.current() != 0)
          output_end -= 1;

        // Finalize pattern
        pattern->associate(shared_output);

        if (predicate != nullptr)
          // Add predicate if needed
          pattern = rewriting_pattern::condition(std::move(pattern), predicate);

        patterns.push_back(std::move(pattern));

        // Basic error checking
        location output_location(output_start, output_end);

        parser.check_balance(output_location);

        if (parser.current() == 0)
          break;
        
        parser.advance();
        parser.reset_single_recipe_state();
      }

    if (patterns.size() == 0)
      parser.report(location(), "No expression could be fully parsed.");

    if (parser.source(parser.offset() - 1) == 0)
      // We got too far, fix this now so error reporting is more accurate
      parser.advance(-1);

    // Report errors and dig deeper if we haven't found any yet
    if (parser.error_list().size() != 0)
      throw parser.build_error();
    
    merge_patterns(patterns);

    return rewriting_recipe(std::move(patterns));
  }
}
