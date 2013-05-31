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

#include "union_find.hh"
#include <iosfwd>

// #define UFTRACE
#ifdef UFTRACE
#define trace std::cerr << this << "  "
#else
#define trace while (0) std::cerr
#endif

namespace spot
{
  union_find::union_find (acc_dict& a) :
    acc_(a), empty(acc_)
  {
    idneg.push_back(0);
    accp.push_back(&empty);
  }

  union_find::~union_find ()
  {
    for (unsigned i = 0; i < accp.size(); ++i)
      if (accp[i] != &empty)
	delete accp[i];

    uf_map::const_iterator s = el.begin();
    while (s != el.end())
      {
	if (s->first)
	  s->first->destroy();
	++s;
      }
    el.clear();
  }

  bool
  union_find::add (const fasttgba_state* s)
  {
    trace << "union_find::add " << idneg.size() << std::endl << std::flush;
    std::pair<uf_map::iterator, bool> i =
      el.insert(std::make_pair(s, idneg.size()));
    assert(i.second);
    idneg.push_back(-1);
    accp.push_back(&empty);
    return i.second;
  }

  union_find::color
  union_find::get_color(const fasttgba_state* state)
  {
    uf_map::const_iterator i = el.find(state);
    if (i != el.end())
     return Alive;
    else if (is_dead(state))
      return Dead;
    else
      return Unknown;
  }

  int union_find::root (int i)
  {
    int p = idneg[i];
    if (p <= 0)
      return i;
    if (idneg[p] <= 0)
      return p;
    return  idneg[i] = root(p);
  }

  bool
  union_find::same_partition (const fasttgba_state* left,
			      const fasttgba_state* right)
  {
    assert(left);
    assert(right);
    int l  = root(el[left]);
    int r  = root(el[right]);
    trace << "union_find::same_partition? "
	  << l << "("<< el[left] << ")   "
	  << r << "("<< el[right] << ")"
	  << std::endl << std::flush;
    return r == l;
  }

  void
  union_find::make_dead(const fasttgba_state* s)
  {
    trace << "union_find::make_dead " << el[s] << " root_ :"
    	  << root(el[s]) << std::endl << std::flush;
    idneg[root(el[s])] = 0;
  }

  bool
  union_find::is_dead(const fasttgba_state* s)
  {
    return idneg[root(el[s])] == 0;
  }

  void
  union_find::unite (const fasttgba_state* left,
		     const fasttgba_state* right)
  {
    assert(contains(left));
    assert(contains(right));
    int root_left = root(el[left]);
    int root_right = root(el[right]);

    trace << "union_find::unite "
    	  << root_left << " " << root_right << std::endl << std::flush;

    assert(root_left > 0);
    assert(root_right > 0);

    int rk_left = idneg[root_left];
    int rk_right = idneg[root_right];

    // Use ranking
    if (rk_left > rk_right)
      {
    	idneg [root_left] =  root_right;

	// instanciate only when it's necessary
	if (accp[root_left] == &empty && accp[root_right] == &empty)
	  return;

	if (accp[root_left] == &empty)
	  accp[root_left]  = new markset(acc_);

	accp[root_left]->operator|=(*accp[root_right]);
      }
    else
      {
    	idneg [root_right] =  root_left;

	// instanciate only when it's necessary
	if (accp[root_left] == &empty && accp[root_right] == &empty)
	  {
	  }
	else if (accp[root_right] == &empty)
	  {
	    accp[root_right]  = new markset(acc_);
	    accp[root_right]->operator|=(*accp[root_left]);
	  }

    	if (rk_left == rk_right)
    	  {
	    --idneg[root_left];
    	  }
      }
  }

  markset
  union_find::get_acc (const fasttgba_state* s)
  {
    trace << "union_find::get_acc" << std::endl << std::flush;
    int r = root(el[s]);
    assert(r);
    return *accp[r];
  }

