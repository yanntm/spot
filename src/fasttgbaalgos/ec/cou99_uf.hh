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

  // This class is used to store one root used in the UF
  class roots
  {
  public:
    int root_;
    bool is_trivial_;
    markset* accset_;
    const markset& empty_;

    roots(int r, bool it, const markset& empty)
      : root_(r), is_trivial_(it),
	accset_(0), empty_(empty)
    {
    }

    virtual ~roots()
    {
      delete accset_;
    }

    void add_acceptance (markset& acc)
    {
      if (acc == empty_)
      	return ;

      if (!accset_)
      	accset_ = new markset (empty_);
      accset_->operator |= (acc);
    }

    const markset& get_acceptance ()
    {
      if (!accset_)
	return empty_;
      return *accset_;
    }
  };

  // This class is used to store all roots used by this algorithm
  class stack_of_roots
  {
    std::vector<roots*> stack_;
    int stack_size_;
    markset* empty_;


  public:
    stack_of_roots(acc_dict& acc):
      stack_size_(0), empty_(new markset(acc))
    { }

    virtual ~stack_of_roots()
    {
      /// TODO DELETE
    }

    void push_trivial (int i)
    {
      ++stack_size_;
      stack_.push_back(new roots(i, true, *empty_));
    }

    int top ()
    {
      return stack_.back()->root_;
      std::cout << "top "
		<< stack_.back()->root_
		<< "  " << stack_size_  << "\n";
    }

    void pop()
    {
      --stack_size_;
      stack_.pop_back();
    }

    bool is_top_fully_accepting()
    {
      assert(stack_.size());
      return stack_.back()->get_acceptance().all();
    }

    const markset& top_acceptance()
    {
      assert(stack_.size());
      return stack_.back()->get_acceptance();
    }

    const markset& add_acceptance_to_top(markset m)
    {
      stack_.back()->add_acceptance (m);
      return stack_.back()->get_acceptance();
    }
  };

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
    // For classic algorithm
    // ------------------------------------------------------------

    /// \brief Push a new state to explore
    virtual void dfs_push_classic(fasttgba_state*);

    /// \brief  Pop states already explored
    virtual void dfs_pop_classic();

    /// \brief merge multiple states
    virtual void merge_classic(fasttgba_state*);

    // ------------------------------------------------------------
    // For integrating SCC stack as original algorithm
    // ------------------------------------------------------------

    /// \brief Push a new state to explore
    void dfs_push_scc(fasttgba_state*);

    /// \brief  Pop states already explored
    void dfs_pop_scc();

    void merge_scc(fasttgba_state*);

    // ------------------------------------------------------------
    // Close to the original algorithm with SCC stack
    // iterator is systematically increased
    // ------------------------------------------------------------

    void dfs_pop_stack(const fasttgba_state*);

    bool merge_stack(markset, fasttgba_state*);

    void main_stack();

    // ------------------------------------------------------------
    // For all algorithms
    // ------------------------------------------------------------

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
    stack_of_roots *r;

    /// \brief the union_find used for the storage
    union_find *uf;
    union_find *uf_stack;

    /// \brief to detect if an iterator has already be once incremented
    bool last;

    /// \brief for using scc scc roots
    std::stack<int> scc;

    /// \brief The instance automaton
    const instance_automaton* inst;
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_COU99_UF_HH
