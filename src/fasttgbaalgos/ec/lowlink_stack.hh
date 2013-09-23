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
  class stack_of_lowlink
  {
  public:
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
      delete empty_;
    }

    /// \brief Insert a new lowlink into the stack
    virtual void push(int lowlink)
    {
      //std::cout << "[push] " << lowlink << std::endl;
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
      assert(!stack_.empty());
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
      //std::cout << "[pop]  " << t << std::endl;
      return t;
    }

    /// Modify lowlink and marset for the element of the top
    /// of the stack
    virtual void set_top(int ll, markset m)
    {
      //std::cout << "[stop] " << ll << std::endl;
      stack_.top().first = ll;
      if (stack_.top().second != empty_)
	delete stack_.top().second;
      stack_.top().second =  new markset(m);
    }

    /// \brief Usefull for SCC-computation
    virtual void set_top(int ll)
    {
      stack_.top().first = ll;
    }

    /// \brief Return the peak of this stack
    virtual unsigned int max_size()
    {
      return max_size_;
    }

    /// \brief Return the current size of the stack
    virtual unsigned int size()
    {
      return stack_.size();
    }

  private:
    std::stack<std::pair<unsigned int, markset*>> stack_; // The stack

  protected:
    markset* empty_;		///< The empty markset
    unsigned int max_size_;
  };


  /// \brief this class represents a compaction of the lowlink stack
  class compressed_stack_of_lowlink: public stack_of_lowlink
  {
  public:
    // This struct defines a base element that will be store into the stack
    struct ll_elt
    {
      int lowlink;		///< The lowlink
      bool backedge_updated;	///< Wether it has been updated
      union {
	int range;		///< Range when backedge_updated is false
	markset* mark;		///< Markset otherwise
      };
    };

    compressed_stack_of_lowlink(acc_dict& acc):
      stack_of_lowlink(acc)
    { }


    /// \brief Clean the stack before desroying it
    virtual ~compressed_stack_of_lowlink()
    {
      while (!stack_.empty())
	{
	  pop();
	}
      delete empty_;
    }

    /// \brief Insert a new lowlink into the stack
    virtual void push(int lowlink)
    {
      //std::cout << "Push : " << lowlink << std::endl;

      if (stack_.empty())
	stack_.push({lowlink, false, 0});
      else if (stack_.top().backedge_updated)
	stack_.push({lowlink, false, 0});
      else
	{
	  assert(stack_.top().backedge_updated == false);
	  assert(stack_.top().range + 1 + stack_.top().lowlink == lowlink);
	  stack_.top().range = stack_.top().range + 1;
	}
    }

    /// \brief Return the lowlink at the top of the stack
    virtual int top()
    {
      if (stack_.top().backedge_updated)
	return stack_.top().lowlink;
      else
	return stack_.top().range + stack_.top().lowlink;
    }

    /// \brief Modify the acceptance set at for the elemet
    /// at the top of the stack
    virtual const markset& top_acceptance()
    {
      if (!stack_.top().backedge_updated)
	return *empty_;
      return *stack_.top().mark;
   }

    /// \brief pop the element at the top of the stack
    virtual int pop()
    {
      int t = top();
      //std::cout << "Pop : " << t << std::endl;
      if (stack_.top().backedge_updated)
	{
	  if (stack_.top().mark != empty_)
	    delete stack_.top().mark;
	  stack_.pop();
	  return t;
	}
      else if (stack_.top().range == 0)
	{
	  if (stack_.top().mark != empty_)
	    delete stack_.top().mark;
	  stack_.pop();
	  return t;
	}
      else
	{
	  stack_.top().range = stack_.top().range - 1;
	}
      return t;
    }

    /// Modify lowlink and marset for the element of the top
    /// of the stack
    virtual void set_top(int ll, markset m)
    {
      //std::cout << "Settop : " << ll << std::endl;
      if (stack_.top().backedge_updated)
	{
	  assert(ll <= stack_.top().lowlink);
	  if (m == *empty_)
	    stack_.top().mark = empty_;
	  else
	    stack_.top().mark = new markset(m);
	  stack_.top().lowlink = ll;
	}
      else
	{

	  if (stack_.top().range == 0)
	    {
	      if (m == *empty_)
		stack_.top().mark = empty_;
	      else
		stack_.top().mark = new markset(m);
	      stack_.top().backedge_updated = true;
	      stack_.top().lowlink = ll;
	    }
	  else
	    {
	      stack_.top().range = stack_.top().range -1;
	      stack_.push({ll, true, 0});
	      if (m == *empty_)
		stack_.top().mark = empty_;
	      else
		stack_.top().mark = new markset(m);
	    }
	}

    }

    /// \brief Usefull for SCC-computation
    virtual void set_top(int ll)
    {
      set_top(ll, *empty_);
    }

    /// \brief Return the peak of this stack
    virtual unsigned int max_size()
    {
      return max_size_;
    }

    /// \brief Return the current size of the stack
    virtual unsigned int size()
    {
      return stack_.size();
    }

  private:
    std::stack<ll_elt> stack_; // The stack
  };
}


#endif // SPOT_FASTTGBAALGOS_EC_LOWLINK_STACK_HH
