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

#ifndef SPOT_FASTTGBAALGOS_EC_UNION_FIND_HH
# define SPOT_FASTTGBAALGOS_EC_UNION_FIND_HH

#include "misc/hash.hh"
#include "fasttgba/fasttgba.hh"
#include "boost/tuple/tuple.hpp"


#include <cassert>
#include <iostream>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace spot
{
  /// This class is a wrapper for manipulating Union
  /// Find structure in emptiness check algotithms
  ///
  /// It's an efficient data structure to compute SCC
  /// In order to be efficient, some methods are added
  /// to impove the efficience of emptiness check algorithm
  ///
  /// Performing operation with a state == 0 will be analysed
  /// as an operation with a dead state cf cou99_uf
  class union_find
  {
  public:

    /// \brief The constructor for the Union-Find structure
    /// \a acc the acceptance dictionary
    union_find (acc_dict& acc);

    /// \brief A simple destructor
    virtual ~union_find ();

    /// \brief Add a partition that contains only \a s
    /// Suppose a clone() has been done and the union-find
    /// is in charge to perform the destroy()
    virtual bool add (const fasttgba_state* s);

    /// \brief Perform the union of the two partition containing
    /// \a left and \a right. No assumptions over the resulting
    /// parent can be do.
    virtual void unite (const fasttgba_state* left,
			const fasttgba_state* right);

    /// \brief Return true if the partition of \a left is the
    /// same that the partition of \a right
    virtual bool same_partition (const fasttgba_state* left,
    			 const fasttgba_state* right);

    /// \brief Add the acceptance set to the partition that contains
    /// the state \a s
    virtual void add_acc (const fasttgba_state* s, markset m);

    /// \brief return the acceptance set of the partition containing
    /// \a s
    virtual markset get_acc (const fasttgba_state* s);

    /// \brief Return wether a state belong to the Union-Find structure
    virtual bool contains (const fasttgba_state* s);

    /// \brief perform a union with dead
    virtual void make_dead (const fasttgba_state* s);

    /// \brief check wether the root of the set containing
    /// this scc is dead.
    virtual bool is_dead (const fasttgba_state* s);

  protected:

    /// \brief grab the id of the root associated to an element.
    virtual int root (int i);

    // type def for the main structure of the Union-Find
    typedef Sgi::hash_map<const fasttgba_state*, int,
			  fasttgba_state_ptr_hash,
			  fasttgba_state_ptr_equal> uf_map;

    /// \brief the structure used to the storage
    /// An element is associated to an integer
    uf_map el;

    /// \brief For each element store the id of the parent
    // std::vector<int> id;
    std::vector<int> idneg;

    /// \brief rank associated to each subtrees.
    // std::vector<int> rk;

    /// \brief Acceptance associated to each element
    std::vector<markset *> accp;

    /// \brief The acceptance dictionary
    acc_dict& acc_;

    /// Avoid to re-create some elements
    markset empty;
  };

  // template<typename const fasttgba_state*,
  // 	   typename Hash = std::hash<const fasttgba_state*>,
  // 	   typename Pred = std::equal_to<const fasttgba_state*> >
  class SetOfDisjointSetsIPC_LRPC: public union_find
  {
  private:
    std::unordered_map<const fasttgba_state*, int,
		       fasttgba_state_ptr_hash,
		       fasttgba_state_ptr_equal> el;
    mutable std::vector<int> id;
    mutable std::vector<int> rk;
    // id of a specially managed partition of "dead" elements
    const int DEAD = 0;

    int root(int i) const {
      int p = id[i];
      if (i == p || p == id[p])
	return p;
      p = root(p);
      id[i] = p;
      return p;
    }
  public:
    SetOfDisjointSetsIPC_LRPC(acc_dict& acc) :
      union_find(acc),
      el(), id(), rk()
    {
      id.push_back(DEAD);
      rk.push_back(0);
    }

    bool add(const fasttgba_state* e) {
      int n = id.size();
      auto r = el.insert(std::make_pair(e, n));
      assert(r.second);
      id.push_back(n);
      rk.push_back(0);
      return r.second;
    }

    void unite(const fasttgba_state* e1, const fasttgba_state* e2) {
      auto i1 = el.find(e1);
      auto i2 = el.find(e2);
      assert(i1->second);
      assert(i2->second);
      assert(i1 != el.end() && i2 != el.end());
      // IPC - Immediate Parent Check
      if (id[i1->second] == id[i2->second])
	return ;//false;
      int root1 = root(i1->second);
      int root2 = root(i2->second);
      if (root1 == root2)
	return ;//false;
      int rk1 = rk[root1];
      int rk2 = rk[root2];
      if (rk1 < rk2)
	id[root1] = root2;
      else {
	id[root2] = root1;
	if (rk1 == rk2)
	  rk[root1] = rk1 + 1;
      }
      //return true;
    }

    void make_dead(const fasttgba_state* e) {
      auto i = el.find(e);
      assert(i != el.end());
      id[root(i->second)] = DEAD;
    }

    bool contains(const fasttgba_state* e)  {
      return el.find(e) != el.end();
    }

    bool same_partition(const fasttgba_state* e1,
			 const fasttgba_state* e2)  {
      auto i1 = el.find(e1);
      auto i2 = el.find(e2);
      assert(i1 != el.end() && i2 != el.end());
      return root(i1->second) == root(i2->second);
    }

    bool is_dead(const fasttgba_state* e)  {
      auto i = el.find(e);
      assert(i != el.end());
      return root(i->second) == DEAD;
    }

    int nbPart() const {
      int nb = 0;
      int size = (int) id.size();
      // the dead partition is not considered (i = 1)
      for (int i = 1; i < size; ++i)
	if (i == id[i])
	  ++nb;
      return nb;
    }

    int maxDepth() const {
      int max = 0;
      int size = (int) id.size();
      // the dead partition is not considered (i = 1)
      for (int i = 1; i < size; ++i) {
	int d = 0, j = i;
	while (j != id[j]) {
	  ++d;
	  j = id[j];
	}
	if (d > max)
	  max = d;
      }
      return max;
    }

    int maxPart() const {
      int max = 0;
      std::unordered_map<int, int> roots;
      int size = (int) id.size();
      // the dead partition is not considered (i = 1)
      for (int i = 1; i < size; ++i) {
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

    void clear() {
      el.clear();
      id.clear();
      rk.clear();
      id.push_back(DEAD);
      rk.push_back(0);
    }


    virtual void add_acc (const fasttgba_state*, markset)
    {
      assert(false);
    }

    virtual markset get_acc (const fasttgba_state*)
    {
      assert(false);
    }
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_UNION_FIND_HH
