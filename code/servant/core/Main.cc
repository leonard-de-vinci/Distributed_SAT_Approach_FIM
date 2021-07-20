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
#include <fstream>
#include <iostream>
#include <ostream>
#include <stdio.h>
#include <unistd.h>
#include <string>

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


static void stop (int sig){
        run = 0;
}

static int is_printable (const char *buf, size_t size){
        for (size_t i = 0 ; i < size ; i++)
            if (!isprint((int)buf[i]))
                return 0;

        return 1;
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

		// Mongo
		const char *mongo_config_file = "mongo.config";
		const bson_t *document;
        char *uri_string;
        char *username;
        char *password;
        char *str;
        mongoc_uri_t *uri;
        mongoc_client_t *client;
        mongoc_database_t *database;
        mongoc_collection_t *collection;
		mongoc_cursor_t *cursor;
        bson_t *query;
        bson_error_t error;

		if (mongo_config(mongo_config_file, &uri_string, &username, &password) != 0){
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
		query = bson_new();
		cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);

		while(mongoc_cursor_next(cursor, &document)){
			str = bson_as_canonical_extended_json(document, NULL);
			FILE* fichier = fopen("test.txt", "w");
			if(fichier != NULL){
				fprintf(fichier, "%s", str);
				fclose(fichier);
			}
			bson_free(str);
		}

		bson_destroy(query);
		mongoc_cursor_destroy(cursor);
		mongoc_collection_destroy(collection);
		mongoc_database_destroy(database);
        mongoc_client_destroy(client);
        mongoc_cleanup();

		return 1;


		// Kafka
		rd_kafka_t *rk; //consumer instance handle
        rd_kafka_conf_t *conf; //temporary configuration object
		rd_kafka_resp_err_t err; //librdkafka API error code
        char errstr[512]; //librdkafka API error reporting buffer

        const char *topics = "guiding_path";           /* Argument: list of topics to subscribe to */
        int topic_cnt = 1;           /* Number of topics to subscribe to */
        rd_kafka_topic_partition_list_t *subscription; /* Subscribed topics */

		const char *config_file = "librdkafka.config";

		// Sets the boostraps servers to the ones indicated in this configuration file
        if (!(conf = read_config(config_file)))
                return 1;
		
		/*
         * Create consumer instance.
         *
         * NOTE: rd_kafka_new() takes ownership of the conf object
         *       and the application must not reference it again after
         *       this call.
         */
        rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
        if (!rk){
            fprintf(stderr, "%% Failed to create new consumer: %s\n", errstr);
            return 1;
        }

		conf = NULL;

		/* Redirect all messages from per-partition queues to
         * the main queue so that messages can be consumed with one
         * call from all assigned partitions.
         *
         * The alternative is to poll the main queue (for events)
         * and each partition queue separately, which requires setting
         * up a rebalance callback and keeping track of the assignment:
         * but that is more complex and typically not recommended. */
        rd_kafka_poll_set_consumer(rk);


        /* Convert the list of topics to a format suitable for librdkafka */
        subscription = rd_kafka_topic_partition_list_new(topic_cnt);
        rd_kafka_topic_partition_list_add(subscription, topics, RD_KAFKA_PARTITION_UA); // the partition is ignored by subscribe()

        /* Subscribe to the list of topics */
        err = rd_kafka_subscribe(rk, subscription);
        if (err){
            fprintf(stderr, "%% Failed to subscribe to %d topics: %s\n", subscription->cnt, rd_kafka_err2str(err));
            rd_kafka_topic_partition_list_destroy(subscription);
            rd_kafka_destroy(rk);
            return 1;
        }

        fprintf(stderr, "%% Subscribed to %d topic(s), waiting for rebalance and messages...\n", subscription->cnt);

        rd_kafka_topic_partition_list_destroy(subscription);

		/* Signal handler for clean shutdown */
        signal(SIGINT, stop);

        /* Subscribing to topics will trigger a group rebalance
         * which may take some time to finish, but there is no need
         * for the application to handle this idle period in a special way
         * since a rebalance may happen at any time.
         * Start polling for messages. */

        while (run) {
            rd_kafka_message_t *rkm;

            rkm = rd_kafka_consumer_poll(rk, 100);
            if (!rkm)
                continue; /* Timeout: no message within 100ms,
                           *  try again. This short timeout allows
                           *  checking for `run` at frequent intervals.
                           */

            /* consumer_poll() will return either a proper message
             * or a consumer error (rkm->err is set). */
            if (rkm->err){
                /* Consumer errors are generally to be considered
                 * informational as the consumer will automatically
                 * try to recover from all types of errors. */
                fprintf(stderr, "%% Consumer error: %s\n", rd_kafka_message_errstr(rkm));
            	rd_kafka_message_destroy(rkm);
                continue;
            }

            /* Proper message. */
            printf("Message on %s [%"PRId32"] at offset %"PRId64":\n", rd_kafka_topic_name(rkm->rkt), rkm->partition, rkm->offset);

            /* Print the message key. */
            if ((const char *)rkm->key && is_printable((const char *)rkm->key, rkm->key_len))
                printf(" Key: %.*s\n", (int)rkm->key_len, (const char *)rkm->key);
            else if ((const char *)rkm->key)
        		printf(" Key: (%d bytes)\n", (int)rkm->key_len);

            /* Print the message value/payload. */
            if ((const char *)rkm->payload && is_printable((const char *)rkm->payload, rkm->len))
                printf(" Value: %.*s\n", (int)rkm->len, (const char *)rkm->payload);
            else if ((const char *)rkm->payload)
                printf(" Value: (%d bytes)\n", (int)rkm->len);

            rd_kafka_message_destroy(rkm);
        }

        /* Close the consumer: commit final offsets and leave the group. */
        fprintf(stderr, "%% Closing consumer\n");
        rd_kafka_consumer_close(rk);

        /* Destroy the consumer */
        rd_kafka_destroy(rk);


		// Standard input file reading
        
		std::string buff = "";
		int status = 0;
		vec<Lit> items;
		vec<Lit> trans;
		std::string temp = "";
		int ind = 0;
		int var_ = 0;

		std::ifstream data(argv[1]);
		while(getline(data, buff)){
			if(buff != ""){
				if(buff.at(0) == 'T'){
					status = 1;
				}
				else if(buff.at(0) == 'A'){
					status = 2;
				}
				else if(buff.at(0) == 'D'){
					status = 3;
				}
				else if(buff.at(0) == 'O'){
					status = 4;
				}

				if(status == 0){ //Items
					if(buff.at(0) != 'I'){
						items.push(mkLit(stoi(buff), false));
					}
				}
				else if(status == 1){ //Tab Transactions
					if(buff.at(0) != 'T'){
						temp = "";
						for(int i = 0; i < buff.length(); i++){
							if(buff.at(i) != '[' && buff.at(i) != ']'){
								if(buff.at(i) == ','){
									trans.push(mkLit(stoi(temp), false));
									temp = "";
								}
								else{
									temp += buff.at(i);
								}
							}
						}
						var_ += trans.size();
						for(int t = 0; t < coop.nbThreads; t++){
	  						while (var_ >= coop.solvers[t].nVars()) coop.solvers[t].newVar();
						}
						coop.addTransactions(trans);
						trans.clear();
					}
				}
				else if(status == 2){ //Appear Trans
					if(buff.at(0) != 'A'){
						temp = "";
						coop.appearTrans.push();
						ind = coop.appearTrans.size()-1;
						for(int i = 0; i < buff.length(); i++){
							if(buff.at(i) != '[' && buff.at(i) != ']'){
								if(buff.at(i) == ','){
									coop.appearTrans[ind].push(stoi(temp));
									temp = "";
								}
								else{
									temp += buff.at(i);
								}
							}
						}
					}
				}
				else if(status == 3){ // Div begining
					if(buff.at(0) != 'D'){
						coop.div_begining = stoi(buff);
					}
				}
				else{ // Occ
					if(buff.at(0) != 'O'){
						coop.occ.push(stoi(buff));
					}
				}
			}
		}

		for(int t = 0; t < nbThreads; t++)
		{
			items.copyTo(coop.solvers[t].allItems);
			for(int i = 0; i < items.size(); i++)
				coop.solvers[t].VecItems.push(var(items[i]));
			coop.solvers[t].nbTrans = coop.tabTransactions.size();
		}
	
		if (coop.solvers[0].verbosity > 0){
        	printf(" ===============================================[ Problem Statistics ]==================================================\n");
        	printf("|                                                                                                                       |\n");
        	printf("|                                                                                                                       |\n"); }
        

		printf("<> instance    : %s\n", "../data.txt");
		printf("<> nbThreads   : %d \n\n", nbThreads);
	
		omp_set_num_threads(nbThreads);
		
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
	
		// launch threads in Parallel 	

	#pragma omp parallel
	{
	  	int t = omp_get_thread_num();
	  	coop.start = true;
	  	coop.solvers[t].EncodeDB(&coop);
	  	ret = coop.solvers[t].solve_(&coop);
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