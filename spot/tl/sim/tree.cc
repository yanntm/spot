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