  void
  union_find::add_acc (const fasttgba_state* s, markset m)
  {
    trace << "union_find::add_acc" << std::endl << std::flush;
    int r = root(el[s]);

    // instanciate only if necessary
    if (*accp[r] == m)
      return;

    if (accp[r] == &empty)
      accp[r]  = new markset(acc_);

    accp[r]->operator|=(m);
  }

  bool union_find::contains (const fasttgba_state* s)
  {
    trace << "union_find::contains" << std::endl << std::flush;
    return el.find(s) != el.end();
  }

  int
  union_find::max_alive()
  {
    return el.size();
  }

  int
  union_find::max_dead()
  {
    return 0;
  }

  // ------------------------------------------------------------
  // setOfDisjointSetsIPC_LRPC
  // ------------------------------------------------------------

   int setOfDisjointSetsIPC_LRPC::root(int i)
   {
     int p = id[i];
     if (i == p || p == id[p])
       return p;
     p = root(p);
     id[i] = p;
     return p;
   }

  setOfDisjointSetsIPC_LRPC::setOfDisjointSetsIPC_LRPC(acc_dict& acc) :
    union_find(acc),
    el(), id(), rk()
  {
    id.push_back(DEAD);
    rk.push_back(0);
  }

   bool setOfDisjointSetsIPC_LRPC::add(const fasttgba_state* e)
   {
     int n = id.size();
     auto r = el.insert(std::make_pair(e, n));
     assert(r.second);
     id.push_back(n);
     rk.push_back(0);
     return r.second;
   }

  union_find::color
  setOfDisjointSetsIPC_LRPC::get_color(const fasttgba_state* state)
  {
    uf_map::const_iterator i = el.find(state);
    if (i != el.end())
     return Alive;
    else if (is_dead(state))
      return Dead;
    else
      return Unknown;
  }

  void
  setOfDisjointSetsIPC_LRPC::unite(const fasttgba_state* e1,
				   const fasttgba_state* e2)
  {
     auto i1 = el.find(e1);
     auto i2 = el.find(e2);
     assert(i1->second);
     assert(i2->second);
     assert(i1 != el.end() && i2 != el.end());
     // IPC - Immediate Parent Check
     if (id[i1->second] == id[i2->second])
       return;
     int root1 = root(i1->second);
     int root2 = root(i2->second);
     if (root1 == root2)
       return;
     int rk1 = rk[root1];
     int rk2 = rk[root2];
     if (rk1 < rk2)
       id[root1] = root2;
     else
       {
	 id[root2] = root1;
	 if (rk1 == rk2)
	   rk[root1] = rk1 + 1;
       }
   }

   void
   setOfDisjointSetsIPC_LRPC::make_dead(const fasttgba_state* e)
   {
     auto i = el.find(e);
     assert(i != el.end());
     id[root(i->second)] = DEAD;
   }

   bool
   setOfDisjointSetsIPC_LRPC::contains(const fasttgba_state* e)
   {
    return el.find(e) != el.end();
   }

   bool
   setOfDisjointSetsIPC_LRPC::same_partition(const fasttgba_state* e1,
					     const fasttgba_state* e2)
   {
     auto i1 = el.find(e1);
     auto i2 = el.find(e2);
     assert(i1 != el.end() && i2 != el.end());
     return root(i1->second) == root(i2->second);
   }

   bool
   setOfDisjointSetsIPC_LRPC::is_dead(const fasttgba_state* e)
   {
    auto i = el.find(e);
    assert(i != el.end());
    return root(i->second) == DEAD;
   }

  int
  setOfDisjointSetsIPC_LRPC::nbPart() const
  {
    int nb = 0;
    int size = (int) id.size();
    // the dead partition is not considered (i = 1)
    for (int i = 1; i < size; ++i)
      if (i == id[i])
	++nb;
    return nb;
  }

  int
  setOfDisjointSetsIPC_LRPC::maxDepth() const
  {
    int max = 0;
    int size = (int) id.size();
    // the dead partition is not considered (i = 1)
    for (int i = 1; i < size; ++i)
      {
	int d = 0, j = i;
	while (j != id[j])
	  {
	    ++d;
	    j = id[j];
	  }
	if (d > max)
	  max = d;
      }
    return max;
  }

