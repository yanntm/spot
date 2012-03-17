// Copyright (C) 2011, 2012 Laboratoire de Recherche et Développement
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

#include <vector>
#include "bddsimul.hh"
#include "tgba/bdddict.hh"
#include "reachiter.hh"
#include "misc/bddlt.hh"
#include "misc/minato.hh"

// for debug
#include "tgba/bddprint.hh"

namespace spot
{

  namespace
  {
    // Encode a tgba using BDD.
    //
    // If an automaton has n states, we need log₂(n) bits to number
    // them from 0 to n-1.  We use BDD variables to encode these
    // log₂(n) bits, hence the name of the class.
    class bdd_log_encoder: public tgba_reachable_iterator_depth_first
    {
    public:
      // Each bit used to number the states is represented by two
      // different BDD variable depending on whether it is used
      // to encode a source (Now) or destination (Next) state.
      //   - bits_[i]    represents Now[i]
      //   - bits_[i]+1  represents Next[i]
      std::vector<int> bits_;
      // The following maps are used for Now<->Next conversion.
      bddPair* now2next_;
      bddPair* next2now_;
      bdd now_vars_;		// the set of all Now[i] vars
      bdd S_;			// set of states (Now)
      bdd R_;			// transition relation
      bdd I_;			// the initial state (Now)

      // This map is used to help converting acceptance conditions
      // into promises.  Basically, a promise is the logical
      // complement of an acceptance condition: an acceptance
      // condition must be seen infinitely often, while a promise must
      // not be seen continuously often.
      //
      // We don't encode these similarly.  In a TGBA that
      // use two acceptance conditions f and g, each
      // acceptance condition is encoded with either
      // (Acc[f]&!Acc[g])  or  (Acc[g]&!Acc[f])
      // A transition labeled by both acceptance condition
      // would have (Acc[f]&!Acc[g])|(Acc[g]&!Acc[f]) as
      // label.
      //
      // For promises, in this file, we use a "more natural" encoding.
      // If a transition makes two promise f and g, we will label it
      // by Prom[f]&Prom[g].  Or actually we labeled it by
      // Acc[f]&Acc[g] because we reuse the same BDD variables that
      // are used for acceptance condition.
      //
      // This map is used to convert
      // bddfalse          to  bddtrue
      // (Acc[f]&!Acc[g])  to  Acc[f]
      // (Acc[g]&!Acc[f])  to  Acc[g]
      // (Acc[f]&!Acc[g])|(Acc[g]&!Acc[f])  to  Acc[f]&Acc[g]
      // this is not a complete conversion to promise.
      //
      // The conversion as promise would be this:
      // bddfalse          to  Acc[f]&Acc[g]
      // (Acc[f]&!Acc[g])  to  Acc[g]
      // (Acc[g]&!Acc[f])  to  Acc[f]
      // (Acc[f]&!Acc[g])|(Acc[g]&!Acc[f])  to  bddtrue
      typedef std::map<bdd, bdd, bdd_less_than> acc2prom_t;
      acc2prom_t acc2prom_;

      /// All acceptance conditions, with the usual encoding.
      /// e.g. (Acc[f]&!Acc[g])|(Acc[g]&!Acc[f])
      bdd all_acc_;

      // All promises, as a conjunction.
      // This is used to finish the conversion into a promise.
      // if acc2prom_[acc] exists, we can convert acc as a promise
      // with:  prom = bdd_exist(all_prom_, acc2prom_[acc]);
      bdd all_prom_;


      // Number of states in the automaton.
      unsigned state_count_;

