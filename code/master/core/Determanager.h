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
/* importClauseSwitchMode : (Cooperation* coop) 
 Description :
 In detreministic case, the two barriers guaranty that during import process no other thread can return to search.
 Otherwise, each found a solution go out.*/
//=================================================================================================

using namespace Minisat;

lbool Solver::importClauses(Cooperation* coop) {
  
  //Control the limit size clause export
  coop->updateLimitExportClauses(this);


  switch(deterministic_mode){

  case 0:   // non deterministic case
    {
      for(int t = 0; t < coop->nThreads(); t++)
	if(coop->answer(t) != l_Undef)
	  return coop->answer(t);
      
      coop->importExtraClauses(this);
      coop->importExtraUnits(this, extraUnits);
      break;
    }
    
  case 1:  // deterministic case static frequency
    {
      if((int) conflicts % coop->initFreq == 0 || coop->answer(threadId) != l_Undef){						

#pragma omp barrier
	
	for(int t = 0; t < coop->nThreads(); t++)
	  if(coop->answer(t) != l_Undef) return coop->answer(t);
	
	coop->importExtraClauses(this);
	coop->importExtraUnits(this, extraUnits);
	
#pragma omp barrier
      }
      
      break;
    }

  case 2: // deterministic case dynamic frequency
    {
      if(((int) conflicts % coop->deterministic_freq[threadId] == 0) || (coop->answer(threadId) != l_Undef)){						coop->learntsz[threadId] = nLearnts();
#pragma omp barrier
	// each thread has its own frequency barrier synchronization
	updateFrequency(coop);

	coop->deterministic_freq[threadId] = updateFrequency(coop);
	
	for(int t = 0; t < coop->nThreads(); t++)
	  if(coop->answer(t) != l_Undef) return coop->answer(t);
	
	coop->importExtraClauses(this);
	coop->importExtraUnits(this, extraUnits);

#pragma omp barrier
      }
      
      break;
    }
  }
  return l_Undef;
}


/*_________________________________________________________________________________________________
 
updateFrequency : (Cooperation* coop) 
 Description :
 when det=2, each thread try to estimate the number of conflicts under which it must to join the barrier.
 This estimation based on the calculus of the number of learnts clauses of all learnts and assume that
 greater the learnts base slower is the unit propagation, which stay a not bad estimation.
 */
int Solver::updateFrequency(Cooperation* coop){
  
  double freq = 0;
  int maxLearnts = 0;
  
  for(int t = 0; t < coop->nThreads(); t++)
    if((int)coop->learntsz[t] > maxLearnts)
      maxLearnts = (int)coop->learntsz[t];
  
  freq = coop->initFreq + (double)coop->initFreq * (maxLearnts -learnts.size()) / maxLearnts;
  return (int) freq;
}



/*_________________________________________________________________________________________________
 
 uncheckedEnqueueImportedUnits : (Cooperation* coop) 
 Description :
 At level 0, units literals propaged are exported to others threads 
 */

void Solver::exportClause(Cooperation* coop, vec<Lit>& learnt_clause) {
  
  if(coop->limitszClauses() < 1) 
    return;
  
  if(decisionLevel() == 0){
    for(int i = tailUnitLit; i < trail.size(); i++)
      coop->exportExtraUnit(this, trail[i]) ;
    tailUnitLit = trail.size();
  }else
    coop->exportExtraClause(this, learnt_clause);
}


//=================================================================================================
// add Clauses received from others threads

CRef Solver::addExtraClause(vec<Lit>& lits){
  CRef cr = ca.alloc(lits, true);
  learnts.push(cr);
  attachClause(cr);
  claBumpActivity(ca[cr]);
  return cr;
}

//=================================================================================================
// at level 0, unit extra clauses stored are propagated

void Solver::propagateExtraUnits(){
  for(int i = 0; i < extraUnits.size(); i++)
    if(value(extraUnits[i]) == l_Undef)
      uncheckedEnqueue(extraUnits[i]);
}