  int
  setOfDisjointSetsIPC_LRPC::maxPart() const
  {
    int max = 0;
    std::unordered_map<int, int> roots;
    int size = (int) id.size();
    // the dead partition is not considered (i = 1)
    for (int i = 1; i < size; ++i)
      {
	int j = i;
	while (j != id[j])
	  j = id[j];
	++roots[j];
      }
    for (auto it = roots.begin(); it != roots.end(); ++it)
      if (it->second > max)
	max = it->second;
    return max;
  }

  void
  setOfDisjointSetsIPC_LRPC::clear()
  {
    el.clear();
    id.clear();
    rk.clear();
    id.push_back(DEAD);
    rk.push_back(0);
  }

  void
  setOfDisjointSetsIPC_LRPC::add_acc (const fasttgba_state*, markset)
  {
    assert(false);
  }

  markset
  setOfDisjointSetsIPC_LRPC::get_acc (const fasttgba_state*)
  {
    assert(false);
  }

  int
  setOfDisjointSetsIPC_LRPC::max_alive()
  {
    return el.size();
  }

  int
  setOfDisjointSetsIPC_LRPC::max_dead()
  {
    return 0;
  }

  // ------------------------------------------------------------
  // setOfDisjointSetsIPC_LRPC_MS
  // ------------------------------------------------------------

  int
  setOfDisjointSetsIPC_LRPC_MS::root(int i)
  {
    assert(i > 0);
    int p = id[i];
    if (p == DEAD)
      return DEAD;
    if (p < 0)
      return i;
    int gp = id[p];
    if (gp == DEAD)
      return DEAD;
    if (gp < 0)
      return p;
    p = root(p);
    id[i] = p;
    return p;
  }

  setOfDisjointSetsIPC_LRPC_MS::setOfDisjointSetsIPC_LRPC_MS(acc_dict& acc) :
    union_find(acc),
    el(), id()
  {
    id.push_back(DEAD);
  }

  bool
  setOfDisjointSetsIPC_LRPC_MS::add(const fasttgba_state* e)
  {
    int n = id.size();
    auto r = el.insert(std::make_pair(e, n));
    assert(r.second);
    id.push_back(-1);
    return r.second;
  }

  union_find::color
  setOfDisjointSetsIPC_LRPC_MS::get_color(const fasttgba_state* state)
  {
    uf_map::const_iterator i = el.find(state);
    if (i == el.end())
      return Unknown;
    else if (root(i->second) == DEAD)
      return Dead;
    else
      return Alive;
  }

  void
  setOfDisjointSetsIPC_LRPC_MS::unite(const fasttgba_state* e1,
				      const fasttgba_state* e2)
  {
    auto i1 = el.find(e1);
    auto i2 = el.find(e2);
    assert(i1 != el.end() && i2 != el.end());
    // IPC - Immediate Parent Check
    int p1 = id[i1->second];
    int p2 = id[i2->second];
    if ((p1 < 0 ? i1->second : p1) == (p2 < 0 ? i2->second : p2))
      return;
    int root1 = root(i1->second);
    int root2 = root(i2->second);
    assert(root1 && root2);
    if (root1 == root2)
      return;
    int rk1 = -id[root1];
    int rk2 = -id[root2];
    if (rk1 < rk2)
      id[root1] = root2;
    else
      {
	id[root2] = root1;
	if (rk1 == rk2)
	  id[root1] = -(rk1 + 1);
      }
  }

  void
  setOfDisjointSetsIPC_LRPC_MS::make_dead(const fasttgba_state* e)
  {
    auto i = el.find(e);
    assert(i != el.end());
    id[root(i->second)] = DEAD;
  }

  bool
  setOfDisjointSetsIPC_LRPC_MS::contains(const fasttgba_state* e)
  {
    return el.find(e) != el.end();
  }

  bool
  setOfDisjointSetsIPC_LRPC_MS::same_partition(const fasttgba_state* e1,
					       const fasttgba_state* e2)
  {
    auto i1 = el.find(e1);
    auto i2 = el.find(e2);
    assert(i1 != el.end() && i2 != el.end());
    return root(i1->second) == root(i2->second);
  }

