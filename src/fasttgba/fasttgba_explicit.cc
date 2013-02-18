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
#include "fasttgba_explicit.hh"

namespace spot
{
  // ----------------------------------------------------------------------
  // fast_explicit_state code here
  // ----------------------------------------------------------------------

  void
  fast_explicit_state::destroy() const
  {

  }

  fast_explicit_state::fast_explicit_state(int label):
    label_(label)
  {
  }

  int
  fast_explicit_state::compare(const fasttgba_state* other) const
  {
    return label_ - ((const fast_explicit_state*)other)->label_;
  }

  size_t
  fast_explicit_state::hash() const
  {
    return label_;
  }

  fasttgba_state*
  fast_explicit_state::clone() const
  {
    return const_cast<fast_explicit_state *>(this);
  }

  void*
  fast_explicit_state::external_information() const
  {
    assert(false);
  }

  int
  fast_explicit_state::label() const
  {
    return label_;
  }

  void
  fast_explicit_state::add_successor(const struct transition t)
  {
    successors.insert(successors.end(), t);
  }

  // ----------------------------------------------------------------------
  // fast_explicit_iterator code here
  // ----------------------------------------------------------------------

  fast_explicit_iterator::fast_explicit_iterator
  (const fast_explicit_state* state):
    start_(state)
  {
  }

  fast_explicit_iterator::~fast_explicit_iterator()
  {
  }

  void
  fast_explicit_iterator::first()
  {
    it_ = start_->successors.begin();
  }

  void
  fast_explicit_iterator::next()
  {
    ++it_;
  }

  bool
  fast_explicit_iterator::done() const
  {
    return it_ == start_->successors.end();
  }

  fasttgba_state*
  fast_explicit_iterator::current_state() const
  {
    assert(!done());
    return const_cast<fast_explicit_state*>(it_->dst);
  }

  cube
  fast_explicit_iterator::current_condition() const
  {
    return it_->conditions;
  }

  markset
  fast_explicit_iterator::current_acceptance_marks() const
  {
    return it_->acceptance_marks;
  }

  // ----------------------------------------------------------------------
  // fasttgbaexplicit code here
  // ----------------------------------------------------------------------

  fasttgbaexplicit::fasttgbaexplicit(ap_dict* aps,
				     acc_dict* acc):
    all_marks_ (*acc),
    init_(0)
  {
    aps_ = aps;
    acc_ = acc;
    all_marks_ = ~all_marks_;
  }

  fasttgbaexplicit::~fasttgbaexplicit()
  {
    // Delete all states
    sm::iterator i = state_map_.begin();

    while (i != state_map_.end())
      {
	i->second->destroy();
	++i;
      }
  }

  fasttgba_state*
  fasttgbaexplicit::get_init_state() const
  {
    return init_->clone();
  }

  fasttgba_succ_iterator*
  fasttgbaexplicit::succ_iter(const fasttgba_state* state) const
  {
    const fast_explicit_state* s =
      down_cast<const fast_explicit_state*>(state);
    assert(s);

    return new fast_explicit_iterator(s);
  }

  ap_dict&
  fasttgbaexplicit::get_dict() const
  {
    return *aps_;
  }

  acc_dict&
  fasttgbaexplicit::get_acc() const
  {
    return *acc_;
  }

  std::string
  fasttgbaexplicit::format_state(const fasttgba_state* s) const
  {
    std::ostringstream oss;
    oss << down_cast<const fast_explicit_state*> (s)->label();
    return oss.str();
  }


  std::string
  fasttgbaexplicit::transition_annotation(const fasttgba_succ_iterator* t) const
  {
    std::ostringstream oss;
    oss << t->current_condition().dump();

    if (!t->current_acceptance_marks().empty())
      oss << " \\nAcc { " << t->current_acceptance_marks().dump()
    	  << "}";
    return oss.str();
  }

  fasttgba_state*
  fasttgbaexplicit::project_state(const fasttgba_state* ,
				  const fasttgba*) const
  {
    assert(false);
  }

  markset
  fasttgbaexplicit::all_acceptance_marks() const
  {
    return all_marks_;
  }

  unsigned int
  fasttgbaexplicit::number_of_acceptance_marks() const
  {
    return acc_->size();
  }

  fast_explicit_state*
  fasttgbaexplicit::add_state(int s)
  {
    sm::iterator available = state_map_.find(s);
    if (available == state_map_.end())
      {
	fast_explicit_state *the_state = new fast_explicit_state(s);
	state_map_.insert(std::make_pair (s, the_state));

	// Initial state not yet fixed
	if (init_ == 0)
	  init_ = the_state;

	return the_state;
      }
    return available->second;
  }

  void
  fasttgbaexplicit::add_transition(int src, int dst,
				   cube cond, markset acc)
  {
    fast_explicit_state* source = 0;
    fast_explicit_state* destination = 0;
    source =  add_state(src);
    destination = add_state(dst);

    // Now we just have to create condition and acceptance over
    // the transition
    spot::transition t = {cond, acc, destination};
    source->add_successor(t);
  }
}
