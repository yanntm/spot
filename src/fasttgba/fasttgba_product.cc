// Copyright (C) 2012 Laboratoire de Recherche et Développement
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

#include <sstream>
#include "fasttgba_product.hh"

namespace spot
{
  // ------------------------------------------------------------
  // Fast product state
  // ------------------------------------------------------------

  fast_product_state::fast_product_state(const fasttgba_state* left,
					 const fasttgba_state* right):
    left_(left), right_(right), count_(1)
  {
  }

  int
  fast_product_state::compare(const fasttgba_state* other) const
  {
    const fast_product_state* o = down_cast<const fast_product_state*>(other);
    assert(o);
    int res = left_->compare(o->left());
    if (res != 0)
      return res;
    return right_->compare(o->right());
  }

  size_t
  fast_product_state::hash() const
  {
    return wang32_hash(left_->hash()) ^ wang32_hash(right_->hash());
  }

  fasttgba_state*
  fast_product_state::clone() const
  {
    ++count_;
    return const_cast<fast_product_state *>(this);
  }

  void*
  fast_product_state::external_information() const
  {
    assert(false);
  }

  void
  fast_product_state::destroy() const
  {
    if (--count_)
      return;
    left_->destroy();
    right_->destroy();
    //this->~fast_product_state();
    delete this;
  }

  const fasttgba_state*
  fast_product_state::left() const
  {
    return left_;
  }

  const fasttgba_state*
  fast_product_state::right() const
  {
    return right_;
  }


  // ------------------------------------------------------------
  // Fast product iterator
  // ------------------------------------------------------------

  fast_product_iterator::
  fast_product_iterator(fasttgba_succ_iterator* left,
			fasttgba_succ_iterator* right,
			bool left_is_kripke):
    left_(left), right_(right), kripke_left(left_is_kripke)
  {
  }

  fast_product_iterator::~fast_product_iterator()
  {
    delete left_;
    delete right_;
  }

  void
  fast_product_iterator::first()
  {
    left_->first();
    right_->first();

    // The iterator consider the synchronisation of all
    // outgoing transitions from left with outgoing transitions
    // from right. So when left is done, fix it to zero to
    // detect efficiently when the iteration is done!
    if (left_->done())
      {
    	left_ = 0;
    	return;
      }

    // We have to check if the synchro is available for the first
    // iterator of each tgba ; otherwise iterate until a good one is found
    //    cube current_cond = current_condition();
    cube r = right_->current_condition();
    cube l = left_->current_condition();
    cube current_cond = l & r;

    if (!current_cond.is_valid())
      {
    	next();
      }
  }

  void
  fast_product_iterator::step()
  {
    right_->next();
    if (right_->done())
      {
    	right_->first();
    	left_->next();
      }
  }

  void
  fast_product_iterator::next()
  {
    step();
    while (!done())
      {
    	cube r = right_->current_condition();
    	cube l = left_->current_condition();
    	cube current_cond = l & r;

      	if (current_cond.is_valid())
      	  {
    	    return;
      	  }
	step();
      }
  }

  bool
  fast_product_iterator::done() const
  {
    // Since the product analyses the syncrhonisation of
    // left with right , when left is done it's over.
    return !left_ || left_->done();
  }


  fasttgba_succ_iterator*
  fast_product_iterator::left() const
  {
    return left_;
  }

  fasttgba_succ_iterator*
  fast_product_iterator::right() const
  {
    return right_;
  }


  fasttgba_state*
  fast_product_iterator::current_state() const
  {
    fasttgba_state* s = new fast_product_state(left_->current_state(),//->clone(),
					       right_->current_state());//->clone());
    return s;//->clone();
  }

  cube
  fast_product_iterator::current_condition() const
  {
    assert(!done());
    cube l = left_->current_condition();
    cube r = right_->current_condition();
    return l & r;

  }

  markset
  fast_product_iterator::current_acceptance_marks() const
  {
    // This is an OR because when synchronising two transitions
    // with two different acceptance set, the result must contains
    // as well the first acceptance set as well the second
    if (kripke_left)
      return right_->current_acceptance_marks();

    return left_->current_acceptance_marks() |
      right_->current_acceptance_marks();
  }


  // ------------------------------------------------------------
  // Fast tgba product
  // ------------------------------------------------------------

  fasttgba_product::fasttgba_product(const fasttgba* left,
				     const fasttgba* right,
				     bool left_is_kripke) :
    left_(left), right_(right), kripke_left(left_is_kripke)
  {
    assert(&(left_->get_dict()) == &(right_->get_dict()));
    assert(&(left_->get_acc()) == &(right_->get_acc()));
  }

  fasttgba_product::~fasttgba_product()
  {
  }

  fasttgba_state*
  fasttgba_product::get_init_state() const
  {
    fasttgba_state* init  = new fast_product_state(left_->get_init_state(),//->clone(),
						   right_->get_init_state());//->clone());
    return init;//->clone();
  }

  fasttgba_succ_iterator*
  fasttgba_product::succ_iter(const fasttgba_state* local_state) const
  {
    const fast_product_state* s =
      down_cast<const fast_product_state*>(local_state);
    assert(s);
    fasttgba_succ_iterator* li = left_->succ_iter(s->left());
    fasttgba_succ_iterator* ri = right_->succ_iter(s->right());
    return new fast_product_iterator(li, ri, kripke_left);
  }


  ap_dict&
  fasttgba_product::get_dict() const
  {
    return left_->get_dict();
  }

  acc_dict&
  fasttgba_product::get_acc() const
  {
    return left_->get_acc();
  }

  std::string
  fasttgba_product::format_state(const fasttgba_state* state) const
  {
    const fast_product_state* s =
      down_cast<const fast_product_state*>(state);
    assert(s);
    std::string l = left_->format_state(s->left());
    std::string r = right_->format_state(s->right());
    return l + " -- " + r;
  }

  std::string
  fasttgba_product::transition_annotation(const fasttgba_succ_iterator* t) const
  {
    std::ostringstream oss;
    oss << t->current_condition().dump();

    if (!t->current_acceptance_marks().empty())
      oss << " \\nAcc { " << t->current_acceptance_marks().dump()
    	  << "}";
    return oss.str();
  }

  fasttgba_state*
  fasttgba_product::project_state(const fasttgba_state*,
				  const fasttgba*) const
  {
    assert(false);
  }

  markset
  fasttgba_product::all_acceptance_marks() const
  {
    return left_->all_acceptance_marks();
  }


  unsigned int
  fasttgba_product::number_of_acceptance_marks() const
  {
    return left_->number_of_acceptance_marks();
  }
}
