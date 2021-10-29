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
#include <time.h>

#include "../utils/System.h"
#include "../utils/ParseUtils.h"
#include "../utils/Options.h"
#include "Dimacs.h"
#include "Solver.h"


using namespace Minisat;


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
        
        IntOption    verb   ("MAIN", "verb",   "Verbosity level (0=silent, 1=some, 2=more).", 1, IntRange(0, 3));
        IntOption    cpu_lim("MAIN", "cpu-lim","Limit on CPU time allowed in seconds.\n", INT32_MAX, IntRange(0, INT32_MAX));
        IntOption    mem_lim("MAIN", "mem-lim","Limit on memory usage in megabytes.\n", INT32_MAX, IntRange(0, INT32_MAX));
	IntOption    ncores ("MAIN", "ncores","# threads.\n", 1,  IntRange(0, INT32_MAX));//IntRange(1, omp_get_num_procs()));
	IntOption    Freq    ("MAIN", "minSupport","# ....\n", 10,  IntRange(1, 100000000));//IntRange(1, omp_get_num_procs()));
	
        parseOptions(argc, argv, true);

	double initial_time = cpuTime();

		
	int nbThreads   = ncores;	
	Cooperation coop(nbThreads);
	
	coop.min_supp = Freq;
	


	for(int t = 0; t < nbThreads; t++){
	  coop.solvers[t].threadId = t;
	  coop.solvers[t].verbosity = verb;
	}

	FILE* file = NULL;
	file = fopen("models.txt", "w");
	fprintf(file, "Results - Models\n\n");
	fclose(file);

	printf(" -----------------------------------------------------------------------------------------------------------------------\n");
	printf("|                                 PSATMiner    %i thread(s) on %i core(s)                                                |\n", coop.nbThreads, omp_get_num_procs()); 
	printf(" -----------------------------------------------------------------------------------------------------------------------\n");



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
            printf("|                                                                                                                       |\n");
            printf("|                                                                                                                       |\n"); }
        

	printf("<> instance    : %s\n", argv[1]);
	printf("<> nbThreads   : %d \n\n", nbThreads);
	
	omp_set_num_threads(nbThreads);
	parse_DIMACS(in, &coop);
		
		gzclose(in);
        FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : NULL;
        
        if (coop.solvers[0].verbosity > 0){
	//  printf("|  Number of variables:  %12d                                                                                   |\n", coop.solvers[0].nVars());

}
        
        double parsed_time = cpuTime();
        if (coop.solvers[0].verbosity > 0){
            printf("|  Parse time:           %12.2f s                                                                                 |\n", parsed_time - initial_time);
            printf("|                                                                                                                       |\n"); }
 



	if (!coop.solvers[0].simplify()){
	  if (res != NULL) fprintf(res, "UNSAT\n"), fclose(res);
	  if (coop.solvers[0].verbosity > 0){
	    printf("========================================================================================================================\n");
	    printf("Solved by unit propagation\n");
	    printf("\n"); }
	  printf("UNSATISFIABLE\n");
	  exit(20);
        }
        
        vec<Lit> dummy;
		
	
		
	lbool ret;
	lbool result;
	double time_elapsed = 0.0;
	clock_t begin = clock();
	
	// launch threads in Parallel 	

#pragma omp parallel
	{
	  int t = omp_get_thread_num();
	  coop.start = true;
	  coop.solvers[t].EncodeDB(&coop);
	  ret = coop.solvers[t].solve_(&coop);
	}

	clock_t end = clock();
	time_elapsed += (double) (end - begin) / CLOCKS_PER_SEC * 1000.0;

	printf("time elapsed: %f\n", time_elapsed);
	
	
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
	  //printf("-----------------------------------------------\n");
	  //coop.solvers[t].AfficheModel();
	}
	printf("-----------------------------------------------\n");
	printf("total  | %15d    | \n", cpt);
	printf("-----------------------------------------------\n");
	
	printf("#total Clauses  : %15d     \n", nbcls);
       
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
