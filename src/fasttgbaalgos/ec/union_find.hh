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
#include <iosfwd>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include "deadstore.hh"

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

    /// \brief The color for a new State
    enum color {Alive, Dead, Unknown};

    virtual color get_color(const fasttgba_state*);

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
    std::vector<int> idneg;

    /// \brief Acceptance associated to each element
    std::vector<markset *> accp;

    /// \brief The acceptance dictionary
    acc_dict& acc_;

    /// Avoid to re-create some elements
    markset empty;
  };

  class setOfDisjointSetsIPC_LRPC: public union_find
  {
  private:
    Sgi::hash_map<const fasttgba_state*, int,
		  fasttgba_state_ptr_hash,
		  fasttgba_state_ptr_equal> el;
    mutable std::vector<int> id;
    mutable std::vector<int> rk;
    // id of a specially managed partition of "dead" elements
    const int DEAD = 0;

    virtual int root(int i);

  public:
    setOfDisjointSetsIPC_LRPC(acc_dict& acc);

    virtual bool add(const fasttgba_state* e);

    virtual void unite(const fasttgba_state* e1, const fasttgba_state* e2);

    virtual void make_dead(const fasttgba_state* e);

    virtual bool contains(const fasttgba_state* e);

    virtual bool same_partition(const fasttgba_state* e1,
				const fasttgba_state* e2);

    virtual bool is_dead(const fasttgba_state* e);

    int nbPart() const;

    int maxDepth() const;

    int maxPart() const;

    void clear();

    virtual void add_acc (const fasttgba_state*, markset);

    virtual markset get_acc (const fasttgba_state*);

    virtual color get_color(const fasttgba_state*);
  };

  class setOfDisjointSetsIPC_LRPC_MS : public union_find
  {
  private:
    Sgi::hash_map<const fasttgba_state*, int,
		  fasttgba_state_ptr_hash, fasttgba_state_ptr_equal> el;
    mutable std::vector<int> id;
    // id of a specially managed partition of "dead" elements
    const int DEAD = 0;

    virtual int root(int i);

  public:
    setOfDisjointSetsIPC_LRPC_MS(acc_dict& acc);

    virtual bool add(const fasttgba_state* e);

    virtual void unite(const fasttgba_state* e1, const fasttgba_state* e2);

    virtual void make_dead(const fasttgba_state* e);

    virtual bool contains(const fasttgba_state* e);

    virtual bool same_partition(const fasttgba_state* e1,
				const fasttgba_state* e2);

    virtual bool is_dead(const fasttgba_state* e);

    int nbPart() const;

    int maxDepth() const;

    int maxPart() const;

    void clear();

    virtual void add_acc (const fasttgba_state*, markset);

    virtual markset get_acc (const fasttgba_state*);

    virtual color get_color(const fasttgba_state*);
  };

  /// \brief this class propose an union find based on a deadstore.
  /// The idea is to used a dedicated map for the storage of dead
  /// state but this storage is done in a lazy way. A counter maintain
  /// the realsize of the union find structure when a make dead is
  /// realized : this is possible with the assumption that make dead
  /// is performed on the dfs root of the SCC. So the structure maintain
  /// the real size of the structure which is the size of Alive states.
  /// Adding a new state only checks if this size is the size of the
  /// structure to avoid extra memory allocation. If it is not true
  /// this means that there are dead states insides the alive structure.
  /// Just pick one a insert it inside the dead store. Otherwise it's a
  /// classic insert.
  ///
  /// Note that this class doesn't support contains since get_color
  /// is the most efficient way to check wheter a state is available
  class setOfDisjointSetsIPC_LRPC_MS_Dead : public union_find
  {
  private:
    typedef Sgi::hash_map<const fasttgba_state*, int,
			  fasttgba_state_ptr_hash,
			  fasttgba_state_ptr_equal> seen_map;
    seen_map el;

    // This stucture allows to bind a position with a state
    struct idpair
    {
      int id;
      const fasttgba_state* state;
    };

    mutable std::vector<idpair> id;
    // id of a specially managed partition of "dead" elements
    const int DEAD = 0;

    deadstore* deadstore_;
    int realsize_;

    virtual int root(int i);

  public:
    setOfDisjointSetsIPC_LRPC_MS_Dead(acc_dict& acc);

    virtual bool add(const fasttgba_state* e);

    virtual void unite(const fasttgba_state* e1, const fasttgba_state* e2);

    virtual void make_dead(const fasttgba_state* e);

    virtual bool contains(const fasttgba_state* e);

    virtual bool same_partition(const fasttgba_state* e1,
				const fasttgba_state* e2);

    virtual bool is_dead(const fasttgba_state* e);

    int nbPart() const;

    int maxDepth() const;

    int maxPart() const;

    void clear();

    virtual void add_acc (const fasttgba_state*, markset);

    virtual markset get_acc (const fasttgba_state*);

    virtual color get_color(const fasttgba_state*);
  };
}

#endif // SPOT_FASTTGBAALGOS_EC_UNION_FIND_HH
