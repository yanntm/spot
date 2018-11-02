#include "common_sys.hh"

#include "treetest.hh"

int generate_states(const std::string filename, int nb_var, int nb_state)
{
  std::srand(std::time(nullptr));
  output_file outf(filename.c_str());

  outf.ostream() << nb_var << std::endl;
  for (int i = 0; i < nb_state; i++)
    {
      int val = std::rand() % MAX_VAL;
      outf.ostream() << val;
      for (int j = 1; j < nb_var; j++)
        {
          val = std::rand() % MAX_VAL;
          outf.ostream() << ',' << val;
        }
      outf.ostream() << std::endl;
    }

  outf.close(filename);
  return 0;
}

std::pair<std::vector<const void*>, time_list>
  file_to_tree(spot::tree_state_manager& tree, std::ifstream& file)
{
  spot::timer t;
  std::vector<clock_t> insert_times;
  std::string line;
  std::vector<const void*> roots;

  while (std::getline(file, line))
    {
      std::string sval;
      std::istringstream s(line);
      int* r = new int[tree.get_state_size()];
      for (size_t i = 0; i < tree.get_state_size(); i++)
        {
          std::getline(s, sval, ',');
          r[i] = std::stoi(sval);
        }

      t.start();
      auto&& res = tree.find_or_put(r, tree.get_state_size());
      if (res.second)
        roots.push_back(res.first);
      t.stop();
      insert_times.push_back(t.utime());
      delete r;
    }

  return make_pair(roots, make_pair(insert_times, t.cutime()));
}

std::pair<std::vector<spot::cspins_state>, time_list>
  file_to_cspins(spot::cspins_state_manager& manager, std::ifstream& file)
{
  spot::timer t;
  std::vector<clock_t> insert_times;
  std::string line;
  cspins_map map;
  std::vector<spot::cspins_state> states;
  int* cmp = new int[manager.size() * 2];

  while (std::getline(file, line))
    {
      std::string sval;
      std::istringstream s(line);
      int* r = new int[manager.size()];
      for (size_t i = 0; i < manager.size(); i++)
        {
          std::getline(s, sval, ',');
          r[i] = std::stoi(sval);
        }

      t.start();
      spot::cspins_state res = manager.alloc_setup(r, cmp, manager.size() * 2);
      auto tmp = map.insert(res);
      bool b = tmp.isnew();
      if (!b)
        manager.dealloc(res);
      else
        states.push_back(res);

      t.stop();
      insert_times.push_back(t.utime());
      delete r;
    }

  delete cmp;
  return make_pair(states, make_pair(insert_times, t.cutime()));
}

void print_tree(spot::tree_state_manager& tree, std::vector<const void*>& roots)
{
  for (auto it = roots.begin(); it != roots.end(); it++)
    {
      int* tmp = tree.get_state(*it);
      for (int i = 2; i < tmp[1] + 2; i++)
        std::cout << tmp[i] << " ";
      std::cout << std::endl;
      delete tmp;
    }
}

void print_normal(spot::cspins_state_manager& manager,
                  std::vector<spot::cspins_state>& states_list)
{
  for (auto it = states_list.begin(); it != states_list.end(); it++)
    {
      int* tmp = new int[manager.size()];
      manager.decompress(*it, tmp, manager.size());
      for (unsigned int i = 0u; i < manager.size(); i++)
        std::cout << tmp[i] << " ";
      std::cout << std::endl;
      delete tmp;
    }
}

void print_time_tree(spot::tree_state_manager& tree,
                     std::vector<const void*>& roots, time_list& tl)
{
  // Shuffle randomly the stored values
  auto rng = std::default_random_engine();
  std::shuffle(roots.begin(), roots.end(), rng);

  spot::timer t;
  for (auto it = roots.begin(); it != roots.end(); it++)
    {
      t.start();
      int* tmp = tree.get_state(*it);
      t.stop();
      delete tmp;
    }

  std::cout << tree.get_state_size() << "," << *(tl.first.rbegin())
            << "," << t.utime() << std::endl;
}

