#include "config.h"

#include <argp.h>

#include "bin/common_setup.hh"
#include "bin/common_conv.hh"

#include <spot/kripke/kripke.hh>
#include <spot/misc/timer.hh>
#include <spot/tl/apcollect.hh>
#include <spot/tl/defaultenv.hh>
#include <spot/tl/parse.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twacube_algos/convert.hh>

const char argp_program_doc[] =
"Bench determinization";

struct mc_options_
{
  char* formula = nullptr;
  char* model = nullptr;
  bool use_timer = false;
  unsigned nb_threads = 1;
} mc_options;

static int
parse_opt_finput(int key, char* arg, struct argp_state*)
{
  // This switch is alphabetically-ordered.
  switch (key)
    {
    case 'f':
      mc_options.formula = arg;
      break;
    case 'm':
      mc_options.model = arg;
      break;
    case 'p':
      mc_options.nb_threads = to_unsigned(arg, "-p/--parallel");
      break;
    case 't':
      mc_options.use_timer = true;
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static const argp_option options[] =
  {
    // Keep each section sorted
    // ------------------------------------------------------------
    { nullptr, 0, nullptr, 0, "Input options:", 1 },
    { "formula", 'f', "STRING", 0, "use the formula STRING", 0 },
    // FIXME do we want support for reading more than one formula?
    { "model", 'm', "STRING", 0, "use  the model stored in file STRING", 0 },
    // ------------------------------------------------------------
    { nullptr, 0, nullptr, 0, "Process options:", 2 },
    { "parallel", 'p', "INT", 0, "use INT threads (when possible)", 0 },
    { "timer", 't', nullptr, 0,
      "time the different phases of the execution", 0 },

    { nullptr, 0, nullptr, 0, "General options:", 3 },
    { nullptr, 0, nullptr, 0, nullptr, 0 }
  };

const struct argp finput_argp = { options, parse_opt_finput,
                                  nullptr, nullptr, nullptr,
                                  nullptr, nullptr };

const struct argp_child children[] =
  {
    { &finput_argp, 0, nullptr, 1 },
    { &misc_argp, 0, nullptr, -1 },
    { nullptr, 0, nullptr, 0 }
  };

static int
checked_main()
{
  auto& env = spot::default_environment::instance();

  auto dict = spot::make_bdd_dict();
  spot::const_twa_graph_ptr aut = nullptr;
  spot::twacube_ptr aut_cube = nullptr;
  spot::formula f = nullptr;
  spot::atomic_prop_set ap;
  spot::timer_map tm;

  int exit_code = 0;

  if (mc_options.formula == nullptr)
    {
      std::cerr << "Please provide a formula to determinize\n";
      return 1;
    }

  tm.start("parsing formula");
  {
    auto pf = spot::parse_infix_psl(mc_options.formula, env, false);
    exit_code = pf.format_errors(std::cerr);
    f = pf.f;
  }
  tm.stop("parsing formula");

  std::cout << "parsed formula" << std::endl;

  tm.start("translating formula");
  {
    spot::translator trans(dict);
    trans.set_level(spot::postprocessor::optimization_level::Low);
    // if (deterministic) FIXME ???
    //   trans.set_pref(spot::postprocessor::Deterministic);
    aut = trans.run(&f);
  }
  tm.stop("translating formula");

  std::cout << "translated formula" << std::endl;
  std::cout << "twa states: " << aut->num_states() << '\n';

  tm.start("translating to/from twacube");
  {
    aut_cube = spot::twa_to_twacube(aut);
    aut = spot::twacube_to_twa(aut_cube);
  }
  tm.stop("translating to/from twacube");

  // FIXME: ???
  atomic_prop_collect(f, &ap);

  std::cout << "twa states: " << aut->num_states() << '\n';
  std::cout << "twacube states: " << aut_cube->num_states() << '\n';

  return exit_code;
}

int
main(int argc, char** argv)
{
  setup(argv);
  const argp ap = { nullptr, nullptr, nullptr,
                    argp_program_doc, children, nullptr, nullptr };

  if (int err = argp_parse(&ap, argc, argv, ARGP_NO_HELP, nullptr, nullptr))
    exit(err);

  auto exit_code = checked_main();

  return exit_code;
}
