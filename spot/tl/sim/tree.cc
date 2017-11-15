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


///////////////////////////////////////
///
/// Tools
///
///////////////////////////////////////

// replace ap names in f by the corresponding in ap_asso

spot::formula chg_ap_name(spot::formula f, str_map ap_asso, bool parse)
{
  //debugging print map
  /*
  for(auto elem : ap_asso)
  {
    std::cout << '\'' << elem.first << "' '" << elem.second << '\''
    << std::endl;
  }
  */
  std::function<spot::formula(spot::formula)> chg_ap;
  chg_ap = [&chg_ap, ap_asso, parse](spot::formula f)->spot::formula
  {
    if (f.is(spot::op::ap))
    {
      auto name = f.ap_name();
      //std::cout << name << std::endl;
      auto search = ap_asso.find(name);
      if (search == ap_asso.end())
        SPOT_UNREACHABLE();
      if (parse)
        return spot::parse_formula(search->second);
      else
        return spot::formula::ap(search->second);
    }
    return f.map(chg_ap);
  };
  return chg_ap(f);
}

// does the formula has child?
bool is_final(spot::formula& f)
{
  bool ret = f.size();
  return !ret;
}

std::string str(spot::op op)
{
  switch (op)
  {
    case spot::op::Or:
    case spot::op::OrRat:
      return " | ";
    case spot::op::And:
    case spot::op::AndNLM:
      return " & ";
    case spot::op::AndRat:
      return " && ";
    default:
      SPOT_UNREACHABLE();
  }
}

// is an nary operator ?
bool is_nary(spot::formula& f)
{
  switch (f.kind())
  {
    case spot::op::And:
    case spot::op::AndRat:
    case spot::op::AndNLM:
    case spot::op::Or:
    case spot::op::OrRat:
    case spot::op::Concat:
    case spot::op::Fusion:
      return true;
    default:
      return false;
  }
}

bool is_commut(spot::op kind)
{
  switch (kind)
  {
    case spot::op::Equiv:
    case spot::op::Or:
    case spot::op::OrRat:
    case spot::op::And:
    case spot::op::AndRat:
    case spot::op::AndNLM:
      return true;
    default:
      return false;
  }
}
// search key in str and replaces it by replace
std::string sar(std::string str, std::string key, std::string replace)
{
  std::string res = str;
  auto pos = res.find(key);
  while (pos != std::string::npos)
  {
    res.replace(pos, key.length(), replace);
    pos = res.find(key, pos + replace.length());
  }
  return res;
}


// precede by backslash special characters for dot format labels
inline std::string dot_no_special(std::string str)
{
  return sar(sar(sar(sar(sar(sar(str, ">", "\\>"), "<", "\\<"), "|", "\\|"),
    "(", "\\("), ")", "\\)"), "\"", "");
}


//////////////////////////////////////////////////
///
/// simtree
///
//////////////////////////////////////////////////


bool sim_tree::insert(sim_line sl)
{
  str_map map;
  conds_vect condv;
  return root_->insert(sl, map, 1, condv);
}

void sim_tree::to_dot(std::string filename)
{
  int i = 1;
  std::ofstream ofs(filename);
  ofs << "digraph simplifyTree{\n";
  root_->to_dot(&i, 0, ofs);
  ofs << "}\n";
}

bool sim_tree::apply(spot::formula f, spot::formula& res, str_map& ap_asso)
{
  std::vector<bool> mark;
  return root_->apply(f, res, ap_asso, true, mark);
}

bool sim_tree::apply(spot::formula f, spot::formula& res)
{
  str_map map;
  std::vector<bool> mark;
  return root_->apply(f, res, map, true, mark);
  /*
  bool rep = root_->apply(f, res, map, true);
  for (auto e : map)
    std::cout << e.first << "->" << e.second << std::endl;
  if (rep)
    std::cout << f << " is simplified " << res << std::endl;
  else
    std::cout << f << " cannot be simplified." << std::endl;
  return rep;
  */
}

//////////////////////////////////////////////////////
///
/// argsnode
///
//////////////////////////////////////////////////////