      bdd_log_encoder(const tgba* a)
	: tgba_reachable_iterator_depth_first(a),
	  now2next_(bdd_newpair()),
	  next2now_(bdd_newpair()),
	  now_vars_(bddtrue),
	  S_(bddfalse),
	  R_(bddfalse),
	  I_(bddfalse),
	  state_count_(0)
      {
	all_acc_ = a->all_acceptance_conditions();
	all_prom_ = bdd_support(all_acc_);

	// Prime the acc2prom_ map with conversion of simple
	// acceptance conditions (no conjunctions).
	bdd all = all_acc_;
	while (all != bddfalse)
	  {
	    // Pick one possible acceptance condition.
	    bdd acc = bdd_satone(all);
	    all -= acc;

	    // Find the unique positive variable on the path.
	    bdd pos = acc;
	    bdd res = bddfalse;
	    while (pos != bddfalse)
	      {
		bdd low = bdd_low(pos);
		if (low == bddfalse)
		  {
		    // Got it.
		    res = bdd_ithvar(bdd_var(pos));
		    break;
		  }
		pos = low;
	      }
	    assert(res != bddfalse);
	    // Remember this association.
	    acc2prom_[acc] = res;
	  }
	// Special case when no acceptance condition is used.
	acc2prom_[bddfalse] = bddtrue;
      }

      ~bdd_log_encoder()
      {
	aut_->get_dict()->unregister_all_my_variables(this);
      }

      // Convert state number s into a BDD.
      // This simply encodes the value of s using Now/Next variables.
      // The choice of Now/Next or indicated by the next argument.
      bdd convstate(int s, bool next)
      {
	bdd res = bddtrue;
	unsigned i = 0;		// bit number.
	while (s)
	  {
	    int b;
	    if (i < bits_.size())
	      {
		b = bits_[i];
	      }
	    else
	      {
		// It seems we need more bits that we have seen so
		// far.  Allocate one more.  That means two variables:
		// one for Now, and one for Next.
		b = aut_->get_dict()->register_anonymous_variables(2, this);
		bits_.push_back(b);
		bdd nnow = bdd_nithvar(b);
		bdd nnext = bdd_nithvar(b + 1);
		// All the sets and relations we have built so
		// far now have to be aware of these new variables.
		S_ &= nnow;
		I_ &= nnow;
		R_ &= nnow & nnext;
		now_vars_ &= bdd_ithvar(b);
		bdd_setpair(now2next_, b, b + 1);
		bdd_setpair(next2now_, b + 1, b);
	      }
	    b += next;
	    // Actually add the bit to res.
	    res &= (s & 1) ? bdd_ithvar(b) : bdd_nithvar(b);
	    s >>= 1;
	    ++i;
	  }
	// If we did not hits the last known bit, finish the encoding
	// with trailing zeroes (i.e. negated variables).
	while (i < bits_.size())
	  res &= bdd_nithvar(bits_[i++] + next);

	return res;
      }

      virtual
      void process_state(const state*, int, tgba_succ_iterator*)
      {
	++state_count_;
      }

      virtual
      void process_link(const state*, int in,
			const state*, int out,
			const tgba_succ_iterator* si)
      {
	// Encode source and destination.
	bdd bin = convstate(in, false);
	unsigned usedbits = bits_.size();
	bdd bout = convstate(out, true);
	unsigned usedbits2 = bits_.size();

	// Complete bin with missing bits, if any new bit was created
	// while converting bout.
	if (usedbits < usedbits2)
	  bin = bdd_satoneset(bin, now_vars_, bddfalse);

	// The first state we encode is the initial state.
	if (I_ == bddfalse)
	  I_ = bin;

	// Don't register destination states in S_.  If there is a
	// state that has no successors, then we can remove it safely
	// from the automaton anyway.
	S_ |= bin;

	// Convert the acceptance condition acc into a promise.
	bdd acc = si->current_acceptance_conditions();
	acc2prom_t::const_iterator it = acc2prom_.find(acc);
	if (it != acc2prom_.end())
	  {
	    acc = it->second;
	  }
	else
	  {
	    // Acc is a disjunction of acceptance variables.
	    bdd res = bddtrue;
	    bdd all = acc;
	    while (all != bddfalse)
	      {
		bdd one = bdd_satone(all);
		all -= one;
		it = acc2prom_.find(one);
		assert(it != acc2prom_.end());
		res &= it->second;
	      }
	    acc2prom_[acc] = res; // Cache result.
	    acc = res;
	  }

	bdd prom = bdd_exist(all_prom_, acc);

	// Finally add the transition to the transition relation.
	R_ |= bin & si->current_condition() & prom & bout;
      }

    };

