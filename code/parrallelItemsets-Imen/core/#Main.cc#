/*****************************************************************************************[Main.cc]
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
#include <omp.h>

#include <errno.h>

#include <signal.h>
#include <zlib.h>

#include "utils/System.h"
#include "utils/ParseUtils.h"
#include "utils/Options.h"
#include "core/Dimacs.h"
#include "core/Solver.h"


using namespace Minisat;

//=================================================================================================


void printStats(Solver& solver)
{
    double cpu_time = cpuTime();
    //double mem_used = memUsedPeak();
    printf("  ----------------------------------------         \n");
    printf("   winner:  more statistics           \n");
    printf("  ----------------------------------------         \n");
    printf("  restarts              : %"PRIu64"\n", solver.starts);
    printf("  conflicts             : %-12"PRIu64"   (%.0f /sec)\n", solver.conflicts   , solver.conflicts   /cpu_time);
    printf("  decisions             : %-12"PRIu64"   (%4.2f %% random) (%.0f /sec)\n", solver.decisions, (float)solver.rnd_decisions*100 / (float)solver.decisions, solver.decisions   /cpu_time);
    printf("  propagations          : %-12"PRIu64"   (%.0f /sec)\n", solver.propagations, solver.propagations/cpu_time);
    printf("  conflict literals     : %-12"PRIu64"   (%4.2f %% deleted)\n", solver.tot_literals, (solver.max_literals - solver.tot_literals)*100 / (double)solver.max_literals);
    //if (mem_used != 0) printf("  Memory used           : %.2f MB\n", mem_used);
    printf("  CPU time              : %g s\n", cpu_time);
}


static Solver* solver;
// Terminate by notifying the solver and back out gracefully. This is mainly to have a test-case
// for this feature of the Solver as it may take longer than an immediate call to '_exit()'.
static void SIGINT_interrupt(int signum) { solver->interrupt(); }

// Note that '_exit()' rather than 'exit()' has to be used. The reason is that 'exit()' calls
// destructors and may cause deadlocks if a malloc/free function happens to be running (these
// functions are guarded by locks for multithreaded use).
static void SIGINT_exit(int signum) {
    printf("\n"); printf("*** INTERRUPTED ***\n");
    if (solver->verbosity > 0){
        printStats(*solver);
        printf("\n"); printf("*** INTERRUPTED ***\n"); }
    _exit(1); }


//=================================================================================================
// Main:


