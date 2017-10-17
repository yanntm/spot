#include <spot/tl/sim/tree.hh>


///////////////////////////////////////
///
/// Tools
///
///////////////////////////////////////

// replace ap names in f by the corresponding in ap_asso
spot::formula chg_ap_name(spot::formula f, str_map ap_asso)
{
//debugging print map

     for(auto elem : ap_asso)
     {
     std::cout << "'" << elem.first << "' '" << elem.second << "'" << std::endl;
     }

  std::function<spot::formula(spot::formula)> chg_ap;
  chg_ap = [&chg_ap, ap_asso](spot::formula f)->spot::formula
  {
    if (f.is(spot::op::ap))
    {
      auto name = f.ap_name();
      //std::cout << name << std::endl;
      auto search = ap_asso.find(f.ap_name());
      if (search == ap_asso.end())
        SPOT_UNREACHABLE();
      return spot::formula::ap(search->second);
    }
    return f.map(chg_ap);
  };
  return chg_ap(f);
}

// does the formula has child?
inline bool is_final(spot::formula f)
{
  return f.size() == 0;
}

inline bool is_special(std::string ap)
{
  auto name = ap[0];
  return name == 'u' || name == 'e' || name == 'f';
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
  return sar(sar(sar(str, ">", "\\>"), "<", "\\<"), "|", "\\|");
}


//////////////////////////////////////////////////
///
/// simtree
///
//////////////////////////////////////////////////


bool sim_tree::insert(sim_line sl)
{
  str_map map;
  return root_->insert(sl, map, true);
}

void sim_tree::to_dot(std::string filename)
{
  int i = 1;
  std::ofstream ofs(filename);
  ofs << "digraph simplifyTree{\n";
  root_->to_dot(&i, 0, ofs);
  ofs << "}\n";
}

bool sim_tree::apply(spot::formula f, spot::formula& res)
{
  str_map map;
  return root_->apply(f, res, map, true);
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
    ofs << father << " -> " << nb << "\n";
  }
  for (unsigned j = 0; j < children_.size(); j++)
  {
    auto& child = children_[j];
    child->to_dot(i, nb, ofs);
  }
}

bool args_node::insert(sim_line sl, str_map& ap_asso, bool last)
{
  auto f = sl.formula_get();
  for (unsigned i = 0; i < children_.size(); i++)
  {
    auto& child = children_[i];
    if(child->op_get()== f.kind() && !is_final(f))
      return child->insert(sl, ap_asso, last);
  }
  if (f.kind() == spot::op::ap)
  {
    auto name = f.ap_name();
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
    if (last)
    {
      if (!condition_)
        condition_ = std::make_unique<std::vector<conds>>();
      condition_->push_back(conds(sl.cond_get(), sl.replace_get(),
            ap_asso));
    }
    return true;
  }
  return children_[children_.size() - 1]->insert(sl, ap_asso, last);
}



bool args_node::is_equivalent(spot::formula f, pair_vect& asso, bool check)
{
  // must do something handling 'args'
  int nb = f.size();
  for (unsigned i = 0; i < nb; i++)
  {
    auto kind = f[i].kind();
    auto& child = *(children_[i]);
    auto chkind = child.op_get();
    // AP in the tree matches any formula, so we put formula in the name map.
    if (chkind == spot::op::ap)
    {
      asso.push_back(std::make_pair(str_psl(f[i]), child.opstr_get()));
      continue;
    }
    if (child.op_get() != kind)
      return false;
  }
  return true;
}

bool args_node::apply(spot::formula f, spot::formula& res, str_map& ap_asso,
    bool last)
{
  for (unsigned i = 0; i < children_.size(); i++)
  {
    auto& child = children_[i];
    auto op = child->op_get();
    if ((op == spot::op::ap && is_special(child->opstr_get()))
        || f.kind() == op)
      if (child->apply(f, res, ap_asso, last))
      {
        // !res so we do not try to access condition_ after
        if (last && !res)
        {
          auto cond = *condition_;
          unsigned nb = cond.size();
          unsigned i = 0;
          for (; i < nb; i++)
          {
            cond[i].check(f, ap_asso);
          }
          if (i == nb)
            continue;
          // building res
          //auto cond = chg_ap_name(search->second.condf_get(), ap_asso);
          res = chg_ap_name(cond[i].retf_get(), ap_asso);
        }
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
    ofs << father << " -> " << nb << "\n";
  for (unsigned j = 0; j < children_.size(); j++)
  {
    auto& child = children_[j];
    child->to_dot(i, nb, ofs);
  }
}

bool op_node::insert(sim_line sl, str_map& ap_asso,
    bool last)
{
  auto f = sl.formula_get();
  auto ins = [&](unsigned child, pair_vect* vect = nullptr) -> bool
  {
    if (vect)
      for (auto pair : *vect)
        ap_asso.emplace(pair.first, pair.second);
    bool inserted = false;
    for (unsigned i = 0; i < f.size(); i++)
    {
      //do not insert duplicates ap when is_equivalent
      if (!vect || f[i].kind() != spot::op::ap)
        inserted |= children_[child]->insert(sim_line(sl.cond_get(), f[i],
              sl.replace_get()), ap_asso, last && i == f.size() - 1);
      else
        //insert cond
        if (last && i == f.size() - 1)
        {
          auto& cond = children_[child]->condition_get();
          if (!cond)
            cond = std::make_unique<std::vector<conds>>();
          cond->push_back(conds(sl.cond_get(),
                sl.replace_get(), ap_asso));
        }
    }
    return inserted;
  };
  for (unsigned i = 0; i < children_.size(); i++)
  {
    pair_vect vect;
    if (children_[i]->is_equivalent(f, vect, false))
      return ins(i, &vect);
  }
  children_.push_back(std::make_unique<args_node>());
  return ins(children_.size() - 1);
}