  bool
  setOfDisjointSetsIPC_LRPC_MS::is_dead(const fasttgba_state* e)
  {
    auto i = el.find(e);
    assert(i != el.end());
    return root(i->second) == DEAD;
  }

  int
  setOfDisjointSetsIPC_LRPC_MS::nbPart() const
  {
    int nb = 0;
    int size = (int) id.size();
    // the dead partition is not considered (i = 1)
    for (int i = 1; i < size; ++i)
      if (id[i] < 0)
	  ++nb;
    return nb;
  }

  int
  setOfDisjointSetsIPC_LRPC_MS::maxDepth() const
  {
    int max = 0;
    int size = (int) id.size();
    // the dead partition is not considered (i = 1)
    for (int i = 1; i < size; ++i)
      {
	int d = 0, j = i;
	while (id[j] > 0)
	  {
	    ++d;
	    j = id[j];
	  }
	if (d > max)
	  max = d;
      }
    return max;
  }

  int
  setOfDisjointSetsIPC_LRPC_MS::maxPart() const
  {
    int max = 0;
    std::unordered_map<int, int> roots;
    int size = (int) id.size();
    // the dead partition is not considered (i = 1)
    for (int i = 1; i < size; ++i)
      {
	int j = i;
	while (id[j] > 0)
	  j = id[j];
	++roots[j];
      }
    for (auto it = roots.begin(); it != roots.end(); ++it)
      if (it->second > max)
	max = it->second;
    return max;
  }

  void
  setOfDisjointSetsIPC_LRPC_MS::clear()
  {
    el.clear();
    id.clear();
    id.push_back(DEAD);
  }

  void
  setOfDisjointSetsIPC_LRPC_MS::add_acc (const fasttgba_state*, markset)
  {
    assert(false);
  }

  markset
  setOfDisjointSetsIPC_LRPC_MS::get_acc (const fasttgba_state*)
  {
    assert(false);
  }

  int
  setOfDisjointSetsIPC_LRPC_MS::max_alive()
  {
    return el.size();
  }

  int
  setOfDisjointSetsIPC_LRPC_MS::max_dead()
  {
    return 0;
  }

  // ------------------------------------------------------------
  // setOfDisjointSetsIPC_LRPC_MS_Dead
  // ------------------------------------------------------------

  int
  setOfDisjointSetsIPC_LRPC_MS_Dead::root(int i)
  {
    assert(i >= 0);
    int p = id[i].id;
    if (p <= 0)
	return i;
    int gp = id[p].id;
    if (gp < 0)
	return p;
    p = root(p);
    id[i].id = p;
    return p;
  }

  setOfDisjointSetsIPC_LRPC_MS_Dead::setOfDisjointSetsIPC_LRPC_MS_Dead
  (acc_dict& acc) : union_find(acc),
		    el(), id(), realsize_(0),
		    max_alive_(0)
  {
    deadstore_ = new deadstore();
    id.push_back({DEAD, 0});
    ++realsize_;
  }

  bool
  setOfDisjointSetsIPC_LRPC_MS_Dead::add(const fasttgba_state* e)
  {
    int n = id.size();
    assert(realsize_ <= n);

    // There is no dead to flush
    if (n == realsize_)
      {
	++realsize_;
	auto r = el.insert(std::make_pair(e, n));
	assert(r.second);
	id.push_back({-1, e});
	// WARNING DEAD must not be counted
	max_alive_ = id.size() -1 > max_alive_ ? id.size() -1 : max_alive_;
	return r.second;
      }
    // Some deads needs to be pushed
    else
      {
	assert(id[realsize_].state);
	auto it = el.find(id[realsize_].state);
	el.erase (it);
	deadstore_->add(id[realsize_].state);
	id[realsize_].state = e;
	id[realsize_].id = -1;
	el.insert(std::make_pair(e, realsize_));
	++realsize_;
	return false;
      }
  }

