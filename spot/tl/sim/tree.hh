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



/// \file tl/sim/tree.hh
/// \brief LTL/PSL simplification interface
#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <spot/tl/parse.hh>
#include <spot/tl/print.hh>
#include <spot/tl/formula.hh>
#include <spot/twa/bdddict.hh>
#include <memory>
#include <map>
#include <set>


typedef std::map<std::string, std::string> str_map;
typedef std::vector<std::string> str_vect;
typedef std::map<std::pair<std::string, std::string>,
        std::tuple<std::string, int, int>> splitmap;


spot::formula chg_ap_name(spot::formula f, str_map& ap_asso,
    bool parse = false);
bool has(spot::formula& f, spot::formula& pattern, str_vect& pos,
    str_vect& neg, str_map& map);
void rm_spaces(std::string& str);
std::string str(spot::op op);
bool is_nary(spot::formula& f);

std::string sar(std::string str, std::string key, std::string replace);


class args_node;

class sim_line
{
  public:
    sim_line(std::string cond, spot::formula formula, std::string replace)
      : cond_(cond),
      formula_(formula),
      replace_(replace)
  {}

    std::string& cond_get()
    {
      return cond_;
    }

    spot::formula& formula_get()
    {
      return formula_;
    }

    std::string& replace_get()
    {
      return replace_;
    }

    void cond_add(std::string condi)
    {
      if (cond_.empty())
        cond_ = condi;
      else
        cond_ += " and " + condi;
    }

  private:
    std::string cond_;
    spot::formula formula_;
    std::string replace_;
};

class builtin
{
public:
  builtin(std::string& str, int& end)
  {
    int save = 0;
    int par = 0;
    for (unsigned i = 0; i < str.size(); i++)
    {
      if (str[i] == '(')
      {
        if (!save)
        {
          name_ = str.substr(0, i);
          save = i + 1;
        }
        ++par;
      }
      else if (str[i] == ')')
      {
        if (!par--)
          throw std::exception();
        if (!par)
        {
          std::string nm = str.substr(save, i - save);
          args_.push_back(nm);
          end = i + 1;
          break;
        }
      }
      else if (str[i] == ',')
      {
        if (!save)
          throw std::exception();
        std::string nm = str.substr(save, i - save);
        args_.push_back(nm);
        save = i + 1;
      }
    }
    rm_spaces(name_);
    for (auto& arg : args_)
      rm_spaces(arg);
  }

  std::string& arg_no(int i)
  {
    return args_[i];
  }

  int size()
  {
    return args_.size();
  }

  std::string name_get()
  {
    return name_;
  }

  std::string to_string()
  {
    std::string str = name_ + "(" + args_[0];
    for (int i = 1; i < args_.size(); i++)
      str += ", " + args_[i];
    str+= ")";
    return str;
  }
private:
  std::string name_;
  std::vector<std::string> args_;
};

std::ostream& operator<<(std::ostream& os, builtin& f);


class conds
{
public:
  conds()
  {
    empty_ = true;
  }

  conds(std::string cond, std::string ret, str_map& ap_asso)
  {
    empty_ = false;
    //for (auto elem : ap_asso)
      //std::cout << "->" << elem.first << " // " << elem.second << std::endl;
    auto add_cond = [this](std::string str)
    {
      cond_ += (cond_.size() ? " & " : "") + str;
      condf_.push_back(spot::parse_formula(str));
    };
    for (auto elem : ap_asso)
    {
      //std::cout << "-> " << elem.first << " // " << elem.second << std::endl;
      auto name = elem.first[0];
      if (name == 'e')
        add_cond(elem.second + ".eventual");
      else if (name == 'u')
        add_cond(elem.second + ".universal");
      else if (name == 'q')
        add_cond(elem.second + ".univentual");
      else if (name == 'b')
        add_cond(elem.second + ".boolean");
      else if (name == 'r')
        add_cond(elem.second + ".sere");
    }
    if (!cond.empty())
    {
      unsigned begin = 0;
      std::string delim = " and ";
      auto search = cond.find(delim);
      bool last = false;
      while (!last)
      {
        if (search == std::string::npos)
          last = true;
        std::string condi = cond.substr(begin, search - begin);
        //std::cout << "CONDi" << condi << std::endl;
        if (condi.find('@') != std::string::npos)
        {
          int end;
          builtin at(condi, end);
          std::string& arg1 = at.arg_no(0);
          auto find = ap_asso.find(arg1);
          if (find == ap_asso.end())
            ap_asso.emplace(arg1, arg1);
          else
            arg1 = find->second;
          add_cond("\""  + at.to_string() + "\"");
        }
        else
        {
          auto eq = condi.find('=');
          if (eq != std::string::npos)
          {
            condi.replace(eq, 1, ".equals");
            add_cond(condi);
          }
          else
            add_cond(spot::str_psl(chg_ap_name(spot::parse_formula(condi),
                     ap_asso)));
        }
        if (last)
          break;
        begin = search + delim.size();
        search = cond.find(delim, begin);
      }
    }
    if (cond_.empty())
      cond_ = "1";
    auto search = ret.find("split");
    while (search != std::string::npos)
    {
      std::string retsub = std::string(ret, search, ret.size() - search);
      int end;
      //std::cout << retsub << std::endl;
      builtin spl(retsub, end);
      std::string& arg1 = spl.arg_no(0);
      auto find = ap_asso.find(arg1);
      if (find == ap_asso.end())
        ap_asso.emplace(arg1, arg1);
      //std::cout << arg1 << std::endl;
      //for (auto elem : ap_asso)
      //std::cout << "-> " << elem.first << " // " << elem.second << std::endl;
      else
        arg1 = find->second;
      //std::cout << arg1 << std::endl;
      std::string& arg2 = spl.arg_no(1);
      std::string arg3;
      int min = 0;
      int max = 0;
      int size =spl.size();
      if (size == 3 || size == 5)
        arg3 = spl.arg_no(2);
      if (size > 3)
      {
        try
        {
          min = std::stoi(spl.arg_no(size - 1));
          max = std::stoi(spl.arg_no(size - 2));
        }
        catch (std::exception e)
        {
          std::cerr << spl.to_string() << e.what() << std::endl;
        }
      }
      std::string str = spl.to_string();
      ap_asso.emplace(str, str);
      if (spl.name_get() == "splitnot")
        splitnot_.emplace(std::make_pair(arg1, arg2), std::make_tuple(arg3,
              min, max));
      else
        split_.emplace(std::make_pair(arg1, arg2), std::make_tuple(arg3, min,
              max));
      std::string rep = "\"" + spl.to_string() + "\"";
      ret.replace(search, end, rep);
      search = ret.find("split", search + rep.size());
    }
    /*
    for (auto p : splitnot_)
      std::cout << p.first.first << '/' << p.first.second << "->"
        << p.second << std::endl;
    for (auto p : split_)
      std::cout << p.first.first << '/' << p.first.second << "->"
        << p.second << std::endl;
    */
    std::string debug = ret;
    debug.insert(0, ">");
    retf_ = chg_ap_name(spot::parse_formula(ret), ap_asso);
    ret_ = str_psl(retf_);
  }