void args_node::to_dot(int *i, int father, std::ofstream& ofs)
{
  int nb = (*i)++;
  if (!father)
    ofs << nb << " [label=\"root\" shape=box];\n";
  else
  {
    if (condition_)
    {
      auto cond = *condition_;
      std::string str = "{ ";
      for (auto elem : cond)
      {
        str += "{ " + dot_no_special(elem.cond_get()) + " | "
          + dot_no_special(elem.ret_get()) + " } |";
      }
      str.pop_back();
      str += " } ";
      ofs << nb << " [label=\"" << str << "\" shape=record];\n";
    }
    else
      ofs << nb << " [label=\"\" shape=box];\n";
    ofs << father << " -> " << nb << '\n';
  }
  for (unsigned j = 0; j < children_.size(); j++)
  {
    auto& child = children_[j];
    child->to_dot(i, nb, ofs);
  }
}

bool args_node::insert(sim_line sl, str_map& ap_asso, int state,
    conds_vect& cond)
{
  auto f = sl.formula_get();
  for (unsigned i = 0; i < children_.size(); i++)
  {
    auto& child = children_[i];
    if (child->op_get()== f.kind() && !is_final(f))
      return child->insert(sl, ap_asso, state, cond);
  }
  if (f.kind() == spot::op::ap)
  {
    auto name = f.ap_name();
    if (name == "...")
      // here could be handled nary checking if input is not trustworthy
      return true;
    auto search = ap_asso.find(name);
    std::string str;
    if (search == ap_asso.end())
    {
      str = "f" + std::to_string(ap_asso.size());
      ap_asso.emplace(name, str);
    }
    else
      str = search->second;
    children_.push_back(std::make_unique<op_node>(f, str));
  }
  else
    children_.push_back(std::make_unique<op_node>(f));
  if (is_final(f))
  {
    if (state)
    {
      if (!condition_)
        condition_ = std::make_unique<std::vector<conds>>();
      if (state == 1)
      {
        conds condi = conds(sl.cond_get(), sl.replace_get(), ap_asso);
        for (auto ptr : cond)
          ptr->get()->push_back(condi);
        condition_->push_back(condi);
      }
      else
        cond.push_back(&condition_);
    }
    return true;
  }
  return children_.back()->insert(sl, ap_asso, state, cond);
}



bool args_node::is_equivalent(spot::formula f, pair_vect& asso, bool insert)
{
  int nb = f.size();
  // handling 'args & ...' -> pushing every n-ary OP children in the map
  if (is_nary(f))
  {
    if (children_.size() == 1)
    {
      auto name = children_[0]->opstr_get();
      asso.emplace_back(str_psl(f), name);
      return true;
    }
    if (children_.size() == nb)
    {
      for (unsigned i = 0; i < nb; i++)
      {
        auto& child = *(children_[i]);
        auto chkind = child.op_get();
        if (chkind == spot::op::ap)
        {
          asso.emplace_back(str_psl(f[i]), child.opstr_get());
          if (!insert)
            continue;
        }
        unsigned j = 0;
        for (; j < nb; j++)
        {
          auto kind = f[j].kind();
          if (chkind == kind)
            break;
        }
        if (j == nb)
          return false;
      }
      return true;
    }
  }
  for (unsigned i = 0; i < nb; i++)
  {
    auto kind = f[i].kind();
    auto& child = *(children_[i]);
    auto chkind = child.op_get();
    // AP in the tree matches any formula, so we put formula in the name map.
    if (chkind == spot::op::ap)
    {
      asso.emplace_back(str_psl(f[i]), child.opstr_get());
      if (!insert)
        continue;
    }
    if (chkind != kind)
      return false;
  }
  return true;
}

bool args_node::apply(spot::formula f, spot::formula& res, str_map& ap_asso,
    bool last, int i)
{
  auto& child = children_[i];
  auto op = child->op_get();
  if (op == spot::op::ap || f.kind() == op)
    if (child->apply(f, res, ap_asso, last))
    {
      // !res so we do not try to access condition_ after
      if (last && !res)
      {
        auto cond = *condition_;
        unsigned nb = cond.size();
        unsigned j = 0;
        for (; j < nb; j++)
        {
          if (cond[j].check(ap_asso))
            break;
        }
        if (j == nb)
          return false;
        res = chg_ap_name(cond[j].retf_get(), ap_asso, true);
      }
      return true;
    }
  return false;
}

