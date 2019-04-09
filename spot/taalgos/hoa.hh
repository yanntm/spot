#pragma once

#include <spot/ta/ta.hh>
#include <iosfwd>

namespace spot
{
  SPOT_API std::ostream&
  print_hoa(std::ostream& os, const const_ta_ptr& a,
                  const char* opt = nullptr);
}
