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