bool args_node::apply(spot::formula f, spot::formula& res, str_map& ap_asso,
    bool last, std::vector<bool>& marks)
{
  for (unsigned i = 0; i < children_.size(); i++)
  {
    if (marks.size() && marks[i])
      continue;
    auto& child = children_[i];
    auto op = child->op_get();
    if (op == spot::op::ap || f.kind() == op)
      if (child->apply(f, res, ap_asso, last))
      {
        // !res so we do not try to access condition_ after
        if (last && !res)
        {
          auto cond = *condition_;
          unsigned nb = cond.size();
          unsigned j = 0;
          for (; j < nb; j++)
          {
            if (cond[j].check(ap_asso))
              break;
          }
          if (j == nb)
            continue;
          //for (auto e : ap_asso)
            //std::cout << e.first << "->" << e.second << std::endl;
          // building res
          // auto cond = chg_ap_name(search->second.condf_get(), ap_asso);
          res = chg_ap_name(cond[j].retf_get(), ap_asso, true);
        }
        if (!marks.empty())
          marks[i] = true;
        return true;
      }
  }
  return false;
}


//////////////////////////////////////////////////////
///
/// opnode
///
//////////////////////////////////////////////////////

void op_node::to_dot(int *i, int father, std::ofstream& ofs)
{
  int nb = (*i)++;
  ofs << nb << " [label=\"" << opstr_ << "\"];\n";
  if (father)
    ofs << father << " -> " << nb << '\n';
  for (unsigned j = 0; j < children_.size(); j++)
  {
    auto& child = children_[j];
    child->to_dot(i, nb, ofs);
  }
}

bool op_node::insert(sim_line sl, str_map& ap_asso, int state,
    conds_vect& cond)
{
  auto update_state = [&state](bool last, bool commut) -> int
  {
    if (!state || (state && last))
      return state;
    if (commut)
      return 2;
    return 0;
  };
  auto f = sl.formula_get();
  auto ins = [&](unsigned child, pair_vect* vect = nullptr) -> bool
  {
    if (vect)
      for (auto pair : *vect)
        ap_asso.emplace(pair.first, pair.second);
    bool inserted = false;
    for (unsigned i = 0; i < f.size(); i++)
    {
      bool commut = is_commut(op_);
      //do not insert duplicates ap when is_equivalent
      if (!vect || f[i].kind() != spot::op::ap)
      {
        inserted |= children_[child]->insert(sim_line(sl.cond_get(), f[i],
            sl.replace_get()), ap_asso, update_state(i == f.size() - 1, commut)
            , cond);
        /*
        {
          auto condi = children_[child]->condition_get()->back();
          for (unsigned i = 0; i < children_.size() && i != child; i++)
            children_[i]->condition_get()->push_back(condi);
        }*/
      }
      else
        //insert cond
        if (state && i == f.size() - 1)
        {
          // FIXME surely handle state == 2
          auto& cond = children_[child]->condition_get();
          if (!cond)
            cond = std::make_unique<std::vector<conds>>();
          auto condi = conds(sl.cond_get(), sl.replace_get(), ap_asso);
          cond->push_back(condi);
          if (commut)
            for (unsigned i = 0; i < children_.size() && i != child; i++)
              children_[i]->condition_get()->push_back(condi);
        }
    }
    return inserted;
  };
  for (unsigned i = 0; i < children_.size(); i++)
  {
    pair_vect vect;
    if (children_[i]->is_equivalent(f, vect, true))
      return ins(i, &vect);
  }
  children_.push_back(std::make_unique<args_node>());
  return ins(children_.size() - 1);
}

bool op_node::apply(spot::formula f, spot::formula& res, str_map& ap_asso,
    bool last)
{
  bool ins;
  //std::cout << f.kindstr() << ' ' << (f.kind() == op_) << std::endl;
  if (op_ == spot::op::ap || is_final(f) && f.kind() == op_)
    return true;
  for (unsigned j = 0; j < children_.size(); j++)
  {
    auto& child = children_[j];
    pair_vect vect;
    if (child->is_equivalent(f, vect, false))
    {
      str_map new_asso = ap_asso;
      for (auto pair : vect)
        if (new_asso.find(opstr_) == new_asso.end())
          new_asso.emplace(pair.second, pair.first);
      unsigned i = 0;
      unsigned fsize = f.size();
      auto kind = f.kind();
      bool commut = is_commut(op_);
      std::vector<bool> marks;
      if (commut)
        marks = std::vector<bool>(fsize, false);
      for (; i < fsize; i++)
      {
        if (commut || is_nary(f))
        {
          if (!child->apply(f[i], res, new_asso, last && i == fsize - 1,
                marks))
          break;
        }
        else
          if (!child->apply(f[i], res, new_asso, last && i == fsize - 1, i))
            break;
      }
      if (i == fsize)
      {
        ap_asso.insert(new_asso.begin(), new_asso.end());
        return true;
      }
    }
  }
  return false;
}

