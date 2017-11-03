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
    std::cout << "'" << elem.first << "' '" << elem.second << "'" << std::endl;
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
  return f.size() == 0;
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

bool sim_tree::apply(spot::formula f, spot::formula& res, str_map& ap_asso)
{
  return root_->apply(f, res, ap_asso, true);
}

bool sim_tree::apply(spot::formula f, spot::formula& res)
{
  str_map map;
  return root_->apply(f, res, map, true);
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



bool args_node::is_equivalent(spot::formula f, pair_vect& asso)
{
  int nb = f.size();
  // handling 'args' -> pushing every n-ary OP children in the map
  if (is_nary(f))
    if (children_.size() == 1)
    {
      auto name = children_[0]->opstr_get();
      asso.emplace_back(str_psl(f), name);
      return true;
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
    if (op == spot::op::ap || f.kind() == op)
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
            if (cond[i].check(ap_asso))
              break;
          }
          if (i == nb)
            continue;
          //for (auto e : ap_asso)
            //std::cout << e.first << "->" << e.second << std::endl;
          // building res
          // auto cond = chg_ap_name(search->second.condf_get(), ap_asso);
          res = chg_ap_name(cond[i].retf_get(), ap_asso, true);
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
    if (children_[i]->is_equivalent(f, vect))
      return ins(i, &vect);
  }
  children_.push_back(std::make_unique<args_node>());
  return ins(children_.size() - 1);
}

bool op_node::apply(spot::formula f, spot::formula& res, str_map& ap_asso,
    bool last)
{
  bool ins;
  //std::cout << f.kindstr() << " " << (f.kind() == op_) << std::endl;
  if (op_ == spot::op::ap || is_final(f) && f.kind() == op_)
    return true;
  for (unsigned j = 0; j < children_.size(); j++)
  {
    auto& child = children_[j];
    pair_vect vect;
    if (child->is_equivalent(f, vect))
    {
      str_map new_asso = ap_asso;
      for (auto pair : vect)
        if (new_asso.find(opstr_) == new_asso.end())
          new_asso.emplace(pair.second, pair.first);
      //if (is_sorted(f))
      unsigned i = 0;
      for (; i < f.size(); i++)
      {
        //std::vector<bool> marked(f.size(), false);
        if (!child->apply(f[i], res, new_asso, last && i == f.size() - 1))
          break;
      }
      //std::cout << i << "/" << f.size() << std::endl;
      if (i == f.size())
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
     std::cout << "'" << elem.first << "' '" << elem.second << "'" << std::endl;
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
              std::cout << "\"" << s << "\" matches " << fp << std::endl;
            for (auto s : neg)
              std::cout << "\"" << s << "\" doesn't match " << fp << std::endl;
            */

            auto op = str(fn.kind());
            // seems important
            auto replace = [&name, &ptrn](str_vect& vect, splitmap map,
                std::string rep = "")
            {
              if (rep.size())
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
              auto search = map.find(std::make_pair(name,ptrn));
              if (search != map.end())
                rep = search->second;
              std::string split;
              if (vect.size())
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
              std::cout << p.first.first << "/" << p.first.second << "->"
                << p.second << std::endl;
            for (auto p : splitnot_)
              std::cout << p.first.first << "/" << p.first.second << "->"
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







