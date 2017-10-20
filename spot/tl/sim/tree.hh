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
#include <memory>
#include <map>

typedef std::vector<std::pair<std::string, std::string>> pair_vect;
typedef std::map<std::string, std::string> str_map;

spot::formula chg_ap_name(spot::formula f, str_map ap_asso);
bool has(spot::formula f, spot::formula pattern);


class args_node;

class sim_line
{
  public:
    sim_line(std::string cond, spot::formula formula, std::string replace)
      : cond_(cond),
      formula_(formula),
      replace_(replace)
  {}

    std::string cond_get()
    {
      return cond_;
    }

    spot::formula formula_get()
    {
      return formula_;
    }

    std::string replace_get()
    {
      return replace_;
    }

  private:
    std::string cond_;
    spot::formula formula_;
    std::string replace_;
};

class conds
{
  public:
    conds(std::string cond, std::string ret, str_map ap_asso)
  {
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
    }
    if (cond.size())
    {
      unsigned begin = 0;
      std::string delim = " and ";
      auto search = cond.find(delim);
      do
      {
        std::string condi = cond.substr(begin, search - begin);
        if (condi.find('@') != std::string::npos)
        {
          //
          condi = "\"" + condi + "\"";
          //
          auto args = condi.find('(') + 1;
          auto comma = condi.find(',');
          condi.replace(args, comma - args,
              ap_asso.find(condi.substr(args, comma - args))->second);
          add_cond(condi);
        }
        else
          add_cond(spot::str_psl(chg_ap_name(spot::parse_formula(condi),
                   ap_asso)));
        begin = search + delim.size();
        search = cond.find(delim, begin);
      } while (search != std::string::npos);
    }
    if (!cond_.size())
      cond_ = "1";
    auto search = ret.find("split");
    while (search != std::string::npos)
    {
      ret.insert(search, "\"");
      //
      auto args = ret.find('(', search) + 1;
      auto comma = ret.find(',', search);
      ret.replace(args, comma - args,
          ap_asso.find(ret.substr(args, comma - args))->second);
      auto searchend = ret.find(')', search);
      ret.insert(searchend + 1, "\"");
      std::string str = ret.substr(search + 1, searchend - search);
      ap_asso.emplace(str, str);
      search = ret.find("split", searchend);
    }
    retf_ = chg_ap_name(spot::parse_formula(ret), ap_asso);
    ret_ = str_psl(retf_);
  }

    bool check(spot::formula f, str_map& ap_asso);
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

  private:
    std::string cond_;
    std::vector<spot::formula> condf_;
    std::string ret_;
    spot::formula retf_;
};


class op_node
{
  public:
    op_node(spot::formula f, std::string asso = "")
      : op_(f.kind()),
      opstr_(f.kindstr())
  {
    if (f.size() == 0)
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

    bool insert(sim_line sl, str_map& ap_asso, bool last);
    bool apply(spot::formula f, spot::formula& res, str_map& ap_asso,
        bool last);
    void to_dot(int *i, int father, std::ofstream& ofs);

    op_node(op_node&) = delete;

  private:
    spot::op op_;
    std::string opstr_;
    std::vector<std::unique_ptr<args_node>> children_;
};


class args_node
{
  public:
    bool insert(sim_line sl, str_map& ap_asso, bool last);
    bool apply(spot::formula f, spot::formula& res, str_map& ap_asso,
        bool last);
    void to_dot(int *i, int father, std::ofstream& ofs);
    bool is_equivalent(spot::formula f, pair_vect& vect);

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

    bool insert(sim_line sl);
    bool apply(spot::formula f, spot::formula& res);
    void to_dot(std::string filename);
  private:
    std::unique_ptr<args_node> root_;
};
