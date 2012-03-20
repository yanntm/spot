// Copyright (C) 2012 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
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
#include "misc/bddop.hh"

//we will split everything after for the moment keep a single file
namespace spot
{
  class tba: public tgba
  {
  public:
    /// A State in a TBA (Transition based Buchi Automaton) is accepting
    /// iff all outgoing transitions are accepting.
    virtual bool is_accepting(const spot::state* s) = 0;
  };

  /// States used by spot::explicit_graph implementation
  /// \ingroup tgba_representation
  template<typename Label, typename label_hash>
  class state_explicit: public spot::state
  {
  public:
    state_explicit()
    {
    }

    state_explicit(const Label& l):
      label_(l)
    {
    }

    typedef Label Label_t;
    typedef label_hash label_hash_t;

    struct transition
    {
      bdd condition;
      bdd acceptance_conditions;
      const state_explicit<Label, label_hash>* dest;
    };

    typedef std::list<transition> transitions_t;

    transitions_t& succ() //const
    {
      return succ_;
    }

    const transitions_t& succ() const
    {
      return succ_;
    }

    const Label& label() const
    {
      return label_;
    }

    //keep it here or move it down to the sub classes?
    virtual int compare(const state* other) const
    {
      const state_explicit<Label, label_hash>* s =
	down_cast<const state_explicit<Label, label_hash>*>(other);

      if (!s)
	return 0;

      return this < s;
    }

  protected:
    Label label_;
    transitions_t succ_;
  };

