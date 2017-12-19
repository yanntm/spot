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

spot::formula chg_ap_name(spot::formula f, str_map& ap_asso, bool parse)
{
  //debugging print map
  /*
  for(auto elem : ap_asso)
  {
    std::cout << '\'' << elem.first << "' '" << elem.second << '\''
    << '\n';
  }
  */
  std::function<spot::formula(spot::formula)> chg_ap;
  chg_ap = [&chg_ap, ap_asso, parse](spot::formula f)->spot::formula
  {
    if (f.is(spot::op::ap))
    {
      auto name = f.ap_name();
      //std::cout << name << '\n';
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

bool find_all_in(spot::formula& f, spot::formula& origin)
{
  auto orsize = origin.size();
  auto fsize = f.size();
  if (origin.kind() != f.kind())
    return false;
  unsigned n = 0;
  for (;n < orsize; n++)
  {
    auto suborigin = origin[n];
    unsigned m = 0;
    for (; m < fsize; m++)
      if (f[m] == suborigin)
        break;
    if (m == fsize)
      break;
  }
  return n == orsize;
}


//////////////////////////////////////////////////
///
/// simtree
///
//////////////////////////////////////////////////


bool sim_tree::insert(sim_line sl, str_map& map)
{
  conds_set condv;
  return root_->insert(sl, map, 1, condv);
}

bool sim_tree::insert(sim_line sl)
{
  str_map map;
  conds_set condv;
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
  return root_->apply(f, res, ap_asso, true, mark, true);
}

bool sim_tree::apply(spot::formula f, spot::formula& res)
{
  str_map map;
  std::vector<bool> mark;
  return root_->apply(f, res, map, true, mark, true);
  /*
  bool rep = root_->apply(f, res, map, true);
  for (auto e : map)
    std::cout << e.first << "->" << e.second << '\n';
  if (rep)
    std::cout << f << " is simplified " << res << '\n';
  else
    std::cout << f << " cannot be simplified." << '\n';
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

bool args_node::insert(sim_line& sl, str_map& ap_asso, int state,
    conds_set& cond, bool nw)
{
  auto f = sl.formula_get();
  if (!nw)
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
    std::string str = "f" + std::to_string(ap_asso.size());
    if (search == ap_asso.end())
      ap_asso.emplace(name, str);
    else
    {
      // inserting unique value to update map size
      ap_asso.emplace(name + '=' + str, str);
      sl.cond_add(str + "=" + search->second);
    }
    //str = search->second;
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
        std::string ret = sl.replace_get();
        conds condi = conds(sl.cond_get(), ret, ap_asso);
        for (auto ptr : cond)
        {
          std::unique_ptr<std::vector<conds>>& c = *ptr;
          if (c != condition_)
            c->push_back(condi);
        }
        condition_->push_back(condi);
      }
      else
        cond.insert(&condition_);
    }
    return true;
  }
  return children_.back()->insert(sl, ap_asso, state, cond, nw);
}




// uses the args_node under op corresponding to f
bool args_node::is_equivalent(spot::formula f, str_map& asso, bool insert)
{
  auto emplace_check = [&asso, &insert](std::string first, std::string second) -> bool
  {
    for (auto p : asso)
      if (p.second == second && p.first != first)
        return false;
    if (!insert)
      asso.emplace(second, first);
    else
      asso.emplace(first, second);
    return true;
  };
  int nb = f.size();
  if (is_nary(f))
  {
    if (children_.size() == 1)
    {
      // handling 'args & ...' -> pushing every n-ary OP children in the map
      // or if it is insertion ->
      std::string match = "";
      if (insert && f.size() == 2 && (f[0].ap_name() == "..."
            || f[1].ap_name() == "..."))
      {
        if (f[0].ap_name() == "...")
          match = f[1].ap_name();
        else
          match = f[0].ap_name();
      }
      else
        match = str_psl(f);
      auto name = children_[0]->opstr_get();
      if (!emplace_check(match, name))
        return false;
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
          if (!emplace_check(str_psl(f[i]), child.opstr_get()))
            return false;
          if (!insert)
            continue;
        }
        unsigned j = 0;
        for (; j < nb; j++)
        {
          auto kind = f[j].kind();
          if (chkind == kind)
          {
            // !insert prevents insert on map
            if (/*!insert || */child.is_equivalent(f[j], asso, insert))
              break;
          }
        }
        if (j == nb)
          return false;
      }
      return true;
    }
    return false;
  }
  for (unsigned i = 0; i < nb; i++)
  {
    auto kind = f[i].kind();
    auto& child = *(children_[i]);
    auto chkind = child.op_get();
    // AP in the tree matches any formula, so we put formula in the name map.
    if (chkind == spot::op::ap)
    {
      if (!emplace_check(str_psl(f[i]), child.opstr_get()))
        return false;
      if (!insert)
        continue;
    }
    // insert prevents map insertion again?
    if (chkind != kind || (/*insert && */!child.is_equivalent(f[i], asso, insert)))
      return false;
  }
  return true;
}

