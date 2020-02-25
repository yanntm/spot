#include "config.h"

#include <argp.h>
#include <cassert>

#include "bin/common_setup.hh"
#include "bin/common_conv.hh"

#include <spot/kripke/kripke.hh>
#include <spot/misc/timer.hh>
#include <spot/parseaut/public.hh>
#include <spot/tl/apcollect.hh>
#include <spot/tl/defaultenv.hh>
#include <spot/tl/parse.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twacube_algos/convert.hh>

const char argp_program_doc[] =
"Bench determinization";

struct mc_options_
{
  char* file = nullptr;
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
      mc_options.file = arg;
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
    { "file", 'f', "STRING", 0, "use the automata stored in file STRING", 0 },
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
  auto dict = spot::make_bdd_dict();
  spot::const_twa_graph_ptr aut = nullptr;
  spot::twacube_ptr aut_cube = nullptr;
  spot::formula f = nullptr;
  spot::atomic_prop_set ap;
  spot::timer_map tm;

  int exit_code = 0;

  auto parser = spot::automaton_stream_parser(mc_options.file);

  spot::parsed_aut_ptr res = nullptr;
  while ((res = parser.parse(dict))->aut != nullptr)
    {
      auto aut = res->aut;
      assert(!res->aborted);
      assert(res->errors.empty());

      assert(!spot::is_deterministic(aut));
    }

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
