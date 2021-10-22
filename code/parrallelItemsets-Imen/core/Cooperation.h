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

#include "SolverTypes.h"
#include "Solver.h"


namespace Minisat {

/*=================================================================================================
 Cooperation Class : ->  [class]
Description:
	This class manage clause sharing component between threads,i.e.,
=================================================================================================*/
  
  class Cooperation{
    
  public:
    bool		start, end;				// start and end multi-threads search
    int	        nbThreads;				// numbe of  threads
    int                div_begining;
    Solver*		solvers;				// set of running Enumerative DPLL algorithms	
    lbool*		answers;				// answer of threads    
    int                 min_supp;
    vec<vec<Lit> >      VecGuiding;
    vec<vec<Lit> >      tabTransactions;       // List of transactions
    vec<vec<int> >      appearTrans;
    vec<int> occ;
    vec<vec<Lit> >      correl;
    
    //=================================================================================================
 
    void printStats			 (int& id);
 
    void Parallel_Info			  ();
    void buildGuidingPaths              ();
    bool addTransactions                (vec<Lit>& ps);
	
    void LaunchSolvers                  ();

    
    //=================================================================================================
    inline int nThreads      ()			{return nbThreads;		}

    //=================================================================================================
    // Constructor / Destructor 
    
    Cooperation(int n){
      
      nbThreads	= n;
      solvers	            = new Solver    [nbThreads];
      answers	            = new lbool     [nbThreads];		
      
	}     
    //=================================================================================================
    ~Cooperation(){}				
  };
}



		
		
