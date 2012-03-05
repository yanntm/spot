// Copyright (C) 2011 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
// Copyright (C) 2003, 2004, 2006 Laboratoire d'Informatique de
// Paris 6 (LIP6), département Systèmes Répartis Coopératifs (SRC),
// Université Pierre et Marie Curie.
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Spot; see the file COPYING.  If not, write to the Free
// Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.

#ifndef SPOT_TGBA_TGBAEXPLICIT2_HH
# define SPOT_TGBA_TGBAEXPLICIT2_HH

#include <list>

#include "misc/hash.hh"
#include "ltlast/formula.hh"
#include "tgba.hh"

//we will split everything after for the moment keep a single file
namespace spot
{
  class tba: public tgba
  {
  public:
    /// A State in a TBA (Transition based Buchi Automaton) is accepting
    /// iff all outgoing transitions are accepting.
    virtual bool is_accepting (const spot::state* s) = 0;
  };

  /// States used by spot::explicit_graph implementation
  /// \ingroup tgba_representation
  template<typename Label, typename label_hash>
  class state_explicit: public state
  {
  public:
    state_explicit(const Label& l):
      label_(l)
    {
    }

    typedef Label Label_t;

    struct transition
    {
      bdd condition;
      bdd acceptance_conditions;
      const state_explicit<Label, label_hash>* dest_;
      const state_explicit<Label, label_hash>* src_;
    };

    const std::list<transition>& succ () const
    {
      return succ_;
    }

    //keep it here or move it down to the sub classes?
    virtual int compare(const state* other) const
    {
      const state_explicit<Label, label_hash>* s =
	dynamic_cast<const state_explicit<Label, label_hash>*>(other);

      if (!s)
	return 0;

      return this == s;
    }

  protected:
    Label label_;
    std::list<transition> succ_;
  };

  /// States labeled by a string
  /// \ingroup tgba_representation
  class state_explicit_string:
    public state_explicit<std::string, string_hash>
  {
  public:
    state_explicit_string(const std::string& s):
      state_explicit<std::string, string_hash>(s)
    {
    }

    virtual size_t hash() const
    {
      string_hash s;
      return s(label_);
    }

    virtual state_explicit<std::string, string_hash>* clone() const
    {
      return new state_explicit_string(label_);
    }
  };
  /// Successor iterators used by spot::tgba_explicit.
  /// \ingroup tgba_representation
  template<typename label, typename label_hash>
  class tgba_explicit_succ_iterator: public tgba_succ_iterator
  {
    //should be better to keep the starting state, see if it mess up something
    tgba_explicit_succ_iterator(const state_explicit<label, label_hash>* start,
				bdd all_acc)
      : start_(start),
	all_acceptance_conditions_(all_acc)
    {
    }

    virtual void first()
    {
      it_ = start_->succ().begin();
    }

    virtual void next()
    {
      ++it_;
    }

    virtual bool done() const
    {
      return it_ == start_->succ().end();
    }

    virtual state_explicit<label, label_hash>* current_state() const
    {
      assert(!done());
      return const_cast<state_explicit<label, label_hash>* > (it_->dest);
    }

    virtual bdd current_condition() const
    {
      assert(!done());
      return it_->condition;
    }

    virtual bdd current_acceptance_conditions() const
    {
      assert(!done());
      return it_->acceptance_conditions & all_acceptance_conditions_;
    }

  private:
    //friend useless here?

    const state_explicit<label, label_hash>* start_;
    typename state_explicit<label, label_hash>::transition::const_iterator it_;
    bdd all_acceptance_conditions_;
  };

  /// Graph implementation for automata representation
  /// \ingroup tgba_representation
  template<typename State, typename Type>
  class explicit_graph: public Type
  {
  public:
    explicit_graph():
      init_(0)
    {
    }

    virtual ~explicit_graph ()
    {
    }

    //A part of the TGBA interface
    virtual spot::state* get_init_state () const
    {
      return init_;
    }

    //here?
    virtual tgba_succ_iterator*
    succ_iter(const spot::state* local_state,
	      const spot::state* global_state = 0,
	      const tgba* global_automaton = 0) const
    {
      (void) local_state;
      (void) global_state;
      (void) global_automaton;

      return 0;
    }

    //no need?
    virtual std::string format_state(const state* state) const
    {
      (void) state;
      return "coucou";
    }

  protected:
    typedef Sgi::hash_map<typename State::Label_t, State> ls_map;
    State* init_;
  };

  template <typename State>
  class tgba_explicit: public explicit_graph<State, tgba>
  {
  public:
    tgba_explicit(bdd_dict* dict):
      dict_(dict)
    {
    }

    virtual ~tgba_explicit ()
    {
    }

    //The remaining parts of the TGBA iterface
    virtual bdd_dict* get_dict() const
    {
      return dict_;
    }

    virtual bdd all_acceptance_conditions() const
    {
      return bddtrue;
    }

    virtual bdd neg_acceptance_conditions() const
    {
      return bddtrue;
    }

  protected:
    virtual bdd compute_support_conditions(const spot::state* state) const
    {
      (void) state;
      return bddtrue;
    }

    virtual bdd compute_support_variables(const spot::state* state) const
    {
      (void) state;
      return bddtrue;
    }

    bdd_dict* dict_;
  };
}

#endif // SPOT_TGBA_TGBAEXPLICIT2_HH