    // Perform the simulation on a BDD-encoded tgba.  (The encoding is
    // done in the constructor.)
    class bdd_simulator
    {
    private:
      const tgba* aut_;		// The automaton to encode.

      // Most of these following variables are the same as in the
      // bdd_log_encoder class.
      bdd S_;			// State set.  (Now)
      bdd R_;			// transition Relation.
      bdd I_;			// Initial state.  (Now)
      bddPair* now2next_;	// Conversion Now->Next
      bddPair* next2now_;	// Conversion Next->Now
      bdd now_vars_;		// Conjunction of all Now variables.
      bdd next_vars_;		// Conjunction of all Next variables.
      bdd all_acc_;		// All acceptance conditions (spot encoding).
      bdd all_prom_;		// All promises as a conjunction.
      std::vector<bdd> classes_; // BDD variable for each class.
      // Partial order between classes.  For each class C,
      // class_po_[C] contains the conjunction of all implied classes.
      std::vector<bdd> classes_po_;
      bdd class_vars_;		// Conjunction of Class variables (positive).
      bdd class_vars_neg_;      // Conjunction of Class variables (negative).
      unsigned class_count_;	// Number of classes used so far.
      bdd part_rel_;
      std::vector<bool> is_nownextvar_; // set of Now & Next variables.
      typedef std::map<bdd, bdd, bdd_less_than> bdd_bdd_map_t;
      typedef std::map<bdd, unsigned, bdd_less_than> bdd_id_map_t;
      bdd_bdd_map_t part_cache_;
      bdd_id_map_t part_cache_final_;
      bdd_bdd_map_t negate_prom_cache_; // cache for negate_prom()

    public:
      bdd_simulator(const tgba* a)
	: aut_(a), class_count_(0)
      {
	aut_->get_dict()->register_all_variables_of(a, this);
	{
	  // log-encode the automaton "a", copy most of its BDDs.
	  bdd_log_encoder ble(a);
	  ble.run();
	  aut_->get_dict()->register_all_variables_of(&ble, this);
	  S_ = ble.S_;
	  R_ = ble.R_;
	  I_ = ble.I_;
	  now2next_ = ble.now2next_;
	  next2now_ = ble.next2now_;
	  now_vars_ = ble.now_vars_;
	  next_vars_ = bdd_replace(now_vars_, now2next_);
	  all_acc_ = ble.all_acc_;
	  all_prom_ = ble.all_prom_;

	  // The max number of classes is the number of states.
	  // We will allocate one variable per class.
	  int max_classes = ble.state_count_;

	  // n is the number of BDD variables we will use.
	  int n = bdd_varnum() + max_classes;

	  // Create a BDD variable order in which
	  // 1. Now & Next variables come first,
	  // 2. Class variables come next,
	  // 3. labels (atomic proposition and promises) come last.
	  int* order = new int[n];
	  int* pos = order;
	  std::set<int> ordered; // Remember which variable has been ordered

	  // Now & Next variables comes first
	  // (At the same time, fill in is_nownextvar_.)
	  is_nownextvar_.resize(n, false);
	  for (std::vector<int>::const_iterator it = ble.bits_.begin();
	       it != ble.bits_.end(); ++it)
	    {
	      *pos++ = *it;
	      *pos++ = *it + 1;
	      is_nownextvar_[*it] = true;
	      ordered.insert(*it);
	      is_nownextvar_[*it + 1] = true;
	      ordered.insert(*it + 1);
	    }

	  // Allocate BDD variables for classes.
	  int c_base =
	    aut_->get_dict()->register_anonymous_variables(max_classes,
							   this);
	  // At the same time as we complete the BDD order,
	  // also initialize classes_, class_vars_, and class_vars_neg.
	  classes_.reserve(max_classes);
	  class_vars_ = bddtrue;
	  class_vars_neg_ = bddtrue;
	  for (int i = 0; i < max_classes; ++i)
	    {
	      int c_num = c_base + i;
	      *pos++ = c_num;
	      ordered.insert(c_num);
	      bdd p = bdd_ithvar(c_num);
	      classes_.push_back(p);
	      class_vars_ &= p;
	      class_vars_neg_ &= bdd_nithvar(c_num);
	    }
	  // Initially each class implies itself.
	  classes_po_ = classes_;

	  // Add all the remaining variables (atomic propositions,
	  // promises, as well as any other variable we do not use)
	  // at the end of the BDD order.
	  for (int i = 0; i < n; ++i)
	    if (ordered.find(i) == ordered.end())
	      *pos++ = i;

	  // Activate our variable order.
	  bdd_setvarorder(order);
	  delete[] order;
	}

	// Remove transitions that have a destination state not in S_.
	// This may happen, because S_ only contain source states.
	R_ &= bdd_replace(S_, now2next_);

	// Let's refine the classes.
	refine();
      }