    bool check(str_map& ap_asso);


    std::string cond_get()
    {
      return cond_;
    }

    std::string ret_get()
    {
      return ret_;
    }

    spot::formula retf_get()
    {
      return retf_;
    }

    operator bool() const
    {
      return empty_;
    }

  private:
    std::string cond_;
    std::vector<spot::formula> condf_;
    std::string ret_;
    spot::formula retf_;
    splitmap split_;
    splitmap splitnot_;
    bool empty_;
};

void generate_simple(spot::formula f, spot::formula res, conds cond,
    std::ostream& os);
typedef std::set<std::unique_ptr<std::vector<conds>>*> conds_set;

class op_node
{
  public:
    op_node(spot::formula f, std::string asso = "")
      : op_(f.kind()),
      opstr_(f.kindstr())
  {
    bool size = f.size();
    if (!size)
    {
      switch (f.kind())
      {
        case spot::op::ap:
          opstr_ = asso;
          break;
        case spot::op::tt:
          opstr_ = "true";
          break;
        case spot::op::ff:
          opstr_ = "false";
          break;
        case spot::op::eword:
          break;
          opstr_ = ".";
          break;
        default:
          SPOT_UNREACHABLE();
      }
    }
  }
    spot::op op_get()
    {
      return op_;
    }

    std::string opstr_get()
    {
      return opstr_;
    }

    int size()
    {
      return children_.size();
    }

    bool insert(sim_line& sl, str_map& ap_asso, int state, conds_set& cond,
        bool nw = false);
    bool is_equivalent(spot::formula f, str_map& asso, bool insert);
    bool apply(spot::formula f, spot::formula& res, str_map& ap_asso,
        bool last, bool top_op = false);
    void to_dot(int *i, int father, std::ofstream& ofs);
    bool apply_top_nary(spot::formula& f, spot::formula& res);

    op_node(op_node&) = delete;

  private:
    spot::op op_;
    std::string opstr_;
    std::vector<std::unique_ptr<args_node>> children_;
};

typedef std::unique_ptr<args_node> op_child;
typedef std::unique_ptr<op_node> args_child;


class args_node
{
  public:
    bool insert(sim_line& sl, str_map& ap_asso, int state, conds_set& cond,
        bool nw =  false);
    // for sorted op
    bool apply(spot::formula f, spot::formula& res, str_map& ap_asso,
        bool last, int i, bool top_op = false);
    // for root and unsorted op
    bool apply(spot::formula f, spot::formula& res, str_map& ap_asso,
        bool last, std::vector<bool>& marks, bool top_op = false);
    void to_dot(int *i, int father, std::ofstream& ofs);
    bool is_equivalent(spot::formula f, str_map& vect, bool insert);

    std::string ap_name()
    {
      if (children_.size() != 1 || children_[0]->op_get() != spot::op::ap)
        SPOT_UNREACHABLE();
      return children_[0]->opstr_get();
    }

    std::unique_ptr<std::vector<conds>>& condition_get()
    {
      return condition_;
    }

    int size()
    {
      return children_.size();
    }

  private:
    std::vector<std::unique_ptr<op_node>> children_;
    std::unique_ptr<std::vector<conds>> condition_;
};

class sim_tree
{
  public:
    sim_tree()
    {
      root_ = std::make_unique<args_node>();
    }

    bool insert(sim_line sl, str_map& str);
    bool insert(sim_line sl);
    bool apply(spot::formula f, spot::formula& res);
    bool apply(spot::formula f, spot::formula& res, str_map& ap_asso);
    void to_dot(std::string filename);
  private:
    std::unique_ptr<args_node> root_;
};
