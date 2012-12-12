// -*- coding: utf-8 -*-
// Copyright (C) 2011, 2012 Laboratoire de Recherche et Développement
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

#include <cstring>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

// MinGW does not define this.
#ifndef WEXITSTATUS
# define WEXITSTATUS(x) ((x) & 0xff)
#endif

#include "dve2.hh"
#include "dve2kripke.hh"


namespace spot
{
  namespace
  {
    struct var_info
    {
      int num;
      int type;
    };

    int
    convert_aps(const ltl::atomic_prop_set* aps,
		const dve2_interface* d,
		bdd_dict* dict,
		const ltl::formula* dead,
		dve2_prop_set& out)
    {
      int errors = 0;

      int state_size = d->get_state_variable_count();
      typedef std::map<std::string, var_info> val_map_t;
      val_map_t val_map;

      for (int i = 0; i < state_size; ++i)
	{
	  const char* name = d->get_state_variable_name(i);
	  int type = d->get_state_variable_type(i);
	  var_info v = { i , type };
	  val_map[name] = v;
	}

      int type_count = d->get_state_variable_type_count();
      typedef std::map<std::string, int> enum_map_t;
      std::vector<enum_map_t> enum_map(type_count);
      for (int i = 0; i < type_count; ++i)
	{
	  int enum_count = d->get_state_variable_type_value_count(i);
	  for (int j = 0; j < enum_count; ++j)
	    enum_map[i]
	      .insert(std::make_pair(d->get_state_variable_type_value(i, j),
				     j));
	}

      for (ltl::atomic_prop_set::const_iterator ap = aps->begin();
	   ap != aps->end(); ++ap)
	{
	  if (*ap == dead)
	    continue;

	  std::string str = (*ap)->name();
	  const char* s = str.c_str();

	  // Skip any leading blank.
	  while (*s && (*s == ' ' || *s == '\t'))
	    ++s;
	  if (!*s)
	    {
	      std::cerr << "Proposition `" << str
			<< "' cannot be parsed." << std::endl;
	      ++errors;
	      continue;
	    }


	  char* name = (char*) malloc(str.size() + 1);
	  char* name_p = name;
	  char* lastdot = 0;
	  while (*s && (*s != '=') && *s != '<' && *s != '!'  && *s != '>')
	    {

	      if (*s == ' ' || *s == '\t')
		++s;
	      else
		{
		  if (*s == '.')
		    lastdot = name_p;
		  *name_p++ = *s++;
		}
	    }
	  *name_p = 0;

	  if (name == name_p)
	    {
	      std::cerr << "Proposition `" << str
			<< "' cannot be parsed." << std::endl;
	      free(name);
	      ++errors;
	      continue;
	    }

	  // Lookup the name
	  val_map_t::const_iterator ni = val_map.find(name);
	  if (ni == val_map.end())
	    {
	      // We may have a name such as X.Y.Z
	      // If it is not a known variable, it might mean
	      // an enumerated variable X.Y with value Z.
	      if (lastdot)
		{
		  *lastdot++ = 0;
		  ni = val_map.find(name);
		}

	      if (ni == val_map.end())
		{
		  std::cerr << "No variable `" << name
			    << "' found in model (for proposition `"
			    << str << "')." << std::endl;
		  free(name);
		  ++errors;
		  continue;
		}

	      // We have found the enumerated variable, and lastdot is
	      // pointing to its expected value.
	      int type_num = ni->second.type;
	      enum_map_t::const_iterator ei = enum_map[type_num].find(lastdot);
	      if (ei == enum_map[type_num].end())
		{
		  std::cerr << "No state `" << lastdot
			    << "' known for variable `"
			    << name << "'." << std::endl;
		  std::cerr << "Possible states are:";
		  for (ei = enum_map[type_num].begin();
		       ei != enum_map[type_num].end(); ++ei)
		    std::cerr << " " << ei->first;
		  std::cerr << std::endl;

		  free(name);
		  ++errors;
		  continue;
		}

	      // At this point, *s should be 0.
	      if (*s)
		{
		  std::cerr << "Trailing garbage `" << s
			    << "' at end of proposition `"
			    << str << "'." << std::endl;
		  free(name);
		  ++errors;
		  continue;
		}

	      // Record that X.Y must be equal to Z.
	      int v = dict->register_proposition(*ap, d);
	      dve2_one_prop p = { ni->second.num, OP_EQ, ei->second, v };
	      out.push_back(p);
	      free(name);
	      continue;
	    }

	  int var_num = ni->second.num;

	  if (!*s)		// No operator?  Assume "!= 0".
	    {
	      int v = dict->register_proposition(*ap, d);
	      dve2_one_prop p = { var_num, OP_NE, 0, v };
	      out.push_back(p);
	      free(name);
	      continue;
	    }

	  dve2_relop op;

	  switch (*s)
	    {
	    case '!':
	      if (s[1] != '=')
		goto report_error;
	      op = OP_NE;
	      s += 2;
	      break;
	    case '=':
	      if (s[1] != '=')
		goto report_error;
	      op = OP_EQ;
	      s += 2;
	      break;
	    case '<':
	      if (s[1] == '=')
		{
		  op = OP_LE;
		  s += 2;
		}
	      else
		{
		  op = OP_LT;
		  ++s;
		}
	      break;
	    case '>':
	      if (s[1] == '=')
		{
		  op = OP_GE;
		  s += 2;
		}
	      else
		{
		  op = OP_GT;
		  ++s;
		}
	      break;
	    default:
	    report_error:
	      std::cerr << "Unexpected `" << s
			<< "' while parsing atomic proposition `" << str
			<< "'." << std::endl;
	      ++errors;
	      free(name);
	      continue;
	    }

	  while (*s && (*s == ' ' || *s == '\t'))
	    ++s;

	  int val = 0; // Initialize to kill a warning from old compilers.
	  int type_num = ni->second.type;
	  if (type_num == 0 || (*s >= '0' && *s <= '9') || *s == '-')
	    {
	      char* s_end;
	      val = strtol(s, &s_end, 10);
	      if (s == s_end)
		{
		  std::cerr << "Failed to parse `" << s
			    << "' as an integer." << std::endl;
		  ++errors;
		  free(name);
		  continue;
		}
	      s = s_end;
	    }
	  else
	    {
	      // We are in a case such as P_0 == S, trying to convert
	      // the string S into an integer.
	      const char* end = s;
	      while (*end && *end != ' ' && *end != '\t')
		++end;
	      std::string st(s, end);

	      // Lookup the string.
	      enum_map_t::const_iterator ei = enum_map[type_num].find(st);
	      if (ei == enum_map[type_num].end())
		{
		  std::cerr << "No state `" << st
			    << "' known for variable `"
			    << name << "'." << std::endl;
		  std::cerr << "Possible states are:";
		  for (ei = enum_map[type_num].begin();
		       ei != enum_map[type_num].end(); ++ei)
		    std::cerr << " " << ei->first;
		  std::cerr << std::endl;

		  free(name);
		  ++errors;
		  continue;
		}
	      s = end;
	      val = ei->second;
	    }

	  free(name);

	  while (*s && (*s == ' ' || *s == '\t'))
	    ++s;
	  if (*s)
	    {
	      std::cerr << "Unexpected `" << s
			<< "' while parsing atomic proposition `" << str
			<< "'." << std::endl;
	      ++errors;
	      continue;
	    }


	  int v = dict->register_proposition(*ap, d);
	  dve2_one_prop p = { var_num, op, val, v };
	  out.push_back(p);
	}

      return errors;
    }