//////////////////////////////////////////////////////////
///
/// conds
///
/////////////////////////////////////////////////////////

bool conds::check(str_map& ap_asso)
{
/*
  for(auto elem : ap_asso)
  {
    std::cout << '\'' << elem.first << "' '" << elem.second << '\''
    << std::endl;
  }
*/
  for (auto form : condf_)
  {
    switch (form.kind())
    {
      case spot::op::tt:
        return true;
      case spot::op::ap:
        {
          std::string condi = form.ap_name();
          auto search = condi.find(".");
          if (search != std::string::npos && condi[search + 1] != '.')
          {
            std::string name = condi.substr(0, search);
            spot::formula fn = spot::parse_formula(ap_asso.find(name)->second);
            //std::cout << fn << std::endl;
            std::string rule = condi.substr(search + 1);
            if (rule == "eventual" && !fn.is_eventual())
              return false;
            if (rule == "universal" && !fn.is_universal())
              return false;
            if (rule == "univentual"
                && (!fn.is_universal() || !fn.is_eventual()))
              return false;
            if (rule == "boolean" && !fn.is_boolean())
              return false;
            break;
          }
          search = condi.find("@");
          if (search != std::string::npos)
          {
            int end;
            builtin at(condi, end);
            std::string& name = at.arg_no(0);
            auto search = ap_asso.find(name);
            std::string name_asso = name;
            while (search != ap_asso.end())
            {
              name = name_asso;
              name_asso = search->second;
              search = ap_asso.find(name_asso);
            }
            spot::formula fn = spot::parse_formula(name_asso);
            std::string& ptrn = at.arg_no(1);
            spot::formula fp = spot::parse_formula(ptrn);
            ///*
            std::cout << "fn & fp: "<< fn << " / " << fp << std::endl;
            /*
            for (auto e : ap_asso)
              std::cout << e.first << "->" << e.second << std::endl;
            */
            str_vect pos;
            str_vect neg;
            if (!has(fn, fp, pos, neg, ap_asso))
              return false;
            // debugging purpose
            /*
            for (auto e : ap_asso)
              std::cout << e.first << "->" << e.second << std::endl;
            //std::cout << retf_ << std::endl;
            for (auto s : pos)
              std::cout << '"' << s << "\" matches " << fp << std::endl;
            for (auto s : neg)
              std::cout << '"' << s << "\" doesn't match " << fp << std::endl;
            */

            auto op = str(fn.kind());
            // seems important
            auto replace = [&name, &ptrn](str_vect& vect, splitmap map,
                std::string rep = "")
            {
              if (!rep.empty())
              {
                sim_tree t;
                t.insert(sim_line("1", spot::parse_formula(ptrn), rep));
                for (unsigned i = 0; i < vect.size(); i++)
                {
                  spot::formula res;
                  // should always be true
                  t.apply(spot::parse_formula(vect[i]), res);
                  vect[i] = str_psl(res);
                }
              }
            };

            auto splits = [&](str_vect& vect, std::string str, splitmap& map)
            {
              std::string rep = "";
              auto search = map.find(std::make_pair(name, ptrn));
              if (search != map.end())
                rep = search->second;
              std::string split;
              if (!vect.empty())
              {
                replace(vect, map, rep);
                split = vect[0];
                for (unsigned i = 1; i < vect.size(); i++)
                  split += op + vect[i];
              }
              ap_asso.emplace(str + "(" + name + ", " + ptrn
                  + (rep.size() ? ", " + rep : "") + ")", split);
            };
            /*
            for (auto p : split_)
              std::cout << p.first.first << '/' << p.first.second << "->"
                << p.second << std::endl;
            for (auto p : splitnot_)
              std::cout << p.first.first << '/' << p.first.second << "->"
                << p.second << std::endl;
            */
            splits(pos, "split", split_);
            splits(neg, "splitnot", splitnot_);
          }
          break;
        }
      case spot::op::Implies:
        // do the Thing. The thing where you do things (equiv)
        break;
      default:
        SPOT_UNREACHABLE();
    }
  }
  return true;
}