      // For debug.  Print a BDD.
      void dumprel(bdd rel)
      {
	bdd svars = now_vars_ & next_vars_;
	bdd notcondvars = svars & all_prom_ & class_vars_;

	bdd csup = bdd_support(rel);
	bdd sup = bdd_existcomp(csup, svars);
	bool have_cond = csup != sup;

	while (rel != bddfalse)
	  {
	    bdd one = bdd_satoneset(rel, sup, bddfalse);
	    rel -= one;

	    bdd src = bdd_existcomp(one, now_vars_);
	    bdd dst = bdd_existcomp(one, next_vars_);
	    bdd acc = bdd_existcomp(one, all_prom_);
	    bdd cls = bdd_existcomp(one, class_vars_);
	    bdd cnd = bdd_exist(one, notcondvars);
	    std::cerr << "-";
	    if (src != bddtrue)
	      std::cerr << " src=" << src;
	    if (dst != bddtrue)
	      std::cerr << " dst=" << dst;
	    if (cls != bddtrue)
	      std::cerr << " cls=" << cls;
	    if (have_cond)
	      {
		std::cerr << " cnd=" << cnd;
		std::cerr << " acc=" << acc;
	      }
	    std::cerr << std::endl;
	  }
      }

      // Main loop of the algorithm.
      //
      // Build a partition of the states of the automaton by
      // successive refinement.  Maintain a partial order of the
      // various classes along the way.
      void refine()
      {
	// A partition, represented as a BDD, is just a relation
	// between states (represented by Next or Now variables
	// depending on the context) and Class variables.

	bdd oldpart = bddfalse;	// Previous partition (using Next variables)
	bdd part = bddtrue;     // Current partition (using Next variables)

	while (oldpart != part)
	  {
	    oldpart = part;
	    // The bdd_relprod() call will replace all next variable
	    // of the transition relation R_ by their class number, as
	    // given by the current partition.  In practice this
	    // create a relation in which each state is associated to
	    // its signature (outgoing labels and destination class).
	    //
	    // partition() will recurse the resulting BDD to compute
	    // a new partitioned relation based on these signatures.
	    part_rel_ = partition(bdd_relprod(R_, part, next_vars_));
	    // Since the result above use Now variables, convert that
	    // to Next variables.
	    part_rel_ = bdd_replace(part_rel_, now2next_);
	    // Compute the partial order on the classes created by
	    // partition().
	    bdd po = partial_order();
	    // Update the partition, taking the partial order into
	    // account.
	    part = part_rel_ & po;
	  }
      }

      // Recursive worker for partition().
      //
      // Because of the BDD variable order we are using, all the
      // states (represented by Now or Next variables) that have same
      // signature (all the other variables) are actually sharing
      // their signature.  To compute this new partition, all we have
      // to do is explore all paths in the BDD until we hit a node
      // whose variable is not Now nor Next, and replace this node by
      // a new class.
      // part_cache_ ensure we do not process a node twice.
      bdd partition_rec(bdd in)
      {
	if (in == bddfalse)
	  return in;

	bdd_bdd_map_t::const_iterator it = part_cache_.find(in);
	if (it != part_cache_.end())
	  return it->second;

	if (in != bddtrue)
	  {
	    int var = bdd_var(in);
	    if (is_nownextvar_[var])
	      {
		bdd low = partition_rec(bdd_low(in));
		bdd high = partition_rec(bdd_high(in));
		bdd res = bdd_ite(bdd_ithvar(var), high, low);
		part_cache_[in] = res;
		return res;
	      }
	  }
	bdd res = classes_[class_count_];
	part_cache_[in] = res;
	part_cache_final_[in] = class_count_;
	++class_count_;
	return res;
      }

