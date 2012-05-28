// Copyright (C) 2012 Laboratoire de Recherche et Développement de
// l'Epita.
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
// 02111-1307, USA

#include "degeneralize.hh"
#include "reachiter.hh"
#include "misc/bddlt.hh"
#include "misc/hash.hh"


namespace spot
{
  namespace
  {
    typedef std::list<bdd> bdd_list;
    typedef std::pair<state*, state_explicit_number*> state_pair;
    typedef Sgi::hash_map<state*, state_explicit_number*,
			  state_ptr_hash,
			  state_ptr_equal> state_map;

    typedef std::map<bdd, state_map, bdd_less_than> bdd_states_map;

    struct todo_elt
    {
      todo_elt (const state_pair& p, bdd_list::const_iterator it)
	: sp (p),
	  acc (it)
      {
      }

      state_pair sp;
      bdd_list::const_iterator acc;
    };
    
    class treat_state
    {
    public:
      treat_state (const tgba* a,
		   tgba_explicit_number* out,
		   const bdd_list& acc_cycle,
		   bdd_states_map& done)
	: a_ (a),
	  out_ (out),
	  acc_cycle_ (acc_cycle),
	  done_ (done),
	  statecpt_ (1) //0 (init state) already created
      {
      }

      void operator() (const state_pair& curstates,
		       bdd_list::const_iterator curacc,
		       std::list<todo_elt>& todo)
      {
	done_[*curacc].insert (curstates);

	tgba_succ_iterator* it = a_->succ_iter (curstates.first);

	for (it->first (); !it->done (); it->next ())
	{
	  state* dest = it->current_state ();

	  bdd trans_acc = it->current_acceptance_conditions ();
	  bdd_list::const_iterator tmp_acc = curacc;
	  bdd final_acc = bddtrue;

	  bool something = false;
	  while ((*tmp_acc & trans_acc) != bddfalse)
	  {
	    something = true;

	    final_acc = *tmp_acc;
	    trans_acc -= final_acc;
	    ++tmp_acc;

	    //ensure looping
	    if (tmp_acc == acc_cycle_.end ())
	      tmp_acc = acc_cycle_.begin ();
	  }

	  state_explicit_number* newstate = 0;

	  state_map cur_level = done_[final_acc];
	  state_map::const_iterator f = cur_level.find (dest);
	  if (f == cur_level.end ())
	  {
	    newstate = out_->add_state (statecpt_);
	    ++statecpt_;
	    cur_level.insert (std::make_pair (dest, newstate));
	    todo.push_back (todo_elt (std::make_pair (dest, newstate), tmp_acc));
	  }
	  else
	    newstate = f->second;

	  //TODO handle SBA or TBA?
	  //TBA done for the moment

	  //Add conditions on transition
	  state_explicit_number::transition* t =
	    out_->create_transition (curstates.second, newstate);
	  t->condition = it->current_condition ();
	  
	  //then handle accepting stuff
	  if (something && final_acc == bddtrue)
	    t->acceptance_conditions = bddtrue;
	  else
	    t->acceptance_conditions = bddfalse;
	}
      }
    
    protected:
      const tgba* a_;
      tgba_explicit_number* out_;
      const bdd_list& acc_cycle_;
      bdd_states_map& done_;
      int statecpt_;
    };

    bdd_list build_acclist (bdd allacc)
    {
      bdd_list res;
      std::cout << "allacc = " << allacc << std::endl;

      for (; allacc != bddfalse; allacc = bdd_high (allacc))
      {
	std::cout << "toto" << std::endl;
	res.push_back (bdd_ithvar (bdd_var (allacc)));
      }
      
      return res;
    }
  }

  tgba_explicit_number* degeneralize_tba(tgba* a)
  {
    std::cout << "coucou" << std::endl;

    tgba_explicit_number* out = new tgba_explicit_number(a->get_dict());

    state* init_state = a->get_init_state();

    bdd_list acc_list = build_acclist (a->all_acceptance_conditions ());
    acc_list.push_back (bddtrue);

    //set of visited states for the current level
    bdd_states_map done;
    std::list<todo_elt> todo;

    bdd_list::const_iterator start = acc_list.begin ();
    state_explicit_number* out_init = out->add_state (0);

    //add init state in todo list
    todo.push_front (todo_elt (std::make_pair (init_state, out_init), start));

    treat_state treat (a, out, acc_list, done);

    //TODO keep a visited set or not??!
    while (!todo.empty ())
    {
      todo_elt first = todo.front ();
      todo.pop_front ();
      treat (first.sp, first.acc, todo);
    }

    return out;
  }

  //TODO
  sba_explicit_number* degeneralize_sba(tgba* a)
  {
    sba_explicit_number* out = new sba_explicit_number(a->get_dict());

    return out;
  }
}
