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

#ifndef SPOT_FASTTGBAALGOS_EC_COU99_UF_HH
# define SPOT_FASTTGBAALGOS_EC_COU99_UF_HH

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
    // out, trivial, acc
    std::vector<std::tuple<unsigned int, bool, markset*>> stack_;
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
      //std::cout << "Push trivial "  << root << std::endl;
      // The stack is empty just push it!
      if (is_empty())
	{
	  assert(root == 0);
	  stack_.push_back(std::make_tuple(root, true, empty_));
	}
      // The stack is empty and the back is a trivial SCC
      // Update it!
      else if (std::get<1>(stack_.back()))
	{
	  assert(root == std::get<0>(stack_.back()) + 1);
	  stack_.pop_back();
	  stack_.push_back(std::make_tuple(root, true, empty_));
	}
      // Otherwise it's a non trivial SCC fix out for this SCC
      // and push the new one.
      else {
 	// std::cout << root << std::endl;
	// std::cout <<  std::get<0>(stack_.back()) << std::endl;
	assert(root_of_the_top() < root &&
	       root <= std::get<0>(stack_.back()) + 1);
	markset m  = top_acceptance();
	stack_.pop_back();
	stack_.push_back(std::make_tuple(root - 1, false, new markset(m)));
	stack_.push_back(std::make_tuple(root, true, empty_));
      }
    }

    void push_non_trivial (unsigned int root,
			   markset m,
			   unsigned int last = 0)
    {
      //std::cout << "Push non trivial "  << root << std::endl;
      assert(root <= last );
      assert (! is_empty() || root == 0);
      assert(is_empty() || root == std::get<0>(stack_.back()) + 1);

      if (m == *empty_)
	stack_.push_back(std::make_tuple(last, false, empty_));
      else
	stack_.push_back(std::make_tuple(last, false, new markset(m)));
    }

    /// This method returns the root stored at the top of the
    /// root stack.
    unsigned int root_of_the_top ()
    {
      assert (!is_empty());
      if (std::get<1>(stack_.back()))
	return std::get<0>(stack_.back());
      if (stack_.size() > 1)
	return std::get<0>(stack_[stack_.size() - 2]) + 1;
      return 0 ;
    }

    void pop()
    {
      //std::cout << "Pop "   << std::endl;
      unsigned int r = root_of_the_top();

      if (std::get<1>(stack_.back()))
	{
	  stack_.pop_back();
	  if (is_empty())
	    {
	      if (r > 0)
		stack_.push_back(std::make_tuple(r-1, true, empty_));
	    }
	  else
	    {
	      if (r - 1 > std::get<0>(stack_.back()))
		stack_.push_back(std::make_tuple(r-1, true, empty_));
	    }
	}
      else
	{
	  if (std::get<2>(stack_.back()) != empty_)
	    delete std::get<2>(stack_.back());
	  stack_.pop_back();
	}
    }

    const markset& top_acceptance()
    {
      assert(stack_.size());
      return *std::get<2>(stack_.back());
    }
  };



  /// This class provides the adaptation of the emptiness
  /// check of couvreur using an Union Find structure and
  /// a specific dedicated root stack
  class cou99_uf : public ec
  {
  public:

    /// A constructor taking the automaton to check
    cou99_uf(instanciator* i);

    /// A destructor
    virtual ~cou99_uf();

    /// The implementation of the interface
    bool check();

  protected:

    /// \brief Fix set ups for the algo
    inline void init();

    // ------------------------------------------------------------
    // For classic algorithm with stack and UF
    // ------------------------------------------------------------

    /// \brief Push a new state to explore
    virtual void dfs_push(fasttgba_state*);

    /// \brief  Pop states already explored
    virtual void dfs_pop();

    /// \brief merge multiple states
    virtual bool merge(fasttgba_state*);

    /// \brief the main procedure
    virtual void main();

    ///< \brief Storage for counterexample found or not
    bool counterexample_found;

    ///< \brief the automaton that will be used for the Emptiness check
    const fasttgba* a_;

    /// \brief define the type that will be use for the todo stack
    typedef std::pair<const spot::fasttgba_state*,
		      fasttgba_succ_iterator*> pair_state_iter;

    /// \brief the todo stack
    std::vector<pair_state_iter> todo;

    /// For the last algorithm (stack)
    std::vector<std::tuple<markset,
			   const spot::fasttgba_state*,
			   fasttgba_succ_iterator*> > todo_stack;
    /// Toot of stack
    compressed_stack_of_roots *roots_stack_;

    /// \brief the union_find used for the storage
    union_find *uf;

    /// \brief to detect if an iterator has already be once incremented
    bool last;

    /// \brief for using scc scc roots
    std::stack<int> scc;

    /// \brief The instance automaton
    const instance_automaton* inst;
  };

  /// This class proposes an algorihtm using the
  /// Union FINd without using a stack of roots
  /// which is better for parallel emptiness checks.
  class cou99_uf_original : public cou99_uf
  {
  public:

    /// \brief Push a new state to explore
    virtual void dfs_push(fasttgba_state*);

    /// \brief  Pop states already explored
    virtual void dfs_pop();

    /// \brief merge multiple states
    virtual bool merge(fasttgba_state*);

  };
}

#endif // SPOT_FASTTGBAALGOS_EC_COU99_UF_HH
