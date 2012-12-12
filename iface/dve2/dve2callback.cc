// -*- coding: utf-8 -*-
// Copyright (C) 2012 Laboratoire de Recherche et Développement
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

#include "dve2callback.hh"
#include "dve2state.hh"

#include "misc/fixpool.hh"
#include "misc/mspool.hh"

#include <cstring>

namespace spot
{
  dve2_callback_context::~dve2_callback_context()
  {
    transitions_t::const_iterator it;
    for (it = transitions.begin(); it != transitions.end(); ++it)
      (*it)->destroy();
  }


  void
  transition_callback(void* arg, dve2_transition_info_t* i, int *dst)
  {
    dve2_callback_context* ctx = static_cast<dve2_callback_context*>(arg);
    fixed_size_pool* p = static_cast<fixed_size_pool*>(ctx->pool);
    dve2_state* out =
      new(p->allocate()) dve2_state(ctx->state_size, p);
    memcpy(out->vars, dst, ctx->state_size * sizeof(int));
    out->compute_hash();
    ctx->transitions.push_back(out);

    ctx->trans_info = i;
  }

  void
  transition_callback_compress(void* arg, dve2_transition_info_t*, int *dst)
  {
    dve2_callback_context* ctx = static_cast<dve2_callback_context*>(arg);
    multiple_size_pool* p = static_cast<multiple_size_pool*>(ctx->pool);

    size_t csize = ctx->state_size * 2;
    ctx->compress(dst, ctx->state_size, ctx->compressed, csize);

    void* mem = p->allocate(sizeof(dve2_compressed_state)
			    + sizeof(int) * csize);
    dve2_compressed_state* out = new(mem) dve2_compressed_state(csize, p);
    memcpy(out->vars, ctx->compressed, csize * sizeof(int));
    out->compute_hash();
    ctx->transitions.push_back(out);
  }

  ////////////////////////////////////////////////////////////////////////
  // POR iterator

  por_callback::trans::trans(int i, int* d)
    : id(i)
      , dst(d)
  {
  }

  por_callback::por_callback(unsigned ss)
    : tr()
      , state_size(ss)
  {
  }

  void
  por_callback::clear()
  {
    for (std::list<trans>::iterator it = tr.begin();
	 it != tr.end();)
      {
	int* st = it->dst;
	++it;
	delete[] st;
      }

    tr.clear();
  }

  void
  fill_trans_callback(void* arg, dve2_transition_info_t* info, int* dst)
  {
    assert(arg);

    por_callback* pc = static_cast<por_callback*>(arg);

    int* tmp = new int[4 * sizeof(int)];
    memcpy(tmp, dst, 4 * sizeof(int));

    pc->tr.push_back(por_callback::trans(info->group, tmp));
  }
}