    // Call divine to compile "foo.dve" as "foo.dve2C" if the later
    // does not exist already or is older.
    bool
    compile_dve2(std::string& filename, bool verbose)
    {

      std::string command = "divine compile --ltsmin " + filename;

      struct stat s;
      if (stat(filename.c_str(), &s) != 0)
	{
	  if (verbose)
	    {
	      std::cerr << "Cannot open " << filename << std::endl;
	      return true;
	    }
	}

      std::string old = filename;
      filename += "2C";

      // Remove any directory, because the new file will
      // be compiled in the current directory.
      size_t pos = filename.find_last_of("/\\");
      if (pos != std::string::npos)
	filename = "./" + filename.substr(pos + 1);

      struct stat d;
      if (stat(filename.c_str(), &d) == 0)
	if (s.st_mtime < d.st_mtime)
	  // The dve2C is up-to-date, no need to recompile it.
	  return false;

      int res = system(command.c_str());
      if (res)
	{
	  if (verbose)
	    std::cerr << "Execution of `" << command.c_str()
		      << "' returned exit code " << WEXITSTATUS(res)
		      << "." << std::endl;
	  return true;
	}
      return false;
    }
  }

  kripke*
  load_dve2(const std::string& file_arg, bdd_dict* dict,
	    const ltl::atomic_prop_set* to_observe,
	    const ltl::formula* dead,
	    int compress,
	    bool verbose,
    	    bool por,
	    bool ample)
  {
    std::string file;
    if (file_arg.find_first_of("/\\") != std::string::npos)
      file = file_arg;
    else
      file = "./" + file_arg;

    std::string ext = file.substr(file.find_last_of("."));
    if (ext == ".dve")
      {
	if (compile_dve2(file, verbose))
	  {
	    if (verbose)
	      std::cerr << "Failed to compile `" << file_arg
			<< "'." << std::endl;
	    return 0;
	  }
      }
    else if (ext != ".dve2C")
      {
	if (verbose)
	  std::cerr << "Unknown extension `" << ext
		    << "'.  Use `.dve' or `.dve2C'." << std::endl;
	return 0;
      }

    if (lt_dlinit())
      {
	if (verbose)
	  std::cerr << "Failed to initialize libltdl." << std::endl;
	return 0;
      }

    lt_dlhandle h = lt_dlopen(file.c_str());
    if (!h)
      {
	if (verbose)
	  std::cerr << "Failed to load `" << file << "'." << std::endl;
	lt_dlexit();
	return 0;
      }

    dve2_interface* d = new dve2_interface;
    d->handle = h;

    d->get_initial_state = (void (*)(void*))
      lt_dlsym(h, "get_initial_state");
    d->have_property = (int (*)())
      lt_dlsym(h, "have_property");
    d->get_successors = (int (*)(void*, int*, TransitionCB, void*))
      lt_dlsym(h, "get_successors");
    d->get_state_variable_count = (int (*)())
      lt_dlsym(h, "get_state_variable_count");
    d->get_state_variable_name = (const char* (*)(int))
      lt_dlsym(h, "get_state_variable_name");
    d->get_state_variable_type = (int (*)(int))
      lt_dlsym(h, "get_state_variable_type");
    d->get_state_variable_type_count = (int (*)())
      lt_dlsym(h, "get_state_variable_type_count");
    d->get_state_variable_type_name = (const char* (*)(int))
      lt_dlsym(h, "get_state_variable_type_name");
    d->get_state_variable_type_value_count = (int (*)(int))
      lt_dlsym(h, "get_state_variable_type_value_count");
    d->get_state_variable_type_value = (const char* (*)(int, int))
      lt_dlsym(h, "get_state_variable_type_value");
    d->get_transition_count = (int (*)())
      lt_dlsym(h, "get_transition_count");
    d->get_transition_read_dependencies = (const int* (*) (int))
      lt_dlsym(h, "get_transition_read_dependencies");

    if (!(d->get_initial_state
	  && d->have_property
	  && d->get_successors
	  && d->get_state_variable_count
	  && d->get_state_variable_name
	  && d->get_state_variable_type
	  && d->get_state_variable_type_count
	  && d->get_state_variable_type_name
	  && d->get_state_variable_type_value_count
	  && d->get_state_variable_type_value
	  && d->get_transition_count
	  && d->get_transition_read_dependencies))
      {
	if (verbose)
	  std::cerr << "Failed to resolve some symbol while loading `"
		    << file << "'" << std::endl;
	delete d;
	lt_dlexit();
	return 0;
      }

    if (d->have_property())
      {
	if (verbose)
	  std::cerr << "Model with an embedded property are not supported."
		    << std::endl;
	delete d;
	lt_dlexit();
	return 0;
      }

    dve2_prop_set* ps = new dve2_prop_set;
    int errors = convert_aps(to_observe, d, dict, dead, *ps);
    if (errors)
      {
	delete ps;
	dict->unregister_all_my_variables(d);
	delete d;
	lt_dlexit();
	return 0;
      }

    return new dve2_kripke(d, dict, ps, dead, compress, por, ample);
  }
}
