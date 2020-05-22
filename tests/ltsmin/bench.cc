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
#include <spot/twaalgos/determinize2.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/powerset.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twacube_algos/convert.hh>
#include <spot/twacube_algos/twacube_determinize.hh>

const char argp_program_doc[] =
"Bench determinization";

struct mc_options_
{
  char* file = nullptr;
  bool use_timer = false;
  unsigned nb_threads = 1;
  unsigned wanted = 0;
  unsigned min = 0;
  unsigned max_states = 0;
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
    case 'm':
      mc_options.min = to_unsigned(arg, "-m/--min");
      break;
    case 'p':
      mc_options.nb_threads = to_unsigned(arg, "-p/--parallel");
      break;
    case 's':
      mc_options.max_states = to_unsigned(arg, "-s/--max-states");
      break;
    case 't':
      mc_options.use_timer = true;
      break;
    case 'w':
      mc_options.wanted = to_unsigned(arg, "-w/--wanted");
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
    // ------------------------------------------------------------
    { nullptr, 0, nullptr, 0, "Output options:", 3 },
    { "wanted", 'w', "INT", 0, "number of auts to bench", 0 },
    { "min", 'm', "INT", 0, "min det time in seconds", 0 },
    { "max-states", 's', "INT", 0, "max num of states before abort", 0 },
    // ------------------------------------------------------------

    { nullptr, 0, nullptr, 0, "General options:", 4 },
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
  size_t count = 0;
  spot::output_aborter* aborter = nullptr;

  int exit_code = 0;

  if (mc_options.max_states != 0)
    {
      aborter = new spot::output_aborter(mc_options.max_states);
    }

  auto parser = spot::automaton_stream_parser("/dev/stdin");

  std::cout << "formula,"
            << "twa_nb_states,"
            << "twa_nb_edges,"
            << "twa_nb_states_deterministic,"
            << "twa_nb_edges_deterministic,"
            << "twacube_nb_states,"
            << "twacube_nb_edges,"
            << "twacube_nb_states_deterministic,"
            << "twacube_nb_edges_deterministic,"
            << "walltime_twa,"
            << "walltime_twacube"
            << std::endl;

  spot::parsed_aut_ptr res = nullptr;
  while ((res = parser.parse(dict))->aut != nullptr)
    {
      auto aut = res->aut;
      SPOT_ASSERT(!res->aborted);
      SPOT_ASSERT(res->errors.empty());
      SPOT_ASSERT(!spot::is_deterministic(aut));

      spot::timer_map tm;

      spot::twa_graph_ptr det_aut = nullptr;
      tm.start("determinize twa");
      {
        det_aut = tgba_determinize2(aut);
      }
      tm.stop("determinize twa");

      spot::twacube_ptr cube_aut = twa_to_twacube(aut);
      spot::twacube_ptr cube_det_aut = nullptr;

      tm.start("determinize twacube");
      {
        cube_det_aut = twacube_determinize(cube_aut);
      }
      tm.stop("determinize twacube");

      auto duration_twa = tm.timer("determinize twa").walltime();
      auto duration_twacube = tm.timer("determinize twacube").walltime();

      if (duration_twacube >= mc_options.min * size_t(1000) && cube_det_aut != nullptr)
        {
          count++;

          auto formula = *aut->get_named_prop<std::string>("automaton-name");

          std::cout << formula << ','
                    << aut->num_states() << ','
                    << aut->num_edges() << ','
                    << det_aut->num_states() << ','
                    << det_aut->num_edges() << ','
                    << cube_aut->num_states() << ','
                    << cube_aut->num_edges() << ','
                    << cube_det_aut->num_states() << ','
                    << cube_det_aut->num_edges() << ','
                    << duration_twa << ','
                    << duration_twacube
                    << std::endl;

          if (mc_options.wanted != 0 && count >= mc_options.wanted)
            return exit_code;
        }

    }

  if (aborter != nullptr)
    delete aborter;

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