      bdd partition(bdd in)
      {
	part_cache_.clear();
	part_cache_final_.clear();
	class_count_ = 0;
	return partition_rec(in);
      }

      // Compute partial order between partition classes
      // This function compute two results:
      // - classes_po_[C] is the conjunction of all classes implied by C
      // - rel is the conjunction of all implications.  For instance if
      //   both C₁ and C₂ imply C₃, then rel = (C₁→C₃)∧(C₂→C₃).
      bdd partial_order()
      {
	bdd rel = bddtrue;
	assert(part_cache_final_.size() == class_count_);
	bdd_id_map_t::const_iterator i, j;
	for (i = part_cache_final_.begin(); i != part_cache_final_.end(); ++i)
	  {
	    bdd irel = classes_[i->second];
	    for (j = part_cache_final_.begin();
		 j != part_cache_final_.end(); ++j)
	      {
		if (i == j)
		  continue;
		// We detect that "a&b -> a" by testing "a&b&a = a&b".
		if ((i->first & j->first) == i->first)
		  {
		    bdd ip = classes_[i->second];
		    bdd jp = classes_[j->second];
		    rel &= (ip >> jp);
		    irel &= jp;
		  }
	      }
	    classes_po_[i->second] = irel;
	  }
	return rel;
      }

      // Once the final refinement of the partition has been
      // achieved it is time to build a partition that we
      // can actually use.
      // This work similarly to partition_rec, but instead
      // of renumbering each class, we label it by the conjunction
      // of all implied classes.
      bdd partition_finalize_rec(bdd in)
      {
	if (in == bddfalse)
	  return in;

	bdd_bdd_map_t::const_iterator it = part_cache_.find(in);
	if (it != part_cache_.end())
	  return it->second;

	if (in != bddtrue)
	  {
	    int var = bdd_var(in);
	    if (is_nownextvar_[var])
	      {
		bdd low = partition_finalize_rec(bdd_low(in));
		bdd high = partition_finalize_rec(bdd_high(in));
		bdd res = bdd_ite(bdd_ithvar(var), high, low);
		part_cache_[in] = res;
		return res;
	      }
	  }
	bdd res = classes_po_[class_count_];
	part_cache_[in] = res;
	part_cache_final_[in] = class_count_;
	++class_count_;
	return res;

      }

      bdd partition_finalize(bdd in)
      {
	unsigned c = class_count_;
	class_count_ = 0;
	part_cache_.clear();
	bdd res = partition_finalize_rec(in);
	assert(c == class_count_);
	return res;
      }

      // Negate all the variables of a promise.
      // For instance  a&b&c  will become  (!a)&(!b)&(!c)
      // The input should be a positive conjunction of variables.
      bdd negate_prom(bdd in)
      {
	if (in == bddtrue)
	  return bddtrue;

	const bdd_bdd_map_t::const_iterator i = negate_prom_cache_.find(in);
	if (i != negate_prom_cache_.end())
	  return i->second;

	assert (in != bddfalse);
	// All input variables are positive.
	assert(bdd_high(in) != bddfalse);

	bdd res = bdd_ite(bdd_ithvar(bdd_var(in)),
			  bddfalse, negate_prom(bdd_high(in)));
	negate_prom_cache_[in] = res;
	return res;
      }

      // Convert a promise back into an acceptance condition.
      bdd prom2acc(bdd prom)
      {
	return all_acc_ & negate_prom(prom);
      }

