#include "propagation.hh"

#include "tgba/tgbaexplicit.hh"
#include "reachiter.hh"
#include "misc/hash.hh"

namespace spot
{
  typedef Sgi::hash_set<const state*,
			state_ptr_hash, state_ptr_equal> state_set;
  typedef Sgi::hash_map<const state*, bdd,
			state_ptr_hash, state_ptr_equal> state_map;
  typedef Sgi::hash_map<const state*, unsigned int,
			state_ptr_hash, state_ptr_equal> exp_map;

  // return true if a change has been made to the acc map during the run
  // false otherwise
  bool
  propagation_dfs (const tgba* a, const state* s,
		   state_set& seen, state_map& acc)
  {
    bool changed = false;

    seen.insert (s);

    if (acc.find (s) == acc.end ())
      acc.insert (std::make_pair<const state*, bdd> (s, bddtrue));

    tgba_succ_iterator* i = a->succ_iter (s);
    for (i->first (); !i->done (); i->next ())
    {
      bdd cmp = acc[s];
      acc[s] &= i->current_acceptance_conditions ();
      changed |= cmp != acc[s];
    }

    delete i;

    i = a->succ_iter (s);
    for (i->first (); !i->done (); i->next ())
    {
      state* to = i->current_state ();

      if (seen.find (to) == seen.end ())
	changed |= propagation_dfs (a, to, seen, acc);

      bdd cmp = acc[s];
      acc[s] |= acc[to];
      changed |= cmp != acc[s];

      to->destroy ();
    }
    delete i;

    return changed;
  }

  class rewrite_automata: public tgba_reachable_iterator_depth_first
  {
  public:
    rewrite_automata (const tgba* a,
		      state_map& acc):
      tgba_reachable_iterator_depth_first (a),
      acc_ (acc),
      rec_ (),
      newa_ (new tgba_explicit_number (a->get_dict ()))
    {
    }

    void
    process_state (const state* s, int n, tgba_succ_iterator* si)
    {
      (void) si;

      if (rec_.find (s) == rec_.end ())
	rec_.insert (std::make_pair<const state*, unsigned int> (s, n));
    }

    void
    process_link (const state* in_s, int in,
		  const state* out_s, int out,
		  const tgba_succ_iterator* si)
    {
      (void) in;
      (void) out;

      tgba_explicit::transition* t =
	newa_->create_transition (rec_[in_s], rec_[out_s]);

      newa_->add_acceptance_conditions (t, acc_[out_s]);
      newa_->add_conditions (t, si->current_condition ());
    }
    
    tgba_explicit_number*
    get_new_automata () const
    {
      return newa_;
    }

  protected:
    state_map& acc_;
    exp_map rec_;
    tgba_explicit_number* newa_;
  };

  const tgba*
  propagate_acceptance_conditions (const tgba* a)
  {
    state_set seen;
    state_map acc;
    state* init_state = a->get_init_state ();

    while (propagation_dfs (a, init_state, seen, acc)) ;
    init_state->destroy ();

    std::cout << "ACC size = " << acc.begin ()->second << std::endl;

    //rewrite automata
    rewrite_automata rw (a, acc);
    rw.run ();

    return rw.get_new_automata ();
  }
}