bool args_node::apply(spot::formula f, spot::formula& res, str_map& ap_asso,
    bool last, int i, bool top_op)
{
  auto& child = children_[i];
  auto op = child->op_get();
  if (op == spot::op::ap || f.kind() == op)
    if (child->apply(f, res, ap_asso, last, top_op))
    {
      // !res so we do not try to access condition_ after
      if (last && !res)
      {
        auto cond = *condition_;
        unsigned nb = cond.size();
        unsigned j = 0;
        for (; j < nb; j++)
        {
          str_map map = ap_asso;
          if (cond[j].check(map))
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
    bool last, std::vector<bool>& marks, bool top_op)
{
  for (unsigned i = 0; i < children_.size(); i++)
  {
    if (marks.size() && marks[i])
      continue;
    auto& child = children_[i];
    auto op = child->op_get();
    if (op == spot::op::ap || f.kind() == op)
      if (child->apply(f, res, ap_asso, last, top_op))
      {
        // !res so we do not try to access condition_ after
        if (last && !res)
        {
          auto cond = *condition_;
          unsigned nb = cond.size();
          unsigned j = 0;
          str_map map;
          for (; j < nb; j++)
          {
            map = ap_asso;
            if (cond[j].check(map))
              break;
          }
          if (j == nb)
            continue;
          //for (auto e : ap_asso)
            //std::cout << e.first << "->" << e.second << '\n';
          // building res
          // auto cond = chg_ap_name(search->second.condf_get(), ap_asso);
          res = chg_ap_name(cond[j].retf_get(), map, true);
          //std::cout << "res: " << res << '\n';
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

bool op_node::insert(sim_line& sl, str_map& ap_asso, int state,
    conds_set& cond, bool nw)
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
  auto ins = [&](unsigned child, str_map* vect = nullptr) -> bool
  {
    if (vect)
      ap_asso.insert(vect->begin(), vect->end());
      //for (auto pair : *vect)
        //ap_asso.emplace(pair.first, pair.second);
    bool inserted = false;
    bool commut = is_commut(op_);
    for (unsigned i = 0; i < f.size(); i++)
    {
      //do not insert duplicates ap when is_equivalent
      if (!vect || f[i].kind() != spot::op::ap)
      {
        sim_line sl2(sl.cond_get(), f[i], sl.replace_get());
        inserted |= children_[child]->insert(sl2, ap_asso,
            update_state(i == f.size() - 1, commut), cond, !vect);
        sl.cond_get() = sl2.cond_get();
      }
      else
        //insert cond if insert was not called
        if (update_state(i == f.size() - 1, commut))
        {
          // FIXME surely handle state == 2
          auto& conda = children_[child]->condition_get();
          if (!conda)
            conda = std::make_unique<std::vector<conds>>();
          auto condi = conds(sl.cond_get(), sl.replace_get(), ap_asso);
          conda->push_back(condi);
          if (commut)
            for (unsigned i = 0; i < children_.size() && i != child; i++)
              children_[i]->condition_get()->push_back(condi);
        }
    }
    return inserted;
  };
  if (!nw)
    for (unsigned i = 0; i < children_.size(); i++)
    {
      str_map vect;
      if (children_[i]->is_equivalent(f, vect, true))
        return ins(i, &vect);
    }
  children_.push_back(std::make_unique<args_node>());
  return ins(children_.size() - 1);
}

bool op_node::is_equivalent(spot::formula f, str_map& asso, bool insert)
{
  if (op_ == spot::op::ap && f.kind() == op_)
  {
    auto search = insert ? asso.find(f.ap_name()) : asso.find(opstr_);
    if (search == asso.end())
      SPOT_UNREACHABLE();
    return search->second == (insert ? opstr_ : f.ap_name());
  }
  for (auto& child : children_)
    if (child->is_equivalent(f, asso, insert))
      return true;
  //std::cout << opstr_ << ' ' << f.kindstr() << '\n';
  return false;
}

inline spot::formula nary_cons(spot::op op, std::vector<spot::formula> c)
{
  switch (op)
  {
    case spot::op::And:
      return spot::formula::And(c);
    case spot::op::AndRat:
      return spot::formula::AndRat(c);
    case spot::op::AndNLM:
      return spot::formula::AndNLM(c);
    case spot::op::Or:
      return spot::formula::Or(c);
    case spot::op::OrRat:
      return spot::formula::OrRat(c);
    case spot::op::Concat:
      return spot::formula::Concat(c);
    case spot::op::Fusion:
      return spot::formula::Fusion(c);
    default:
      SPOT_UNREACHABLE();
  }
}

bool op_node::apply_top_nary(spot::formula& f, spot::formula& res)
{
  std::function<bool(int, std::vector<spot::formula>, spot::formula&,
      op_child&)> iterate;
  int k = f.size();
  int n;
  std::set<int> set;
  // iterate goes through the list of binomial coefficient possibilities
  iterate = [&](int start, std::vector<spot::formula> comb,
      spot::formula& res, op_child& child) -> bool
  {
    if (n == comb.size())
    {
      auto newf = nary_cons(f.kind(), comb);
      //std::cout << newf << '\n';
      int fsize = newf.size();
      auto marks = std::vector<bool>(fsize, false);
      int i;
      str_map new_asso;
      for (; i < fsize; i++)
        if (!child->apply(newf[i], res, new_asso, i == fsize - 1, marks))
          break;
      if (i == fsize)
        return true;
    }
    else
      for (int i = start; i < k; i++)
      {
        //std::cout << "add " << i << ' ' << f[i] << '\n';
        set.insert(i);
        comb.push_back(f[i]);
        if (iterate(i + 1, comb, res, child))
        {
          std::vector<spot::formula> excluded;
          for (int j = 0; j < k; j++)
            if (set.find(j) == set.end())
              excluded.push_back(f[j]);
          // adding simplification of included parts
          excluded.push_back(res);
          res = nary_cons(f.kind(), excluded);
          return true;
        }
        //std::cout << "rm " << i << ' ' << f[i] << '\n';
        set.erase(i);
        comb.pop_back();
      }
      return false;
  };
  //std::map<std::vector<std::pair<int, int>>, str_map> cache;

  // iterate over pattern possiblities
  for (int i = 0; i < children_.size(); i++)
  {
      auto& child = children_[i];
      n = child->size();
      if (iterate(0, std::vector<spot::formula>(), res, child))
        return true;
  }
  return false;
}

bool op_node::apply(spot::formula f, spot::formula& res, str_map& ap_asso,
    bool last, bool top_op)
{
  bool ins;
  //std::cout << f.kindstr() << ' ' << (f.kind() == op_) << '\n';
  if (op_ == spot::op::ap || is_final(f) && f.kind() == op_)
  {
    if (op_ == spot::op::ap)
      ap_asso.emplace(opstr_, str_psl(f));
    return true;
  }
  if (top_op && is_nary(f))
  {
    spot::formula rep;
    if (apply_top_nary(f, rep))
    {
      while (true)
      {
        spot::formula tmp;
        //check nary because the transformed formula can be another if there
        //were not trash terms and we dont want to use apply_top_nary on it.
        if (!is_nary(rep) || !apply_top_nary(rep, tmp))
          break;
        rep = tmp;
      }
      res = rep;
      return true;
    }
    return false;
  }

  for (unsigned j = 0; j < children_.size(); j++)
  {
    auto& child = children_[j];
    str_map vect;
    if (child->is_equivalent(f, vect, false))
    {
      str_map new_asso = ap_asso;
      for (auto pair : vect)
        if (new_asso.find(opstr_) == new_asso.end())
          new_asso.emplace(pair.first, pair.second);
      unsigned i = 0;
      unsigned fsize = f.size();
      if (is_nary(f) && children_.size() == 1)
        fsize = 1;
      auto kind = f.kind();
      bool commut = is_commut(op_);
      std::vector<bool> marks;
      if (commut)
          marks = std::vector<bool>(fsize, false);

      for (; i < fsize; i++)
      {
        //if (top_operator)

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
    std::cout << '\'' << elem.first << "' '" << elem.second << "\'\n";
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
            //std::cout << fn << '\n';
            std::string rule = condi.substr(search + 1);
            if (rule == "eventual" && !fn.is_eventual())
              return false;
            else if (rule == "universal" && !fn.is_universal())
              return false;
            else if (rule == "univentual"
                && (!fn.is_universal() || !fn.is_eventual()))
              return false;
            else if (rule == "boolean" && !fn.is_boolean())
              return false;
            else if (rule == "sere" && !fn.is_sere_formula())
              return false;
            else if (rule.substr(0, 6) == "equals")
              if (ap_asso.find(name)->second
                  != ap_asso.find(rule.substr(6))->second)
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
            if (search == ap_asso.end())
              SPOT_UNREACHABLE();
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
            /*
            std::cout << "fn & fp: "<< fn << " / " << fp << '\n';
            /*
            for (auto e : ap_asso)
              std::cout << e.first << "->" << e.second << '\n';
            */
            str_vect pos;
            str_vect neg;
            if (!has(fn, fp, pos, neg, ap_asso))
              return false;
            // debugging purpose
            /*
            for (auto e : ap_asso)
              std::cout << e.first << "->" << e.second << '\n';
            //std::cout << retf_ << '\n';
            for (auto s : pos)
              std::cout << '"' << s << "\" matches " << fp << '\n';
            for (auto s : neg)
              std::cout << '"' << s << "\" doesn't match " << fp << '\n';
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
            int min = 0;
            int max = 0;
            auto splits = [&](str_vect& vect, std::string str, splitmap& map)
            {
              std::string rec_name = name;
              std::string former = name;
              do
              {
                former = rec_name;
                for (auto s : ap_asso)
                  if (s.second == name)
                  {
                    rec_name = s.first;
                    break;
                  }
              } while (rec_name != former);
              std::string rep = "";
              auto search = map.find(std::make_pair(rec_name, ptrn));
              if (search != map.end())
              {
                auto tup = search->second;
                rep = std::get<0>(tup);
                min = std::get<1>(tup);
                max = std::get<2>(tup);
              }
              std::string split;
              if (!vect.empty())
              {
                replace(vect, map, rep);
                split = vect[0];
                for (unsigned i = 1; i < vect.size(); i++)
                  split += op + vect[i];
              }
              ap_asso.emplace(str + "(" + rec_name + ", " + ptrn
                  + (rep.size() ? ", " + rep : "") + ")", split);
            };
            /*
            for (auto p : split_)
              std::cout << p.first.first << '/' << p.first.second << "->"
                << p.second << '\n';
            for (auto p : splitnot_)
              std::cout << p.first.first << '/' << p.first.second << "->"
                << p.second << '\n';
            */
            auto checks = [&](str_vect& vect, std::string str, splitmap& map)
            -> bool
            {
              splits(vect, str, map);
              int size = vect.size();
              // FIXME
              if ((min && size < min) || (max && size > max))
                return false;
              return true;
            };
            if (!checks(pos, "split", split_))
              return false;
            if (!checks(neg, "splitnot", splitnot_))
              return false;
          }
          break;
        }
      case spot::op::Implies:
        // FIXME do the Thing. The thing where you do things (equiv)
        break;
      default:
        SPOT_UNREACHABLE();
    }
  }
  return true;
}







