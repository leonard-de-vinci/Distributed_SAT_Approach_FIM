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
#include <bson/bson.h>
#include <mongoc/mongoc.h>

#include "common.h"
#include "json.h"

#include "../utils/System.h"
#include "../utils/ParseUtils.h"
#include "../utils/Options.h"
#include "Dimacs.h"
#include "Solver.h"


using namespace Minisat;
using namespace std;

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

static void stop (int sig) {
        run = 0;
        fclose(stdin); /* abort fgets() */
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
		
		gzclose(in);
        
        double parsed_time = cpuTime();
        if (coop.solvers[0].verbosity > 0){
            printf("|  Parse time:           %12.2f s                                                                                 |\n", parsed_time - initial_time);
            printf("|                                                                                                                       |\n"); 
        }

        //Mongo
        const char *mongo_config_file = "mongo.config";
        const char *key;
        char *uri_string;
        char *str;
        char temp[16] = {0};
        size_t keylen;
        uint32_t count = 0;
        mongoc_uri_t *uri;
        mongoc_client_t *client;
        mongoc_database_t *database;
        mongoc_collection_t *collection;
        bson_t *items, *tab_transactions, *appear_trans, *occ;
        bson_t child;
        bson_error_t error;
        bson_oid_t oid_items, oid_tab_transactions, oid_appear_trans, oid_occ;

        if (!(uri_string = mongo_config(mongo_config_file))){
            printf("Failed mongo config");
            return 1;
        }

        mongoc_init();

        uri = mongoc_uri_new_with_error(uri_string, &error);
        if (!uri){
            fprintf (stderr, "failed to parse URI: %s\nerror message: %s\n", uri_string, error.message);
            return EXIT_FAILURE;
        }

        client = mongoc_client_new_from_uri(uri);
        if (!client){
            return EXIT_FAILURE;
        }

        mongoc_client_set_appname (client, "database-push");

        database = mongoc_client_get_database(client, "data");
        collection = mongoc_client_get_collection (client, "data", "dataset");

        items = bson_new();
        bson_oid_init(&oid_items, NULL);
        BSON_APPEND_OID (items, "_id", &oid_items);

        //Append Sorted items
        BSON_APPEND_ARRAY_BEGIN(items, "items", &child);
        for(uint32_t i = 0; (int) i < coop.items.size(); i++){
            sprintf(temp, "%s%d", sign(coop.items[i]) ? "-" : "", var(coop.items[i]));
            keylen = bson_uint32_to_string(i, &key, temp, sizeof(temp));
            bson_append_utf8(&child, key, (int) keylen, temp, -1);
        }
        bson_append_array_end(items, &child);

        str = bson_as_canonical_extended_json(items, NULL);

        FILE* fichier = fopen("test.txt", "w");
        if(fichier != NULL){
            fprintf(fichier, "%s\n", str);
            fclose(fichier);
        }

        bson_free(str);

        if (!mongoc_collection_insert_one(collection, items, NULL, NULL, &error)){
            fprintf (stderr, "%s\n", error.message);
        }

        bson_destroy(items);

        printf("Items sent\n");



        tab_transactions = bson_new();
        bson_oid_init(&oid_tab_transactions, NULL);
        BSON_APPEND_OID (tab_transactions, "_id", &oid_tab_transactions);

        //Append Tab_transactions
        BSON_APPEND_ARRAY_BEGIN(tab_transactions, "tab_transactions", &child);
        for(int i = 0; i < coop.tabTransactions.size(); i++){
            for(int j = 0; j < coop.tabTransactions[i].size(); j++){
                count++;
                sprintf(temp, "%s%d", sign(coop.tabTransactions[i][j]) ? "-" :  "", var(coop.tabTransactions[i][j]));
                keylen = bson_uint32_to_string(count, &key, temp, sizeof(temp));
                bson_append_utf8(&child, key, (int) keylen, temp, -1);
            }
            count++;
            sprintf(temp, ";");
            keylen = bson_uint32_to_string(count, &key, temp, sizeof(temp));
            bson_append_utf8(&child, key, (int) keylen, temp, -1);
        }
        bson_append_array_end(tab_transactions, &child);
        count = 0;

        str = bson_as_canonical_extended_json(tab_transactions, NULL);

        fichier = fopen("test.txt", "a");
        if(fichier != NULL){
            fprintf(fichier, "%s\n", str);
            fclose(fichier);
        }

        bson_free(str);

        if (!mongoc_collection_insert_one(collection, tab_transactions, NULL, NULL, &error)){
            fprintf (stderr, "%s\n", error.message);
        }

        bson_destroy(tab_transactions);

        printf("Tab transctions sent\n");



        appear_trans = bson_new();
        bson_oid_init(&oid_appear_trans, NULL);
        BSON_APPEND_OID (appear_trans, "_id", &oid_appear_trans);
        
        //Append Appear_Trans
        BSON_APPEND_ARRAY_BEGIN(appear_trans, "appear_trans", &child);
        for(int i = 0; i < coop.appearTrans.size(); i++){
            for(int j = 0; j < coop.appearTrans[i].size(); j++){
                count++;
                sprintf(temp, "%d", coop.appearTrans[i][j]);
                keylen = bson_uint32_to_string(count, &key, temp, sizeof(temp));
                bson_append_utf8(&child, key, (int) keylen, temp, -1);
            }
            count++;
            sprintf(temp, ";");
            keylen = bson_uint32_to_string(count, &key, temp, sizeof(temp));
            bson_append_utf8(&child, key, (int) keylen, temp, -1);
        }
        bson_append_array_end(appear_trans, &child);

        str = bson_as_canonical_extended_json(appear_trans, NULL);

        fichier = fopen("test.txt", "a");
        if(fichier != NULL){
            fprintf(fichier, "%s\n", str);
            fclose(fichier);
        }

        bson_free(str);

        if (!mongoc_collection_insert_one(collection, appear_trans, NULL, NULL, &error)){
            fprintf (stderr, "%s\n", error.message);
        }

        bson_destroy(appear_trans);

        printf("Appear trans sent\n");



        occ = bson_new();
        bson_oid_init(&oid_occ, NULL);
        BSON_APPEND_OID (occ, "_id", &oid_occ);
        
        //Append Occ
        BSON_APPEND_ARRAY_BEGIN(occ, "occ", &child);
        for(uint32_t i = 0; (int) i < coop.occ.size(); i++){
            sprintf(temp, "%d", coop.occ[i]);
            keylen = bson_uint32_to_string(i, &key, temp, sizeof(temp));
            bson_append_utf8(&child, key, (int) keylen, temp, -1);
        }
        bson_append_array_end(occ, &child);

        str = bson_as_canonical_extended_json(occ, NULL);

        fichier = fopen("test.txt", "a");
        if(fichier != NULL){
            fprintf(fichier, "%s", str);
            fclose(fichier);
        }

        bson_free(str);

        if (!mongoc_collection_insert_one(collection, occ, NULL, NULL, &error)){
            fprintf (stderr, "%s\n", error.message);
        }

        bson_destroy(occ);

        printf("Occ sent\n");

        mongoc_collection_destroy(collection);
        mongoc_database_destroy(database);
        mongoc_client_destroy(client);
        mongoc_cleanup();

        return 1;

        //Kafka
        rd_kafka_t *rk; //producer instance handle
        rd_kafka_conf_t *conf; //temporary configuration object
        char errstr[512]; //librdkafka API error reporting buffer
        char buff[512]; //Message value temporary buffer

        const char *topic = "guiding_path";
        const char *config_file = "librdkafka.config";

        rd_kafka_resp_err_t err;
        size_t len;

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

        /* Signal handler for clean shutdown */
        signal(SIGINT, stop);

        create_topic(rk, topic, 1);

        //Guiding_Path

        for(int i = coop.div_begining; run && i < coop.items.size(); i++){
            sprintf(buff, "%d", i);
            len = strlen(buff);

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
            do{
                err = rd_kafka_producev(
                    /* Producer handle */
                    rk,
                    /* Topic name */
                    RD_KAFKA_V_TOPIC(topic),
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
                    fprintf(stderr, "%% Failed to produce to topic %s: %s\n", topic, rd_kafka_err2str(err));

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
                    }
                }
                else{
                    fprintf(stderr, "%% Enqueued message \"%s\" (%zd bytes) for topic %s\n", buff, len, topic);
                }
            }while(err == RD_KAFKA_RESP_ERR__QUEUE_FULL);


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

        fprintf(stderr, "%% Flushing final messages..\n");
        rd_kafka_flush(rk, 10*1000 /* wait for max 10 seconds */);

        /* If the output queue is still not empty there is an issue
         * with producing messages to the clusters. */
        if (rd_kafka_outq_len(rk) > 0)
            fprintf(stderr, "%% %d message(s) were not delivered\n", rd_kafka_outq_len(rk));

        /* Destroy the producer instance */
        rd_kafka_destroy(rk);

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
