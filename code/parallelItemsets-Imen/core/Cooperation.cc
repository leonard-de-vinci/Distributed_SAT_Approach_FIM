/*****************************************************************************************[Cooperation.h]
Copyright (c) 2008-20011, Youssef Hamadi, Saïd Jabbour and Lakhdar Saïs
 
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*******************************************************************************************/
#include "core/Cooperation.h"
#include "mtl/Sort.h"

namespace Minisat {


  /*_________________________________________________________________________________________________
    |
    |  addTableClause_ : ()  ->  [bool]
    |  Description : manage assumptions in a VecGuiding
    |________________________________________________________________________________________________@*/


bool Cooperation::addTransactions(vec<Lit>& ps)
{
  tabTransactions.push();
  int ind = tabTransactions.size()-1;
  for (int i = 0; i < ps.size(); i++)
    tabTransactions[ind].push(ps[i]);
  
  return true;
}


  struct reduceDB_lt {
    vec<int>&  occ;
    reduceDB_lt(vec<int>& occ_) : occ(occ_) {}
    bool operator () (Lit x, Lit y) {
      return occ[var(x)] < occ[var(y)]; }
  };


  struct reduceDB_lta {
    bool operator () (vec<Lit>& vec1, vec<Lit>& vec2) {
      int mini = vec1.size();
      if (vec2.size() < vec1.size())
	mini = vec2.size();
      for(int i = 0; i < mini; i++)
	if(var(vec1[i]) < var(vec2[i]))
	  return true;
	else if (var(vec2[i]) < var(vec1[i]))
	  return false;
      if(vec1.size() < vec2.size())
	return true;
      return false;}
  };
  

/*_________________________________________________________________________________________________
    |
    |  FillVecGuiding : ()  ->  [void]
    |  Description : manage assumptions in a VecGuiding
    |________________________________________________________________________________________________@*/

  void Cooperation::buildGuidingPaths(){

    vec<Lit> items;

    for(int i = 0; i < solvers[0].nVars(); i++){
      items.push(mkLit(i, false));
      correl.push();
    }
    
    for(int  i = 0; i < items.size(); i++){
      appearTrans.push();
      occ.push(0);
    }

    
    
    for( int i = 0; i < tabTransactions.size(); i++){
      for(int j = 0;j< tabTransactions[i].size(); j++){
	Var v = var(tabTransactions[i][j]);
	occ[v]++;
	appearTrans[v].push(i);
      }
    }


    sort(items, reduceDB_lt(occ));
    
    for(int t = 0; t < nbThreads; t++)
      items.copyTo(solvers[t].allItems);
    

    int nb = 0;
    for(int i = 0; i < items.size(); i++)
      if(occ[var(items[i])] == 0)
	nb++;


    int total = 0;
    int all_items = items.size();
    for(int  i = 0; i < items.size(); i++)
      if(occ[var(items[i])] < min_supp)
	all_items--;

    for( int i = 0; i < tabTransactions.size(); i++){
      int u = 0;
      for(int j = 0;j< tabTransactions[i].size(); j++){
        Var v = var(tabTransactions[i][j]);
        if (occ[v] >= min_supp)
	  u++;
      }
      total += 2*(all_items - u+1);
    }
    
    total += all_items;
  //  printf("total : %d \n", total);

    div_begining = 0;
    for(int  i = 0; i < items.size(); i++)
      if(occ[var(items[i])] < min_supp)
	div_begining++;
      else
	break;

    printf("----------------------------------------------------------------------------\n");
    printf("              |                  |--<>- items            : %10d      | \n",items.size());
    printf("   DataBase Description          |--<>- transactions     : %10d      |\n", tabTransactions.size());
    printf(" +++++++++++++++++++++++++     +-|-+                                       | \n");
    printf("                               +-|-+                                       | \n");
    printf("                                 |--<>-  infrequent items : %10d     |\n",div_begining);
    printf("                                 |--<>- min support      : %10d      |\n", min_supp);
    printf("                                 |                                         | \n");
    printf("--------------------------------------------------------------------------\n\n");
    

    //copy the sorted items to each threads
      for(int t = 0; t < nbThreads; t++){
	for(int i = 0; i < items.size(); i++)	
	  solvers[t].VecItems.push(var(items[i]));
	solvers[t].nbTrans = tabTransactions.size();
      }
      
    printf("<> start enumerating....\n");
  }
 
}
