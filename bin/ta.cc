// -*- coding: utf-8 -*-
// Copyright (C) 2017 Laboratoire de Recherche et Développement de l'Epita.
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

#include "common_sys.hh"

#include <ltdl.h>

#include <spot/zgastwa/zgastwa.hh>
#include <spot/twaalgos/dot.hh>

#include <utap2tchk-predef.hh>
#include <base/labelset.hh>
#include <semantics/semantics.hh>
#include <semantics/zg.hh>
#include <syntax/model.hh>
#include <syntax/system_builder.hh>

#define STRINGIFY(s) #s
#define TO_STRING(s) STRINGIFY(s)

using namespace utap2tchk;

std::string xml2cc(const std::string& filename)
{
  std::string file;
  size_t pos = filename.find_last_of("/\\");
  if (pos != std::string::npos)
    file = "./" + filename.substr(pos + 1);
  else
    file = filename;

  file = file.substr(0, file.find_last_of(".")) + ".cc";

  std::string tchk_path(TO_STRING(TCHECKER_PATH));
  std::string command = tchk_path + "/utap2tchk " + filename
                        + " > " + file;
  int res = system(command.c_str());
  if (res)
    throw std::runtime_error(std::string("Execution of '")
                             + command.c_str() + "' returned exit code "
                             + std::to_string(WEXITSTATUS(res)));
  return file;
}

std::string cc2so(const std::string& filename)
{
  std::string file;
  size_t pos = filename.find_last_of("/\\");
  if (pos != std::string::npos)
    file = "./" + filename.substr(pos + 1);
  else
    file = filename;

  file = file.substr(0, file.find_last_of(".")) + ".so";

  std::string tchk_path(TO_STRING(TCHECKER_PATH));
  std::string udbm_path(TO_STRING(UDBM_PATH));
  std::string flags = "-I" + tchk_path + "/src/include"
                      + " -I" + udbm_path + "/include";
//                      + " -L" + tchk_path + "/src"
//                      + " -L" + udbm_path + "/lib"
//                      + " -Wl,--whole-archive -ludbm -Wl,--no-whole-archive";
  std::string command = "g++ -std=c++11 -g -shared -fPIC " + flags + " -o "
                        + file + " " + filename;

  int res = system(command.c_str());
  if (res)
    throw std::runtime_error(std::string("Execution of '")
                             + command.c_str() + "' returned exit code "
                             + std::to_string(WEXITSTATUS(res)));

  return file;
}

syntax::extern_system_builder_t load(const std::string& filename,
                                     lt_dlhandle& h)
{
  std::string file = filename;
  std::string ext = file.substr(file.find_last_of("."));
  if (ext == ".xml")
    {
      file = xml2cc(filename);
      ext = file.substr(file.find_last_of("."));
    }

  if (ext == ".cc")
    {
      file = cc2so(file);
      ext = file.substr(file.find_last_of("."));
    }
  file = "./" + file;

  h = lt_dlopen(file.c_str());
  if (!h)
    {
      std::string lt_error = lt_dlerror();
      lt_dlexit();
      throw std::runtime_error(std::string("Failed to load '")
                               + file + "'.\n" + lt_error);
    }

  auto sym = [&](const char* name)
    {
      void (*res)(void*);
      *reinterpret_cast<void**>(&res) = lt_dlsym(h, name);
      if (res == nullptr)
        {
          std::string lt_error = lt_dlerror();
          lt_dlclose(h);
          lt_dlexit();
          throw std::runtime_error(std::string("Failed to resolve symbol '")
                                   + name + "' in '" + file + "': " + lt_error);
        }

      return res;
    };

  void (*build_model)(syntax::system_t&) = (void (*)(syntax::system_t&))
                                     sym("_Z11build_modelRN6syntax8system_tE");
  syntax::extern_system_builder_t tchecker_system_builder(build_model);

  return tchecker_system_builder;
}

int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    std::cerr << "Need a file with timed automata in parameter" << std::endl;
    return 1;
  }

  labels_t label;
  lt_dlhandle h;

  if (lt_dlinit())
    throw std::runtime_error("Failed to initialize libltdl.");

  auto sys_builder = load(argv[1], h);
  syntax::model_t* model =
       syntax::model_factory_t::instance().new_model(sys_builder, label);
  auto a = std::make_shared<spot::zg_as_twa>(spot::make_bdd_dict(), *model);
  spot::print_dot(std::cout, a);

  if (!h)
    lt_dlclose(h);
  lt_dlexit();
  return 0;
}
