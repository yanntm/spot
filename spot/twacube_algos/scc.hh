#include <bddx.h>
#include <iostream>
#include <spot/twacube/twacube.hh>
#include <spot/mc/unionfind.hh>
#include <stack>




namespace spot 
{
class SPOT_API scc_cube
{
  
   public:
   unsigned calculate_scc(twacube_ptr aut); 
    void dfs(unsigned v, twacube_ptr aut); 
    int_unionfind uf;

   private:
    unsigned vertex_;    
    std::stack<unsigned> temp_stack;
    std::stack<unsigned> stack;
    bool *checked_; 
 
   };


}
