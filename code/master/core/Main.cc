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

#include "common.h"
#include "json.h"

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

static void dr_msg_cb (rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque) {
        if (rkmessage->err)
            fprintf(stderr, "%% Message delivery failed: %s\n", rd_kafka_err2str(rkmessage->err));
        else
            fprintf(stderr, "%% Message delivered (%zd bytes, partition %"PRId32")\n", rkmessage->len, rkmessage->partition);

        /* The rkmessage is destroyed automatically by librdkafka */
}

static int run_producer (const char *topic, int msgcnt, rd_kafka_conf_t *conf){
    rd_kafka_t *rk;
    char errstr[512];
    int delivery_counter = 0;

    /* Set up a delivery report callback that will be triggered
     * from poll() or flush() for the final delivery status of
     * each message produced. */
    rd_kafka_conf_set_dr_msg_cb(conf, dr_cb);


    /* Create producer.
     * A successful call assumes ownership of \p conf. */
    rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!rk){
        fprintf(stderr, "Failed to create producer: %s\n", errstr);
        rd_kafka_conf_destroy(conf);
        return -1;
    }

    /* Create the topic. */
    if (create_topic(rk, topic, 1) == -1){
        rd_kafka_destroy(rk);
        return -1;
    }

    /* Produce messages */
    for (int i = 0 ; run && i < msgcnt ; i++){
        const char *user = "alice";
        char json[64];
        rd_kafka_resp_err_t err;

        snprintf(json, sizeof(json), "{ \"count\": %d }", i+1);

        fprintf(stderr, "Producing message #%d to %s: %s=%s\n", i, topic, user, json);

        /* Asynchronous produce */
        err = rd_kafka_producev(
            rk,
            RD_KAFKA_V_TOPIC(topic),
            RD_KAFKA_V_KEY(user, strlen(user)),
            RD_KAFKA_V_VALUE(json, strlen(json)),
            /* producev() will make a copy of the message
             * value (the key is always copied), so we
             * can reuse the same json buffer on the
             * next iteration. */
            RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
            RD_KAFKA_V_OPAQUE(&delivery_counter),
            RD_KAFKA_V_END);
        if (err){
            fprintf(stderr, "Produce failed: %s\n", rd_kafka_err2str(err));
            break;
        }

        /* Poll for delivery report callbacks to know the final
         * delivery status of previously produced messages. */
        rd_kafka_poll(rk, 0);
    }

    if (run){
        /* Wait for outstanding messages to be delivered,
         * unless user is terminating the application. */
        fprintf(stderr, "Waiting for %d more delivery results\n", msgcnt - delivery_counter);
        rd_kafka_flush(rk, 15*1000);
    }

    /* Destroy the producer instance. */
    rd_kafka_destroy(rk);

    fprintf(stderr, "%d/%d messages delivered\n", delivery_counter, msgcnt);

    return 0;
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
        rd_kafka_t *rk; //producer instance handle
        rd_kafka_conf_t *conf; //temporary configuration object
        char errstr[512]; //librdkafka API error reporting buffer
        char buff[512]; //Message value temporary buffer

        const char *topic1 = "items";
        const char *topic2 = "tab_transactions";
        const char *topic3 = "appear _rans";
        const char *topic4 = "div_begining";
        const char *topic5 = "occ";

        const char *config_file = "librdkafka.config";

        // Sets the boostraps servers to the ones indicated in this configuration file
        if (!(conf = read_config(config_file)))
                return 1;

        /* Set the delivery report callback.
         * This callback will be called once per message to inform
         * the application if delivery succeeded or failed.
         * See dr_msg_cb() above.
         * The callback is only triggered from rd_kafka_poll() and
         * rd_kafka_flush(). */
        rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);

        /*
         * Create producer instance.
         *
         * NOTE: rd_kafka_new() takes ownership of the conf object
         *       and the application must not reference it again after
         *       this call.
         */
        if (!(rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr)))){
            fprintf(stderr, "%% Failed to create new producer: %s\n", errstr);
            exit(1);
        }

        //
        create_topic(rk, topic1, 1);
        for(int i = 0; run && i < items.size(); i++){
            rd_kafka_resp_err_t err;
            sprintf(buff, "%s%d", sign(coop.items[i]) ? "-" : "", var(coop.items[i]));
            size_t len = strlen(buff)

            /*
             * Send/Produce message.
             * This is an asynchronous call, on success it will only
             * enqueue the message on the internal producer queue.
             * The actual delivery attempts to the broker are handled
             * by background threads.
             * The previously registered delivery report callback
             * (dr_msg_cb) is used to signal back to the application
             * when the message has been delivered (or failed).
             */
            retry:
                err = rd_kafka_producev(
                    /* Producer handle */
                    rk,
                    /* Topic name */
                    RD_KAFKA_V_TOPIC(topic1),
                    /* Make a copy of the payload. */
                    RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                    /* Message value and length */
                    RD_KAFKA_V_VALUE(buff, len),
                    /* Per-Message opaque, provided in
                     * delivery report callback as
                     * msg_opaque. */
                    RD_KAFKA_V_OPAQUE(NULL),
                    /* End sentinel */
                    RD_KAFKA_V_END);

                if (err){
                    /*
                     * Failed to *enqueue* message for producing.
                     */
                    fprintf(stderr, "%% Failed to produce to topic %s: %s\n", topic1, rd_kafka_err2str(err));

                    if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL) {
                        /* If the internal queue is full, wait for
                         * messages to be delivered and then retry.
                         * The internal queue represents both
                         * messages to be sent and messages that have
                         * been sent or failed, awaiting their
                         * delivery report callback to be called.
                         *
                         * The internal queue is limited by the
                         * configuration property
                         * queue.buffering.max.messages */
                        rd_kafka_poll(rk, 1000/*block for max 1000ms*/);
                        goto retry;
                    }
                }else{
                    fprintf(stderr, "%% Enqueued message (%zd bytes) for topic %s\n", len, topic);
                }


                /* A producer application should continually serve
                 * the delivery report queue by calling rd_kafka_poll()
                 * at frequent intervals.
                 * Either put the poll call in your main loop, or in a
                 * dedicated thread, or call it after every
                 * rd_kafka_produce() call.
                 * Just make sure that rd_kafka_poll() is still called
                 * during periods where you are not producing any messages
                 * to make sure previously produced messages have their
                 * delivery report callback served (and any other callbacks
                 * you register). */
                rd_kafka_poll(rk, 0/*non-blocking*/);
        }

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