  /// States labeled by a string
  /// \ingroup tgba_representation
  class state_explicit_string:
    public state_explicit<std::string, string_hash>
  {
  public:
    state_explicit_string():
      state_explicit<std::string, string_hash>()
    {
    }

    state_explicit_string(const std::string& label)
      : state_explicit<std::string, string_hash>(label)
    {
    }

    static const std::string default_val;

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
  template<typename State>
  class tgba_explicit_succ_iterator: public tgba_succ_iterator
  {
    //should be better to keep the starting state, see if it mess up something
  public:
    tgba_explicit_succ_iterator(const State* start,
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

    virtual State* current_state() const
    {
      assert(!done());

      //ugly but I can't see any other wayout
      const State* res = down_cast<const State*>(it_->dest);
      assert(res);

      return
	const_cast<State*>(res);
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

    const State* start_;
    typename State::transitions_t::const_iterator it_;
    bdd all_acceptance_conditions_;
  };

  /// Graph implementation for automata representation
  /// \ingroup tgba_representation
  template<typename State, typename Type>
  class explicit_graph: public Type
  {
  protected:
    typedef Sgi::hash_map<typename State::Label_t, State> ls_map;
    typedef Sgi::hash_map<State*, typename State::Label_t> sl_map;
    typedef typename State::transition transition;

  public:
    explicit_graph(bdd_dict* dict)
      : ls_ (),
	sl_ (),
	init_(0),
	dict_ (dict),
	all_acceptance_conditions_(bddfalse),
	all_acceptance_conditions_computed_(false),
	neg_acceptance_conditions_(bddtrue)
    {
    }

    State* add_default_init()
    {
      return add_state(State::default_val);
    }

    transition*
    create_transition(State* source, const State* dest)
    {
      transition t;

      t.dest = dest;
      t.condition = bddtrue;
      t.acceptance_conditions = bddfalse;

      typename State::transitions_t::iterator i =
	source->succ().insert (source->succ ().end (),t);

      return &*i;
    }

    transition*
    get_transition(const tgba_explicit_succ_iterator<State>* si)
    {
      return const_cast<transition*>(&(*(si->i_)));
    }

    void add_condition(transition* t, const ltl::formula* f)
    {
      t->condition &= formula_to_bdd(f, dict_, this);
      f->destroy();
    }

    /// This assumes that all variables in \a f are known from dict.
    void add_conditions(transition* t, bdd f)
    {
      dict_->register_propositions(f, this);
      t->condition &= f;
    }

    /// \brief Copy the acceptance conditions of a tgba.
    ///
    /// If used, this function should be called before creating any
    /// transition.
    void copy_acceptance_conditions_of(const tgba *a)
    {
      assert(neg_acceptance_conditions_ == bddtrue);
      assert(all_acceptance_conditions_computed_ == false);
      bdd f = a->neg_acceptance_conditions();
      dict_->register_acceptance_variables(f, this);
      neg_acceptance_conditions_ = f;
    }

    /// The acceptance conditions.
    void set_acceptance_conditions(bdd acc)
    {
      assert(neg_acceptance_conditions_ == bddtrue);
      assert(all_acceptance_conditions_computed_ == false);

      dict_->register_acceptance_variables(bdd_support(acc), this);
      neg_acceptance_conditions_ = compute_neg_acceptance_conditions(acc);
      all_acceptance_conditions_computed_ = true;
      all_acceptance_conditions_ = acc;
    }

    bool has_acceptance_condition(const ltl::formula* f) const
    {
      return dict_->is_registered_acceptance_variable(f, this);
    }
    
    void add_acceptance_condition(transition* t, const ltl::formula* f)
    {
      bdd c = get_acceptance_condition(f);
      t->acceptance_conditions |= c;
    }

    /// This assumes that all acceptance conditions in \a f are known from dict.
    void add_acceptance_conditions(transition* t, bdd f)
    {
      bdd c = get_acceptance_condition(f);
      t->acceptance_conditions |= c;
    }

    //old tgba explicit labelled interface
    bool has_state(const typename State::Label_t& name)
    {
      return ls_.find(name) != ls_.end();
    }

    const typename State::Label_t& get_label(const State* s) const
    {
      typename sl_map::const_iterator i = sl_.find(s);
      assert(i != sl_.end());
      return i->second;
    }

    const typename State::Label_t& get_label(const spot::state* s) const
    {
      const State* se = down_cast<const State*>(s);
      assert(se);
      return get_label(se);
    }

    transition*
    create_trainsition(const typename State::Label_t& source,
		       const typename State::Label_t& dest)
    {
      // It's important that the source be created before the
      // destination, so the first source encountered becomes the
      // default initial state.
      State* s = add_state(source);
      return create_transition(s, add_state(dest));
    }

    void
    complement_all_acceptance_conditions()
    {
      bdd all = all_acceptance_conditions();
      typename ls_map::iterator i;
      for (i = ls_.begin (); i != ls_.end (); ++i)
	{
	  typename State::transition_t::iterator i2;
	  for (i2 = i->second->succ ().begin ();
	       i2 != i->second->succ ().end (); ++i2)
	    i2->acceptance_conditions = all - i2->acceptance_conditions;
	}
    }

    void
    declare_acceptance_conditions(const ltl::formula* f)
    {
      int v = dict_->register_acceptance_variable(f, this);
      f->destroy();
      bdd neg = bdd_nithvar(v);
      neg_acceptance_conditions_ &= neg;

      // Append neg to all acceptance conditions.
      typename ls_map::iterator i;
      for (i = ls_.begin(); i != ls_.end(); ++i)
	{
	  typename State::transition_t::iterator i2;
	  for (i2 = i->second->succ().begin ();
	       i2 != i->second->succ().end; ++i2)
	    i2->acceptance_conditions &= neg;
	}

      all_acceptance_conditions_computed_ = false;
    }

    void
    merge_transitions()
    {
      typename ls_map::iterator i;
      for (i = ls_.begin(); i != ls_.end(); ++i)
      {
	typename State::transition_t::iterator t1;
	for (t1 = i->second->succ().begin();
	     t1 != i->second->succ().end(); ++t1)
	{
	  bdd acc = t1->acceptance_conditions;
	  const State* dest = t1->dest;

	  // Find another transition with the same destination and
	  // acceptance conditions.
	  typename State::transitions_t::iterator t2 = t1;
	  ++t2;
	  while (t1 != i->seccond->succ().end())
	  {
	    typename State::trasitions_t::iterator t2copy = t2++;
	    if (t2copy->acceptance_conditions == acc && t2copy->dest == dest)
	    {
	      t1->conditions |= t2copy->condition;
	      i->second->succ().erase(t2copy);
	    }
	  }
	}
      }
    }

    /// Return the state_explicit for \a name, creating the state if
    /// it does not exist.
    State* add_state (const typename State::Label_t& name)
    {
      typename ls_map::iterator i = ls_.find(name);
      if (i == ls_.end ())
	{
	  State s (name);
	  ls_[name] = s;
	  sl_[&ls_[name]] = name;

	  // The first state we add is the inititial state.
	  // It can also be overridden with set_init_state().
	  if (!init_)
	    init_ = &ls_[name];

	  return &(ls_[name]);
	}
      return &(i->second);
    }

    State*
    set_init_state(const typename State::Label_t& state)
    {
      State* s = add_state(state);
      init_ = s;
      return s;
    }

    // tgba interface
    virtual ~explicit_graph()
    {
    }

    virtual State* get_init_state() const
    {
      return init_;
    }

    virtual tgba_explicit_succ_iterator<State>*
    succ_iter(const spot::state* state,
	      const spot::state* global_state = 0,
	      const tgba* global_automaton = 0) const
    {
      const State* s = down_cast<const State*>(state);
      assert(s);

      (void) global_state;
      (void) global_automaton;

      return
	new tgba_explicit_succ_iterator<State>(s,
					       this->all_acceptance_conditions ());
    }

    virtual bdd_dict* get_dict() const
    {
      return dict_;
    }

    virtual bdd all_acceptance_conditions() const
    {
      if (!all_acceptance_conditions_computed_)
      {
	all_acceptance_conditions_ =
	  compute_all_acceptance_conditions(neg_acceptance_conditions_);
	all_acceptance_conditions_computed_ = true;
      }
      return all_acceptance_conditions_;
    }

    virtual bdd neg_acceptance_conditions() const
    {
      return neg_acceptance_conditions_;
    }

    //no need?
    virtual std::string format_state(const state* state) const
    {
      (void) state;
      return "coucou";
    }

  protected:

    virtual bdd compute_support_conditions(const spot::state* in) const
    {
      const State* s = down_cast<const State*>(in);
      assert(s);
      const typename State::transitions_t& st = s->succ();

      bdd res = bddfalse;

      typename State::transitions_t::const_iterator i;
      for (i = st.begin (); i != st.end(); ++i)
	res |= i->condition;

      return res;
    }

    virtual bdd compute_support_variables(const spot::state* in) const
    {
      const State* s = down_cast<const State*>(in);
      assert(s);
      const typename State::transitions_t& st = s->succ();

      bdd res = bddtrue;

      typename State::transitions_t::const_iterator i;
      for (i = st.begin (); i != st.end (); ++i)
	res &= bdd_support(i->condition);

      return res;
    }

    bdd get_acceptance_condition(const ltl::formula* f)
    {
      bdd_dict::fv_map::iterator i = dict_->acc_map.find(f);
      assert(has_acceptance_condition(f));
      f->destroy();
      bdd v = bdd_ithvar(i->second);
      v &= bdd_exist(neg_acceptance_conditions_, v);
      return v;
    }

    ls_map ls_;
    sl_map sl_;
    State* init_;

    bdd_dict* dict_;
    mutable bdd all_acceptance_conditions_;
    mutable bool all_acceptance_conditions_computed_;
    bdd neg_acceptance_conditions_;
  };

  template <typename State>
  class tgba_explicit: public explicit_graph<State, tgba>
  {
  protected:
    typedef typename State::transition transition;
  public:
    tgba_explicit(bdd_dict* dict):
      explicit_graph<State,tgba>(dict)
    {
    }

    virtual ~tgba_explicit()
    {
    }

  private:
    // Disallow copy.
    tgba_explicit(const tgba_explicit<State>& other);
    tgba_explicit& operator=(const tgba_explicit& other);
  };
}

#endif // SPOT_TGBA_TGBAEXPLICIT2_HH
