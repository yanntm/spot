#include "config.h"
#include "bin/common_conv.hh"
#include "bin/common_setup.hh"
#include "bin/common_output.hh"

#include <iostream>

#include <spot/ltsmin/ltsmin.hh>
#include <spot/ltsmin/spins_kripke.hh>
#include <spot/mc/mc.hh>
#include <spot/twaalgos/dot.hh>
#include <spot/tl/defaultenv.hh>
#include <spot/tl/parse.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twaalgos/emptiness.hh>
#include <spot/twaalgos/postproc.hh>
#include <spot/twaalgos/are_isomorphic.hh>
#include <spot/twa/twaproduct.hh>
#include <spot/misc/timer.hh>
#include <spot/misc/memusage.hh>
#include <cstring>
#include <spot/kripke/kripkegraph.hh>
#include <spot/twaalgos/hoa.hh>

#include <thread>
#include <spot/twacube/twacube.hh>
#include <spot/twacube_algos/convert.hh>
#include <spot/mc/ec.hh>

#include <spot/mc/mpi/mc_mpi.hh>

int main(int argc, char** argv)
{
  if (argc != 3)
  {
    std::cout << "usage: FORMULA MODEL" << std::endl;
    return 2;
  }
  int exit_code = 0;
  spot::default_environment& env = spot::default_environment::instance();
  spot::timer_map tm;
  spot::formula f = nullptr;
  spot::formula deadf = spot::formula::tt();
  spot::const_twa_graph_ptr prop = nullptr;
  auto dict = spot::make_bdd_dict();

  std::cout << "parsing formula" << std::endl;
  tm.start("parsing formula");
  {
    auto pf = spot::parse_infix_psl(argv[1], env, false);
    exit_code = pf.format_errors(std::cerr);
    f = pf.f;
  }
  tm.stop("parsing formula");

  std::cout << "translating formula" << std::endl;
  tm.start("translating formula");
  {
    spot::translator trans(dict);
    prop = trans.run(&f);
  }
  tm.stop("translating formula");

  std::cout << "twa to twacube" << std::endl;
  tm.start("twa to twacube");
  auto propcube = spot::twa_to_twacube(prop);
  tm.stop("twa to twacube");

  std::cout << "load kripkecube" << std::endl;
  tm.start("load kripkecube");
  spot::ltsmin_kripkecube_ptr modelcube = nullptr;
  try
  {
    modelcube = spot::ltsmin_model::load(argv[2])
      .kripkecube(propcube->get_ap(), deadf, 0, 1);
  }
  catch (const std::runtime_error& e)
  {
    std::cerr << e.what() << '\n';
  }
  tm.stop("load kripkecube");

  std::cout << "dfs_cep" << std::endl;
  tm.start("dfs_cep");
  auto res = spot::distribute_dfs<spot::ltsmin_kripkecube_ptr,
                                  spot::cspins_state,
                                  spot::cspins_iterator,
                                  spot::cspins_state_hash>(modelcube);
  tm.stop("dfs_cep");

  tm.reset_all();
  return exit_code;
}