int main(int argc, char** argv)
{
    try {
        setUsageHelp("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n");
        // printf("This is MiniSat 2.0 beta\n");
        
#if defined(__linux__)
        fpu_control_t oldcw, newcw;
        _FPU_GETCW(oldcw); newcw = (oldcw & ~_FPU_EXTENDED) | _FPU_DOUBLE; _FPU_SETCW(newcw);
        printf("WARNING: for repeatability, setting FPU to use double precision\n");
#endif
        // Extra options:
        //
        IntOption    verb   ("MAIN", "verb",   "Verbosity level (0=silent, 1=some, 2=more).", 1, IntRange(0, 3));
        IntOption    cpu_lim("MAIN", "cpu-lim","Limit on CPU time allowed in seconds.\n", INT32_MAX, IntRange(0, INT32_MAX));
        IntOption    mem_lim("MAIN", "mem-lim","Limit on memory usage in megabytes.\n", INT32_MAX, IntRange(0, INT32_MAX));

	IntOption    ncores ("MAIN", "ncores","# threads.\n", 1,  IntRange(0, INT32_MAX));//IntRange(1, omp_get_num_procs()));
	IntOption    limitEx("MAIN", "limitEx","Limit size clause exchange.\n", 10, IntRange(0, INT32_MAX));
	IntOption    ctrl   ("MAIN", "ctrl","Dynamic control clause sharing with 2 modes.\n", 0, IntRange(0, 2));
	IntOption    Freq    ("MAIN", "minSupport","# ....\n", 10,  IntRange(1, 100000000));//IntRange(1, omp_get_num_procs()));
	
        parseOptions(argc, argv, true);

	double initial_time = cpuTime();

		
	int nbThreads   = ncores;
	int limitExport = limitEx;	
	Cooperation coop(nbThreads, limitExport);
	
	coop.ctrl = ctrl;
	coop.min_supp = Freq;
	


	for(int t = 0; t < nbThreads; t++){
	  coop.solvers[t].threadId = t;
	  coop.solvers[t].verbosity = verb;
	}
	


	printf(" -----------------------------------------------------------------------------------------------------------------------\n");
	printf("|                                 PSATMiner    %i thread(s) on %i core(s)                                                |\n", coop.nbThreads, omp_get_num_procs()); 
	printf(" -----------------------------------------------------------------------------------------------------------------------\n");



		
        // Use signal handlers that forcibly quit until the solver will be able to respond to
        // interrupts:
        signal(SIGINT, SIGINT_exit);
        signal(SIGXCPU,SIGINT_exit);

        // Set limit on CPU-time:
        if (cpu_lim != INT32_MAX){
            rlimit rl;
            getrlimit(RLIMIT_CPU, &rl);
            if (rl.rlim_max == RLIM_INFINITY || (rlim_t)cpu_lim < rl.rlim_max){
                rl.rlim_cur = cpu_lim;
                if (setrlimit(RLIMIT_CPU, &rl) == -1)
                    printf("WARNING! Could not set resource limit: CPU-time.\n");
            } }

        // Set limit on virtual memory:
        if (mem_lim != INT32_MAX){
            rlim_t new_mem_lim = (rlim_t)mem_lim * 1024*1024;
            rlimit rl;
            getrlimit(RLIMIT_AS, &rl);
            if (rl.rlim_max == RLIM_INFINITY || new_mem_lim < rl.rlim_max){
                rl.rlim_cur = new_mem_lim;
                if (setrlimit(RLIMIT_AS, &rl) == -1)
                    printf("WARNING! Could not set resource limit: Virtual memory.\n");
            } }
        
        if (argc == 1)
            printf("Reading from standard input... Use '--help' for help.\n");
        
        gzFile in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
        if (in == NULL)
            printf("ERROR! Could not open file: %s\n", argc == 1 ? "<stdin>" : argv[1]), exit(1);
	
	

        if (coop.solvers[0].verbosity > 0){
            printf(" ===============================================[ Problem Statistics ]==================================================\n");
            printf("|                                                                                                                       |\n"); }
        
        //parse_DIMACS(in, &S);
	printf("<> minSupport  : %d \n", coop.min_supp);
	printf("<> instance    : %s\n", argv[1]);
	printf("<> nbThreads   : %d \n\n", nbThreads);
	
	omp_set_num_threads(nbThreads);
	parse_DIMACS(in, &coop);
		
		gzclose(in);
        FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : NULL;
        
        if (coop.solvers[0].verbosity > 0){
	  printf("|  Number of cores:      %12d                                                                                   |\n", coop.nbThreads); 
	  printf("|  Number of variables:  %12d                                                                                   |\n", coop.solvers[0].nVars());
	  printf("|  Number of clauses:    %12d                                                                                   |\n", coop.solvers[0].nClauses()); 

}
        
        double parsed_time = cpuTime();
        if (coop.solvers[0].verbosity > 0){
            printf("|  Parse time:           %12.2f s                                                                                 |\n", parsed_time - initial_time);
            printf("|                                                                                                                       |\n"); }
 

        // Change to signal-handlers that will only notify the solver and allow it to terminate
        // voluntarily:
        signal(SIGINT, SIGINT_interrupt);
        signal(SIGXCPU,SIGINT_interrupt);
       
 

	if (coop.solvers[0].verbosity > 0){
	  printf(" ==============================================[ Search Statistics ]====================================================\n");
	  printf("|   Thread  | Conflicts |          ORIGINAL         |          LEARNT          |  exported  / imported      |  Progress |\n");
	  printf("|    (id)   |           |    Vars  Clauses Literals |    Limit  Clauses Lit/Cl |  (clauses by restart)      |           |\n");
	  printf(" =======================================================================================================================\n");
    }


	if (!coop.solvers[0].simplify()){
	  if (res != NULL) fprintf(res, "UNSAT\n"), fclose(res);
	  if (coop.solvers[0].verbosity > 0){
	    printf("========================================================================================================================\n");
	    printf("Solved by unit propagation\n");
	    printStats(coop.solvers[0]);
	    printf("\n"); }
	  printf("UNSATISFIABLE\n");
	  exit(20);
        }
        
        vec<Lit> dummy;
		
	
		
	int winner = 0;
	lbool ret;
	lbool result;
	
	// launch threads in Parallel 	

	
#pragma omp parallel
	{
	  int t = omp_get_thread_num();
	  coop.start = true;
	  coop.solvers[t].EncodeDB(&coop);
	  ret = coop.solvers[t].solve_(&coop);
	}
	
	// select winner threads with respect to deterministic mode 
	for(int t = 0; t < coop.nThreads(); t++)
	  if(coop.answer(t) != l_Undef){
	    winner = t;
	    result = coop.answer(t);
	    break;
	  }

	int cpt = 0;
	// each worker print its models
	printf("-----------------------------------------------\n");
	printf("thread | nb models          | nb conflicts    |\n");
	printf("-----------------------------------------------\n");

	int nbcls = 0;
	for(int t = 0; t < coop.nThreads(); t++){
	  cpt +=  coop.solvers[t].nbModels;
	  nbcls += coop.solvers[t].nbClauses;
	  printf("  %2d   |   %15d  | %d \n", t, coop.solvers[t].nbModels, (int)coop.solvers[t].conflicts);
	  //coop.solvers[t].printModels();
	}
	printf("-----------------------------------------------\n");
	printf("total  | %15d    | \n", cpt);
	printf("-----------------------------------------------\n");
	
	printf("#total Clauses  : %15d     \n", nbcls);
	//coop.printStats(coop.solvers[winner].threadId);
	//printStats(coop.solvers[winner]);
	
	if (coop.solvers[winner].verbosity > 0){
	  //printStats(coop.solvers[winner]);
            printf("\n"); }
        printf(result == l_True ? "SATISFIABLE\n" : result == l_False ? "UNSATISFIABLE\n" : "INDETERMINATE\n");
        if (res != NULL){
            if (result == l_True){
	      fprintf(res, "SAT\n");
                for (int i = 0; i < coop.solvers[winner].nVars(); i++)
                    if (coop.solvers[winner].model[i] != l_Undef)
                        fprintf(res, "%s%s%d", (i==0)?"":" ", (coop.solvers[winner].model[i]==l_True)?"":"-", i+1);
                fprintf(res, " 0\n");
            }else if (result == l_False)
                fprintf(res, "UNSAT\n");
            else
                fprintf(res, "INDET\n");
            fclose(res);
        }
        
#ifdef NDEBUG
        exit(result == l_True ? 10 : result == l_False ? 20 : 0);     // (faster than "return", which will invoke the destructor for 'Solver')
#else
        return (result == l_True ? 10 : result == l_False ? 20 : 0);
#endif
    } catch (OutOfMemoryException&){
        printf("===============================================================================\n");
        printf("INDETERMINATE\n");
        exit(0);
    }
}
