/****************************************************************************************[Dimacs.h]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson

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
**************************************************************************************************/

#ifndef Minisat_Dimacs_h
#define Minisat_Dimacs_h

#include <stdio.h>

#include "Cooperation.h"
#include "../utils/ParseUtils.h"
#include "SolverTypes.h"

namespace Minisat {

//=================================================================================================
// DIMACS Parser:

template<class B, class Cooperation>
static void readClause(B& in, Cooperation* coop, vec<Lit>& lits) {
    int     parsed_lit, var;
    lits.clear();
    for (;;){
        parsed_lit = parseInt(in);
        if (parsed_lit == 0) break;
        var = abs(parsed_lit)-1;
	int initvar = var;
	for(int t = 0; t < coop->nbThreads; t++){
	  var = initvar;	
	  while (var >= coop->solvers[t].nVars()) coop->solvers[t].newVar(); //pb ptet ici aussi
	}
	lits.push( (parsed_lit > 0) ? mkLit(var, false) : ~mkLit(var, false) );
    }
}
 
template<class B, class Cooperation>
static void parse_DIMACS_main(B& in, Cooperation* coop) {
    vec<Lit> lits;
    int vars    = 0;
    int clauses = 0;
    int cnt     = 0;
    for (;;){
        skipWhitespace(in);
        if (*in == EOF) break;
        else if (*in == 'p'){
            if (eagerMatch(in, "p cnf")){
                vars    = parseInt(in);
                clauses = parseInt(in);
            }else{
                printf("PARSE ERROR! Unexpected char: %c\n", *in), exit(3);
            }
        } else if (*in == 'c' || *in == 'p')
            skipLine(in);
        else{
            cnt++;
            readClause(in, coop, lits);
	    coop->addTransactions(lits);
	}
	
    }

    coop->buildGuidingPaths();
    
    
 //   if (vars != coop->solvers[0].nVars())
     //   fprintf(stderr, "WARNING! DIMACS header mismatch: wrong number of variables.\n");
  //  if (cnt  != clauses)
    //    fprintf(stderr, "WARNING! DIMACS header mismatch: wrong number of clauses.\n");
}

// Inserts problem into solver.
//
template<class Cooperation>
static void parse_DIMACS(gzFile input_stream, Cooperation* coop) {
    StreamBuffer in(input_stream);
    parse_DIMACS_main(in, coop); }

//=================================================================================================
}

#endif
