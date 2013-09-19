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

#ifndef SPOT_FASTTGBAALGOS_EC_LOWLINK_STACK_HH
# define SPOT_FASTTGBAALGOS_EC_LOWLINK_STACK_HH

#include <stack>
#include <tuple>
#include <string>
#include "misc/hash.hh"
#include "union_find.hh"
#include "fasttgba/fasttgba.hh"
#include "ec.hh"

namespace spot
{
  /// This class is used to store all lowlink used by Tarjan algorithm
  /// It's a wrapper for many implementations and compressions
  /// techniques
  class stack_of_lowlinks
  {
    stack_of_lowlink(acc_dict& acc):
      empty_(new markset(acc)),
      max_size_(0)
    { }


    /// \brief Clean the stack before desroying it 
    virtual ~stack_of_lowlink()
    {
      while (!stack_.empty())
	{
	  pop();
	}
    }

    /// \brief Insert a new lowlink into the stack
    virtual void push(int lowlink)
    {
      stack_.push(std::make_pair(lowlink, empty_));
    }

    /// \brief Return the lowlink at the top of the stack
    virtual int top()
    {
      return stack_.top().first;
    }

    /// \brief Modify the acceptance set at for the elemet
    /// at the top of the stack
    virtual const markset& top_acceptance()
    {
      return *stack_.top().second;
    }

    /// \brief pop the element at the top of the stack
    virtual int pop()
    {
      max_size_ = max_size_ > stack_.size()? max_size_ : stack_.size();
      int t = top();
      if (stack_.top().second != empty_)
	delete stack_.top().second;
      stack_.pop();
      return t;
    }

    /// Modify lowlink and marset for the element of the top
    /// of the stack
    virtual void set_top(int ll, markset m)
    {
      stack_.top().first = ll;
      stack_.top().second =  new markset(m);
    }

    /// \brief Return the peak of this stack
    virtual unsigned int max_size()
    {
      return max_size_;
    }

  private:
    std::stack<std::pair<unsigned int, markset*>> stack_; // The stack
    markset* empty_;		///< The empty markset
    unsigned int max_size_;
  };
}


#endif // SPOT_FASTTGBAALGOS_EC_LOWLINK_STACK_HH
