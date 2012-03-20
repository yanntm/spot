#include <iostream>
#include <cassert>

#include "tgba/tgbaexplicit2.hh"

using namespace spot;

int
main (int argc, char** argv)
{
  (void) argc;
  (void) argv;

 bdd_dict* d = new spot::bdd_dict();
 tgba_explicit<state_explicit_string>* tgba =
   new tgba_explicit<state_explicit_string> (d);
 state_explicit_string* s1 = tgba->add_state ("toto");
 state_explicit_string* s2 = tgba->add_state ("tata");
 state_explicit_string::transition* t =
   tgba->create_transition (s1, s2);
 (void) t;

  tgba_explicit_succ_iterator<state_explicit_string>* it
    = tgba->succ_iter (tgba->get_init_state ());
  for (it->first (); !it->done();it->next ())
  {
    state_explicit_string* se = it->current_state ();
    std::cout << (se)->label () << std::endl;
  }
}
