#include <spot/twacube_algos/scc.hh>

namespace spot 
{
/**************************************************************************************************************************
* Function Name - calculate_scc(twacube_ptr aut)
* Class: scc_cube
* Description - calculate number of SCC in a automata
* Return - void
* Date - Jun 29 2017
* ************************************************************************************************************************/
  

  unsigned scc_cube::calculate_scc(twacube_ptr aut) 
  {
    
    /*get number of states to allocate memory */
    vertex_ = aut->get_graph().num_states();
    /*array with states that have been visited */
    checked_ =   new bool[vertex_](); 
    /*perform dfs from initial state*/
    dfs (aut->get_initial() ,aut);
    
    delete[] (checked_); 
    
    return uf.count_root();


  }

  /**************************************************************************************************************************
   * Function Name - void dfs (unsigned v, twacube_ptr aut)
   * Class - dfs
   * Description - does DFS search of the graph
   * Return - void
   * Date - Jun 29 2017
   * ************************************************************************************************************************/
  void scc_cube::dfs(unsigned v, twacube_ptr aut) 
  {
    /*Mark state as visited*/
    checked_[v] = true;
    /*push to root stack*/
    temp_stack.push(v);
    /*push to DFS stack*/
    stack.push(v);
    /*add state to union find*/
    uf.makeset(v);
    /*check all reachable states from v*/
    for (auto it = aut->succ(v); !it->done ();it->next())
    {
      unsigned w =  aut->trans_storage(it).dst;
      /*If state is not visited perform DFS on the it*/
      if (!checked_[w])
        dfs(w,aut);
      /*If state is not part of a SCC already pop stack*/  
      else if (!uf.isdead(w))
      {
        while (!uf.sameset(stack.top() , w ) )
        { 
          uf.unite ( w , stack.top() );
          stack.pop();
        }
      }
    }
    if (stack.top() == v) 
    {
      /*mark dead and form SCC*/
      uf.markdead(v);
      stack.pop();
      unsigned w;
      do 
      {
        w = temp_stack.top();  
        temp_stack.pop();
      } while (w != v);  
    }                       



  } 


}
