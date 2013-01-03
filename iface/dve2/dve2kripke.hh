// Copyright (C) 2012 Laboratoire de Recherche et Developpement
// de l'Epita (LRDE)
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

#ifndef SPOT_IFACE_DVE2_DVE2KRIPKE_HH
# define SPOT_IFACE_DVE2_DVE2KRIPKE_HH

# include "dve2.hh"

# include "misc/fixpool.hh"
# include "misc/mspool.hh"

# include "dve2callback.hh"

#include "misc/hashfunc.hh"

namespace spot
{
  class dve2_twophase;
  class ample_iterator;

  class dve2_kripke: public kripke
  {
  private:
    friend class dve2_twophase;
    friend class ample_iterator;

  public:

    dve2_kripke(const dve2_interface* d, bdd_dict* dict,
		const dve2_prop_set* ps, const ltl::formula* dead,
		int compress, por::type por = por::NONE);

    ~dve2_kripke();

    virtual state* get_init_state() const;

    bdd compute_state_condition_aux(const int* vars) const;

    dve2_callback_context* build_cc(const int* vars, int& t) const;

    bdd compute_state_condition(const state* st) const;

    const int* get_vars(const state* st) const;
    virtual kripke_succ_iterator* succ_iter(const state* local_state,
					    const state* prod_state= 0,
					    const tgba* prod_tgba= 0,
					    const por_info* po = 0) const;

    virtual bdd state_condition(const state* st) const;

    virtual std::string format_state(const state *st) const;
    virtual spot::bdd_dict* get_dict() const;

    bool invisible(const int* start, const int* to,
		   bdd form_vars = bddfalse) const;
    unsigned hash_state(const int* in) const;

  private:
    const dve2_interface* d_;
    int state_size_;
    bdd_dict* dict_;
    const char** vname_;
    bool* format_filter_;
    const dve2_prop_set* ps_;
    bdd alive_prop;
    bdd dead_prop;
    void (*compress_)(const int*, size_t, int*, size_t&);
    void (*decompress_)(const int*, size_t, int*, size_t);
    int* uncompressed_;
    int* compressed_;
    fixed_size_pool statepool_;
    multiple_size_pool compstatepool_;

    // This cache is used to speedup repeated calls to state_condition()
    // and get_succ().
    // If state_condition_last_state_ != 0, then state_condition_last_cond_
    // contain its (recently computed) condition.  If additionally
    // state_condition_last_cc_ != 0, then it contains the successors.
    mutable const state* state_condition_last_state_;
    mutable bdd state_condition_last_cond_;
    mutable dve2_callback_context* state_condition_last_cc_;
    por::type por_;
    mutable std::set<unsigned> visited_;
    unsigned cur_process_;
    std::vector<int> processes_;
    std::vector<int> global_vars_;
  };
}
#endif // SPOT_IFACE_DVE2_DVE2KRIPKE_HH
