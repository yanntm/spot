// -*- coding: utf-8 -*-
// Copyright (C) 2015, 2016, 2017 Laboratoire de Recherche et Développement de
// l'Epita (LRDE).
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



#include <spot/tl/sim/tree.hh>


void rm_spaces(std::string& str)
{
  while (str[0] == ' ')
    str.erase(0, 1);
  int max = str.size() - 1;
  while (str[max] == ' ')
    str.pop_back();
}

std::ostream& operator<<(std::ostream& os, builtin& f)
{
  os << f.name_get() << '(' << f.arg_no(0);
  for (int i = 1; i < f.size(); i++)
    os << ", " << f.arg_no(i);
  os << ')';
  return os;
}

int error(unsigned line, std::string message)
{
  std::cerr << "Error: line " << line << ": " << message << '.' << std::endl;
  return false;
}

std::vector<std::string> input_divide(std::string filename)
{
  std::ifstream ifs(filename);
  std::vector<std::string> vect;
  std::string str;
  std::string save;
  while (std::getline(ifs, str))
  {
    if (!save.empty())
      str = save + str;
    auto size = str.size();
    if (size)
    {
      if (str[size - 1] == '\\')
        save = str.substr(0, size - 1);
      else if (size > 2 && str[0] == '/' && str[1] == '/')
      {
        save = "";
        continue;
      }
      else
      {
        vect.push_back(str);
        save = "";
      }
    }
  }
  return vect;
}

bool input_check(int i, std::string str, sim_line& sl)
{
  auto split_delim = [](const std::string str, const std::string& delim,
      std::string& left, std::string& right) -> bool
  {
    auto pos = str.find(delim);
    if (pos != std::string::npos)
    {
      left = str.substr(0, pos);
      right = str.substr(pos + delim.length());
      rm_spaces(left);
      rm_spaces(right);
      return true;
    }
    return false;
  };
  std::string cond;
  split_delim(str, " IF ", str, cond);
  std::string ret;
  if (!split_delim(str, "=", str, ret))
    return error(i, "missing =");
  sl = sim_line(cond, spot::parse_formula(str), ret);
  //std::cout << sl.formula_get() << std::endl;
  return true;
}

bool simp(sim_tree& t, spot::formula f, spot::formula& res,
    str_vect* pos = nullptr, str_vect* neg = nullptr,
    str_map* ap_asso = nullptr)
{
  bool b = false;
  std::function<spot::formula(spot::formula)> simplify;
  simplify = [&](spot::formula f) -> spot::formula
  {
    if (ap_asso && t.apply(f, res, *ap_asso) || (!ap_asso && t.apply(f, res)))
    {
      b = true;
      spot::formula rec;
      if (!pos && simp(t, res, rec))
      {
        if (pos)
          pos->push_back(str_psl(f));
        std::cout << f << " reduced to " << rec << std::endl;
        return rec;
      }
      if (pos)
        pos->push_back(str_psl(f));
      std::cout << f << " reduced to " << res << std::endl;
      return res;
    }
    if (neg)
      neg->push_back(str_psl(f));
    if (pos)
      return f;
    return f.map(simplify);
  };
  res = simplify(f);
  return b;
}

bool has(spot::formula& f, spot::formula& pattern, str_vect& pos,
    str_vect& neg, str_map& map)
{
  std::vector<std::tuple<std::string, std::string, std::string>> chg;
  spot::formula ptrn = pattern;
  // might be useless or overkill
  pattern.traverse([&chg](spot::formula f)
  {
    if (is_nary(f))
    {
      if (f.size() == 2)
        for (int i = 0; i < 2; i++)
          if (f[i].kind() == spot::op::ap && f[i].ap_name() == "...")
            chg.push_back(std::make_tuple(f[0].ap_name() + str(f.kind())
                  + f[1].ap_name(), f[(i + 1) % 2].ap_name(), str(f.kind())));
    }
    bool ret = f.size();
    return !ret;
  });
  sim_tree t;
  std::string nary_name;
  str_map ap_asso = map;
  if (!chg.empty())
  {
    std::string ret = str_psl(pattern);
    std::string cond;
    // vector might be overkill? doin 2 distinct functions ?
    for (auto tuple : chg)
    {
      nary_name = std::get<1>(tuple);
      auto find = std::get<0>(tuple);
      auto arg = "(" + std::get<1>(tuple) + ", f)";
      std::string rep = "splitnot" + arg + std::get<2>(tuple) + "split" + arg;
      ret.replace(ret.find(find), find.size(), rep);
      cond += cond.size() ? " and @" + arg : "@" + arg;
    }
    t.insert(sim_line(cond, pattern, ret), ap_asso);
  }
  else
    t.insert(sim_line("", ptrn, str_psl(pattern)), ap_asso);
  t.to_dot("subtree.dot");
  bool res = false;
  str_map all;
  for (unsigned i = 0; i < f.size(); i++)
  {
    spot::formula ref;
    str_map map = ap_asso;
    res |= simp(t, f[i], ref, &pos, &neg, &map);
    all.insert(map.begin(), map.end());
  }
  if (res)
    map.insert(all.begin(), all.end());
  if (!nary_name.empty())
  {
    // f0 because first insertion on a has map is the name
    auto search = ap_asso.find("f0");
    //for (auto s : ap_asso)
      //std::cout << s.first << "->" << s.second << std::endl;
    if (search != ap_asso.end())
      map.emplace(nary_name, search->second);
  }
  return res;
}

int main()
{
  sim_tree t;
  auto vect = input_divide("input");
  sim_line sl("", spot::formula(),"");
  for (unsigned i = 0; i < vect.size(); i++)
    if (input_check(i+1, vect[i], sl))
      t.insert(sl);
  t.to_dot("tree.dot");
  spot::formula res;
  spot::formula f = spot::parse_formula(input_divide("user_input")[0]);
  if (simp(t, f, res))
    std::cout << f << " final simplification: " << res << std::endl;
  else
    std::cout << f << " could not be reduced." << std::endl;
  //generate_simple(spot::parse_formula("f U (g | e)"), std::cout);
  return 0;
}
