#pragma once

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>

#include "common_file.hh"
#include "spot/bricks/brick-hashset"
#include "spot/ltsmin/tree_state.hh"
#include "spot/ltsmin/spins_kripke.hh"
#include "spot/misc/timer.hh"

#define MAX_VAL 1000

using time_list = std::pair<std::vector<clock_t>, clock_t>;
using tree_list = std::pair<spot::tree_state_manager, std::vector<const void*>>;
struct cspins_test_hasher
{
  cspins_test_hasher(int*) { }
  cspins_test_hasher() = default;

  brick::hash::hash128_t hash(int* n) const
  {
    // Not modulo 31 according to brick::hashset specifications, and unsigned.
   unsigned m = (unsigned)n[0] % (1<<30);
    return {m, m};
  }

  bool equal(int* a, int* b) const
  {
    bool res = a[1] == b[1];
    int i = 0;
    for (i = 0; res && i < a[1]; i++)
      res = a[i + 2] == b[i + 2];
    return res;
  }
};

using cspins_map = brick::hashset::FastConcurrent <int*,
                                                   cspins_test_hasher>;

/// \brief Generate files contaning states
int generate_states(const std::string filename, int nb_var, int nb_state);

/// \brief Read a file and store states into a tree
std::pair<std::vector<const void*>, time_list>
  file_to_tree(spot::tree_state_manager& tree, std::ifstream& file);

/// \brief Read a file and store states with cspins_state_manager
std::pair<std::vector<spot::cspins_state>, time_list>
  file_to_cspins(spot::cspins_state_manager& manager, std::ifstream& file);

/// \brief Free the memory used by cspins_state_manager
void free_cspins_states(spot::cspins_state_manager& manager,
                        std::vector<spot::cspins_state>& states_list);

/// \brief Print functions
void print_tree(spot::tree_state_manager& tree,
                std::vector<const void*>& roots);
void print_normal(spot::cspins_state_manager& manager,
                  std::vector<spot::cspins_state>& states_list);
void print_time_tree(spot::tree_state_manager& tree,
                     std::vector<const void*>& roots, time_list& tl);
void print_time_normal(spot::cspins_state_manager& manager,
                       std::vector<spot::cspins_state>& states_list,
                       time_list& tl);