  union_find::color
  setOfDisjointSetsIPC_LRPC_MS_Dead::get_color(const fasttgba_state* state)
  {
    seen_map::const_iterator i = el.find(state);
    if (i != el.end())
      {
	if (i->second >= realsize_)
	  return Dead;
	else
	  return Alive;
      }
    else if (deadstore_->contains(state))
      return Dead;
    else
      return Unknown;
  }

  void
  setOfDisjointSetsIPC_LRPC_MS_Dead::unite(const fasttgba_state* e1,
					   const fasttgba_state* e2)
  {
    //std::cout << "unite" << std::endl;
    auto i1 = el.find(e1);
    auto i2 = el.find(e2);
    assert(i1 != el.end() && i2 != el.end());
    // IPC - Immediate Parent Check
    int p1 = id[i1->second].id;
    int p2 = id[i2->second].id;
    if ((p1 < 0 ? i1->second : p1) == (p2 < 0 ? i2->second : p2))
      return;
    int root1 = root(i1->second);
    int root2 = root(i2->second);
    //assert(root1 && root2);
    //assert(root1 <= realsize_ && root2 <= realsize_);
    if (root1 == root2)
      return;
    int rk1 = -id[root1].id;
    int rk2 = -id[root2].id;
    if (rk1 < rk2)
      id[root1].id = root2;
    else
      {
	id[root2].id = root1;
	if (rk1 == rk2)
	  id[root1].id = -(rk1 + 1);
      }
  }

  void
  setOfDisjointSetsIPC_LRPC_MS_Dead::make_dead(const fasttgba_state* e)
  {
    auto i = el.find(e);
    assert(i != el.end());
    assert(i->second < realsize_);
    realsize_ = i->second;
  }

  bool
  setOfDisjointSetsIPC_LRPC_MS_Dead::contains(const fasttgba_state*)
  {
    // For this Union Find prefer Get Color.
    assert(false);
  }

  bool
  setOfDisjointSetsIPC_LRPC_MS_Dead::same_partition(const fasttgba_state* e1,
						    const fasttgba_state* e2)
  {
    auto i1 = el.find(e1);
    auto i2 = el.find(e2);
    bool containse1 = deadstore_->contains(e1);
    bool containse2 = deadstore_->contains(e2);

    if (containse1 && containse2)
      return true; // Never Happens?
    if (containse1 || containse2)
      return false;
    assert(i1 != el.end() && i2 != el.end());
    if (i1->second >= realsize_ && i2->second >= realsize_)
      return true;
    if (i1->second >= realsize_ || i2->second >= realsize_)
      return false;

    return root(i1->second) == root(i2->second);
  }

  bool
  setOfDisjointSetsIPC_LRPC_MS_Dead::is_dead(const fasttgba_state* e)
  {
    //std::cout << "is_dead" << std::endl;
    auto i = el.find(e);
    bool containse = deadstore_->contains(e);
    assert(containse || i != el.end());
    return containse || (i->second >= realsize_);
  }

  int
  setOfDisjointSetsIPC_LRPC_MS_Dead::nbPart() const
  {
    assert(false);
    return 0;
  }

  int
  setOfDisjointSetsIPC_LRPC_MS_Dead::maxDepth() const
  {
    assert(false);
    return 0;
  }

  int
  setOfDisjointSetsIPC_LRPC_MS_Dead::maxPart() const
  {
    assert(false);
    return 0;
  }

  void
  setOfDisjointSetsIPC_LRPC_MS_Dead::clear()
  {
    el.clear();
    id.clear();
  }

  void
  setOfDisjointSetsIPC_LRPC_MS_Dead::add_acc (const fasttgba_state*, markset)
  {
    assert(false);
  }

  markset
  setOfDisjointSetsIPC_LRPC_MS_Dead::get_acc (const fasttgba_state*)
  {
    assert(false);
  }

  int
  setOfDisjointSetsIPC_LRPC_MS_Dead::max_alive()
  {
    return max_alive_;
  }

  int
  setOfDisjointSetsIPC_LRPC_MS_Dead::max_dead()
  {
    return deadstore_->size();
  }
}
