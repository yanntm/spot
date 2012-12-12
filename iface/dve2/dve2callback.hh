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

#ifndef SPOT_IFACE_DVE2_DVE2CALLBACK_HH
# define SPOT_IFACE_DVE2_DVE2CALLBACK_HH

# include "dve2.hh"

namespace spot
{
  struct dve2_callback_context
  {
    typedef std::list<state*> transitions_t;
    transitions_t transitions;
    int state_size;
    void* pool;
    int* compressed;
    void (*compress)(const int*, size_t, int*, size_t&);
    dve2_transition_info_t* trans_info;

    ~dve2_callback_context();
  };

  void transition_callback(void* arg, dve2_transition_info_t* i, int *dst);
  void transition_callback_compress(void* arg, dve2_transition_info_t*,
				    int *dst);

  ////////////////////////////////////////////////////////////////////////
  // POR iterator

  struct por_callback
  {
    struct trans
    {
      trans(int i, int* d);

      int id;
      int* dst;
    };

    por_callback(unsigned ss);

    void clear();

    std::list<trans> tr;
    unsigned state_size;
  };

  void fill_trans_callback(void* arg, dve2_transition_info_t* info, int* dst);
}

#endif // SPOT_IFACE_DVE2_DVE2CALLBACK_HH