void print_time_normal(spot::cspins_state_manager& manager,
                       std::vector<spot::cspins_state>& states_list,
                       time_list& tl)
{
  // Shuffle randomly the stored values
  auto rng = std::default_random_engine();
  std::shuffle(states_list.begin(), states_list.end(), rng);

  spot::timer t;
  for (auto it = states_list.begin(); it != states_list.end(); it++)
    {
      t.start();
      int* tmp = new int[manager.size()];
      manager.decompress(*it, tmp, manager.size());
      t.stop();
      delete tmp;
    }

  std::cout << manager.size() << "," << *(tl.first.rbegin())
            << "," << t.utime() << std::endl;
}

void free_cspins_states(spot::cspins_state_manager& manager,
                        std::vector<spot::cspins_state>& states_list)
{
  for (auto it = states_list.begin(); it != states_list.end(); it++)
    manager.dealloc(*it);
}

int main (int argc, char* argv[])
{
  if (argc < 2)
    {
      std::cerr << "No parameter given (use --help for manual)" << std::endl;
      return 1;
    }

  std::string arg(argv[1]);
  if (arg == "--help")
    {
      std::cout << "./treetest [OP] [PARAM...]\n"
                << "  OP: --gen FILE NB_VAR NB_STATES:\n"
                << "            generates in a file FILE NB_STATES states\n"
                << "            with NB_VAR variables in each state\n"
                << "      --check-tree FILE:\n"
                << "             Read states from a csv file FILE, store them\n"
                << "             in a tree states manager data structure and\n"
                << "             then display them\n"
                << "      --bench-tree FILE:\n"
                << "             Read states from a csv file FILE, store them\n"
                << "             in a tree states manager data structure and\n"
                << "             then display the number of variables, the\n"
                << "             insertion time and the pick up time\n"
                << "      --check-normal FILE:\n"
                << "             Read states from a csv file FILE, store them\n"
                << "             in a classic data structure and then display\n"
                << "             them\n"
                << "      --bench-normal FILE:\n"
                << "             Read states from a csv file FILE, store them\n"
                << "             in a classic data structure and then display\n"
                << "             the number of variables, the insertion time\n"
                << "             and the pick up time\n"
                << std::endl;
      return 0;
    }
  // Generator of exemples
  else if (arg == "--gen")
    {
      if (argc < 5)
        {
          std::cerr << "Not enough parameters for --gen (--help for manual)"
                    << std::endl;
          return 1;
        }
      return generate_states(argv[2], std::stoi(argv[3]), std::stoi(argv[4]));
    }
  // Tests and bench with the tree_state_manager data structure
  else if (arg == "--check-tree" || arg == "--bench-tree")
    {
      if (argc < 3)
        {
          std::cerr << "File name missing (--help for manual)" << std::endl;
          return 1;
        }

      // Open file
      std::ifstream file;
      std::string line;
      file.open(argv[2]);
      if (!file.is_open())
        {
          std::cerr << argv[2] << " can not be opened" << std::endl;
          return 2;
        }

      // Read the number of variables
      std::getline(file, line);
      size_t size = std::stoi(line);
      spot::tree_state_manager tree(size);
      auto&& res = file_to_tree(tree, file);

      if (arg == "--check-tree")
        print_tree(tree, res.first);
      else
        print_time_tree(tree, res.first, res.second);
      return 0;
    }
  // Tets and bench with the cspins_state_manager data structure
  else if (arg == "--check-normal" || arg == "--bench-normal")
    {
      if (argc < 3)
        {
          std::cerr << "File name missing (--help for manual)" << std::endl;
          return 1;
        }

      // Open file
      std::ifstream file;
      std::string line;
      file.open(argv[2]);
      if (!file.is_open())
        {
          std::cerr << argv[2] << " can not be opened" << std::endl;
          return 2;
        }

      // Read the number of variables
      std::getline(file, line);
      size_t size = std::stoi(line);
      spot::cspins_state_manager manager(size, 1);
      auto&& res = file_to_cspins(manager, file);

      if (arg == "--check-normal")
        print_normal(manager, res.first);
      else
        print_time_normal(manager, res.first, res.second);
      free_cspins_states(manager, res.first);
    }
  else
    {
      std::cerr << "Unknown command (--help for manual)" << std::endl;
      return 1;
    }

  return 0;
}