      // Build an explicit automaton based on the computed partition.
      tgba_explicit_number*
      build_quotient()
      {
	tgba_explicit_number* a = new tgba_explicit_number(aut_->get_dict());

	// Associate class variables to states.
	// Keys used here are conjunctions of class variables, as
	// filled in classes_po_.
	bdd_id_map_t classes2state;

	// Create one state per partition.
	for (unsigned i = 0; i < class_count_; ++i)
	  {
	    a->add_state(i);
	    classes2state[classes_po_[i]] = i;
	    // classes2state[classes_[i]] = i;
	  }

	// Compute the final partition, with all implied classes.
	bdd partition = partition_finalize(part_rel_);
	// Inject the labels (promises and atomic properties) in
	// partition, and remove destinations (Next).
	bdd succpart = bdd_relprod(R_, partition, next_vars_);

	// The list of all class and promise variables.  To be used
	// with bdd_exist()
	bdd class_prom_vars = class_vars_ & all_prom_;

	// Now compute the outgoing transitions of each state.
	for (unsigned i = 0; i < class_count_; ++i)
	  {
	    // Get the Now variables representing the states
	    // of partition #i.

	    // First get the class variable for i, and compute the
	    // a conjunction where all class variables appear
	    // negatively except the one for i.
	    bdd c = classes_[i];
	    assert(c == bdd_support(c));
	    bdd partfilter = bdd_exist(class_vars_neg_, c) & c;
	    // Then combine that with part_rel_ to retrieve
	    // the associated Next variables.
	    bdd states = bdd_relprod(part_rel_, partfilter, class_vars_);
	    // And finally replace Next vars by Now vars.
	    states = bdd_replace(states, next2now_);


	    // Does this set contain an initial state?
	    if ((states & I_) != bddfalse)
	      a->set_init_state(i);

	    // Common signature of all these states.
	    bdd sig = bdd_relprod(succpart, states, now_vars_);

	    // The signature contain all the information about
	    // the outgoing transitions: atomic propositions, promises,
	    // and classes of the destination.  We are going to iterate
	    // all these path to create the transitions.

	    // The set of atomic propositions occuring in the signature.
	    bdd var_set = bdd_exist(bdd_support(sig), class_prom_vars);
	    // ALL_PROPS is the combinations we have yet to consider.
	    // We used to start with `all_props = bddtrue', but it is
	    // more efficient to start with the set of all satisfiable
	    // variables combinations.
	    bdd all_props = bdd_exist(sig, class_prom_vars);

	    // First loop over all possible valuations atomic properties.
	    while (all_props != bddfalse)
	      {
		bdd cond = bdd_satoneset(all_props, var_set, bddtrue);
		all_props -= cond;

		// For each possible valuation, iterator over all possible
		// destination classes.   We use minato_isop here, because
		// if the same valuation of atomic properties can go
		// to two different classes C₁ and C₂, iterating on
		// C₁∨C₂ with the above bdd_satoneset loop will see
		// C₁ then (¬C₁)∧C₂, instead of C₁ then C₂.
		// With minatop_isop, we ensure that the no negative
		// class variable will be seen (likewise for promises).
		minato_isop isop(bdd_relprod(sig, cond, var_set));
		bdd cond_prom_dest;
		while ((cond_prom_dest = isop.next()) != bddfalse)
		  {
		    // Get the class variable representing the destination
		    bdd dest = bdd_existcomp(cond_prom_dest, class_vars_);

		    // Which state does it corresponds to?
		    bdd_id_map_t::const_iterator it = classes2state.find(dest);
		    assert(it != classes2state.end());

		    // Get the promise.
		    bdd prom = bdd_existcomp(cond_prom_dest, all_prom_);

		    tgba_explicit_number::transition* t
		      = a->create_transition(i, it->second);
		    a->add_conditions(t, cond);
		    a->add_acceptance_conditions(t, prom2acc(prom));
		  }
	      }
	  }
	a->merge_transitions();
	return a;
      }

      ~bdd_simulator()
      {
	bdd_freepair(now2next_);
	bdd_freepair(next2now_);
	aut_->get_dict()->unregister_all_my_variables(this);
      }
    };
  }

  tgba_explicit* bdd_simulation(const tgba* a)
  {
    bdd_simulator bs(a);
    return bs.build_quotient();
  }
}
