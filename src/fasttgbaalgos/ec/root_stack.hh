// Copyright (C) 2013 Laboratoire de Recherche et Développement
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

#ifndef SPOT_FASTTGBAALGOS_EC_ROOT_STACK_HH
# define SPOT_FASTTGBAALGOS_EC_ROOT_STACK_HH

#include <stack>
#include <tuple>
#include "misc/hash.hh"
#include "union_find.hh"
#include "fasttgba/fasttgba.hh"
#include "ec.hh"

namespace spot
{
  /// This class is used to store all roots used by this algorithm
  /// It's a wrapper for many implementations and compressions
  // techniques
  class stack_of_roots
  {
  public:
    stack_of_roots(acc_dict& acc):
      empty_(new markset(acc))
    { }

    virtual ~stack_of_roots()
    {
      while (!stack_.empty())
	{
	  pop();
	}
    }

    bool is_empty ()
    {
      return stack_.empty();
    }

    void push_trivial (unsigned int root)
    {
      stack_.push(std::make_pair(root, empty_));
    }

    void push_non_trivial (unsigned int root,
			   markset m,
			   unsigned int last = 0)
    {
      assert(!last);		// Unused for this implem.
      if (m == *empty_)
	stack_.push(std::make_pair(root, empty_));
      else
	stack_.push(std::make_pair(root, new markset(m)));
    }

    unsigned int root_of_the_top ()
    {
      return stack_.top().first;
    }

    void pop()
    {
      if (stack_.top().second != empty_)
	delete stack_.top().second;
      stack_.pop();
    }

    const markset& top_acceptance()
    {
      assert(stack_.size());
      return *stack_.top().second;
    }

  private:
    std::stack<std::pair<unsigned int, markset*>> stack_; // The stack
    markset* empty_;		///< The empty markset
  };

  class compressed_stack_of_roots
  {
    struct stack_entry
    {
      unsigned int root;
      bool is_trivial;
      markset *mark;
    };

    // out, trivial, acc
    std::vector<// std::tuple<unsigned int, bool, markset*>
      stack_entry> stack_;
    markset* empty_;

  public:
    compressed_stack_of_roots(acc_dict& acc):
      empty_(new markset(acc))
    { }

    virtual ~compressed_stack_of_roots()
    {
      while (!stack_.empty())
	{
	  pop();
	}
    }

    bool is_empty ()
    {
      return stack_.empty();
    }

    void push_trivial (unsigned int root)
    {
      // std::cout << "Push trivial "  << root << std::endl;
      // The stack is empty just push it!
      if (is_empty())
	{
	  assert(root == 0);
	  stack_.push_back({root, true, empty_});
	}
      // The stack is empty and the back is a trivial SCC
      // Update it!
      else if (stack_.back().is_trivial)
	{
	  assert(root == stack_.back().root +1);
	  stack_.pop_back();
	  stack_.push_back({root, true, empty_});
	}
      // Otherwise it's a non trivial SCC fix out for this SCC
      // and push the new one.
      else {
	assert(root_of_the_top() < root &&
	       root <= stack_.back().root +1);
	stack_.back().root = root -1;
	stack_.push_back({root, true, empty_});
      }
    }

    void push_non_trivial (unsigned int root,
			   markset m,
			   unsigned int last = 0)
    {
      // std::cout << "Push non trivial "  << root << std::endl;
      assert(root <= last );
      assert (! is_empty() || root == 0);
      assert(is_empty() || root == stack_.back().root +1);

      if (m == *empty_)
	stack_.push_back({last, false, empty_});
      else
	stack_.push_back({last, false, new markset(m)});
    }

    /// This method returns the root stored at the top of the
    /// root stack.
    unsigned int root_of_the_top ()
    {
      assert (!is_empty());
      if (stack_.back().is_trivial)
	return stack_.back().root;
      if (stack_.size() > 1)
	return  (stack_[stack_.size() - 2]).root + 1;
      return 0 ;
    }

    void pop()
    {
      //std::cout << "Pop "   << std::endl;
      unsigned int r = root_of_the_top();

      if (stack_.back().is_trivial)
	{
	  stack_.pop_back();
	  if (is_empty())
	    {
	      if (r > 0)
		stack_.push_back({r-1, true, empty_});
	    }
	  else
	    {
	      if (r - 1 > stack_.back().root)
		stack_.push_back({r-1, true, empty_});
	    }
	}
      else
	{
	  if (stack_.back().mark != empty_)
	    delete stack_.back().mark;
	  stack_.pop_back();
	}
    }

    const markset& top_acceptance()
    {
      assert(stack_.size());
      return *stack_.back().mark;
    }
  };
}


#endif // SPOT_FASTTGBAALGOS_EC_ROOT_STACK_HH
