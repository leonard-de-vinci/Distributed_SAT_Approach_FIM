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
#include <unistd.h>
#include <stdio.h>
#include <string>
#include <iostream>

#include <librdkafka/rdkafka.h>
#include <cppkafka/cppkafka.h>
#include <boost/program_options.hpp>

#include "../utils/System.h"
#include "../utils/ParseUtils.h"
#include "../utils/Options.h"
#include "Dimacs.h"
#include "Solver.h"


using namespace Minisat;
using namespace std;
using namespace cppkafka;

string convertToString(char* a){
    string s = "";
    int size = sizeof(a);
    for(int i = 0; i < size; i++){
        s += a[i];
    }
    return s;
}

static void on_delivery(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque){
    if (rkmessage->err)
        fprintf(stderr, "%% Message delivery failed: %s\n", rd_kafka_message_errstr(rkmessage));
}

void init_rd_kafka(){
  rd_kafka_conf_t *conf = rd_kafka_conf_new();
  rd_kafka_conf_set_dr_msg_cb(conf, on_delivery);

  // initialization omitted
}

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
		
	    Cooperation coop(1);
	    coop.min_supp = Freq;
	    coop.solvers[0].threadId = 0;
	    coop.solvers[0].verbosity = verb;

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
            } 
        }

        // Set limit on virtual memory:
        if (mem_lim != INT32_MAX){
            rlim_t new_mem_lim = (rlim_t)mem_lim * 1024*1024;
            rlimit rl;
            getrlimit(RLIMIT_AS, &rl);
            if (rl.rlim_max == RLIM_INFINITY || new_mem_lim < rl.rlim_max){
                rl.rlim_cur = new_mem_lim;
                if (setrlimit(RLIMIT_AS, &rl) == -1)
                    printf("WARNING! Could not set resource limit: Virtual memory.\n");
            }
        }
        
        if (argc == 1)
            printf("Reading from standard input... Use '--help' for help.\n");
        
        gzFile in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
        if (in == NULL)
            printf("ERROR! Could not open file: %s\n", argc == 1 ? "<stdin>" : argv[1]), exit(1);

        if (coop.solvers[0].verbosity > 0){
            printf(" ===============================================[ Problem Statistics ]==================================================\n");
            printf("|                                                                                                                       |\n");
            printf("|                                                                                                                       |\n");
        }
        
	    printf("<> instance    : %s\n", argv[1]);
	


	    parse_DIMACS(in, &coop);

        string items = "";
        string tabTransactions = "";
        string appearTrans = "";
        string div_begining = "";
        string occ = "";
        char temp[20] = {0};

        // Items
        for(int i = 0; i < coop.items.size(); i++){
            sprintf(temp, "%s%d,", sign(coop.items[i]) ? "-" : "", var(coop.items[i]));
            items += convertToString(temp);
        }

        // Tab Transactions
        for(int i  = 0; i < coop.tabTransactions.size(); i++){
            sprintf(temp, "[");
            tabTransactions += convertToString(temp);
            for(int j = 0; j < coop.tabTransactions[i].size(); j++){
                sprintf(temp, "%s%d,", sign(coop.tabTransactions[i][j]) ? "-" :  "", var(coop.tabTransactions[i][j]));
                tabTransactions += convertToString(temp);
            }
            sprintf(temp, "],");
            tabTransactions += convertToString(temp);
        }

        // Appear Trans
        for(int i = 0; i < coop.appearTrans.size(); i++){
            sprintf(temp, "[");
            appearTrans += convertToString(temp);
            for(int j = 0; j < coop.appearTrans[i].size(); j++){
                sprintf(temp, "%d,", coop.appearTrans[i][j]);
                appearTrans += convertToString(temp);
            }
            sprintf(temp, "],");
            appearTrans += convertToString(temp);
        }

        // Div begining
        sprintf(temp, "%d", coop.div_begining);
        div_begining += convertToString(temp);

        // Occ
        for(int i = 0; i < coop.occ.size(); i++){
            sprintf(temp, "%d,", coop.occ[i]);
            occ += convertToString(temp);
        }
		
		gzclose(in);
        
        double parsed_time = cpuTime();
        if (coop.solvers[0].verbosity > 0){
            printf("|  Parse time:           %12.2f s                                                                                 |\n", parsed_time - initial_time);
            printf("|                                                                                                                       |\n"); 
        }



        //Kafka

        char hostname[128];
        char errstr[512];

        rd_kafka_conf_t *conf = rd_kafka_conf_new();

        if (gethostname(hostname, sizeof(hostname))){
            fprintf(stderr, "%% Failed to lookup hostname\n");
            exit(1);
        }

        if (rd_kafka_conf_set(conf, "client.id", hostname, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK){
            fprintf(stderr, "%% %s\n", errstr);
            exit(1);
        }

        if (rd_kafka_conf_set(conf, "bootstrap.servers", "host1:9092,host2:9092", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK){
            fprintf(stderr, "%% %s\n", errstr);
            exit(1);
        }

        rd_kafka_topic_conf_t *topic_conf = rd_kafka_topic_conf_new();

        if (rd_kafka_topic_conf_set(topic_conf, "acks", "all", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK){
            fprintf(stderr, "%% %s\n", errstr);
            exit(1);
        }

        /* Create Kafka producer handle */
        rd_kafka_t *rk;
        if (!(rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr)))) {
            fprintf(stderr, "%% Failed to create new producer: %s\n", errstr);
            exit(1);
        }

        rd_kafka_topic_t *rkt = rd_kafka_topic_new(rk, topic, topic_conf);

        if (rd_kafka_produce(rkt, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY, payload, payload_len, key, key_len, NULL) == -1){
            fprintf(stderr, "%% Failed to produce to topic %s: %s\n", topic, rd_kafka_err2str(rd_kafka_errno2err(errno)));
        }

        
        
        // Configuration config{
        //     {"metadata.broker.list", "localhost:29092"}
        // };

        // Producer producer(config);

        // string message = "Hello there";
        // producer.produce(MessageBuilder("mytopic").partition(0).payload(message));
        // producer.flush();

        vec<Lit> dummy;
		lbool ret;
	    lbool result;

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
