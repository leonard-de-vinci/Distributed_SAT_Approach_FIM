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
#include <time.h>

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

char *gettime(){
        time_t rawtime;
        struct tm *timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        return asctime(timeinfo);
}

void delay(int duration){
        clock_t start_time = clock();
	while(clock() < start_time + duration)
		;
}

static int is_printable (const char *buf, size_t size){
    for (size_t i = 0 ; i < size ; i++)
        if (!isprint((int)buf[i]))
            return 0;

    return 1;
}

static void stop (int sig){
    run = 0;
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

		int nbThreads   = ncores;
		int nsolvers	= 0;
		Cooperation coop(nbThreads);
		time_t rawtime1;
		time_t rawtime2;
		struct tm *start_date;
		struct tm *end_date;
	
		coop.min_supp = Freq;
	
		for(int t = 0; t < nbThreads; t++){
	  		coop.solvers[t].threadId = t;
	  		coop.solvers[t].verbosity = verb;
		}

		fprintf(stderr, " -----------------------------------------------------------------------------------------------------------------------\n");
		fprintf(stderr, "|                                 DSATMiner    %i thread(s) on %i core(s)                                                |\n", coop.nbThreads, omp_get_num_procs()); 
		fprintf(stderr, " -----------------------------------------------------------------------------------------------------------------------\n");


    	// Set limit on CPU-time:
    	if (cpu_lim != INT32_MAX){
        	rlimit rl;
        	getrlimit(RLIMIT_CPU, &rl);
        	if (rl.rlim_max == RLIM_INFINITY || (rlim_t)cpu_lim < rl.rlim_max){
            	rl.rlim_cur = cpu_lim;
            	if (setrlimit(RLIMIT_CPU, &rl) == -1)
                	fprintf(stderr, "WARNING! Could not set resource limit: CPU-time.\n");
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
                	fprintf(stderr, "WARNING! Could not set resource limit: Virtual memory.\n");
        	}
		}
		
		fprintf(stderr, "\n");



//		+-------------------------------------------------------+
//		|                                                       |
//		|                      MongoDB                          |
//		|                                                       |
//		+-------------------------------------------------------+

		const char *mongo_config_file = "mongo.config";
		const char *key;
		const bson_t *config, *items, *tab_transactions, *appear_trans;
		const bson_value_t *value;
        char *uri_string;
		char temp[30] = {0};
		int n_items, n_trans, n_appear_trans, var_;
		bool sent = true;
		size_t keylen;
        mongoc_uri_t *uri;
        mongoc_client_t *client;
        mongoc_database_t *database;
        mongoc_collection_t *collection;
		mongoc_cursor_t *cursor;
        bson_t *query;
		bson_t *document;
		bson_t child2;
        bson_error_t error;
		bson_oid_t oid;
		bson_iter_t iter, child;

		vec<Lit> items_temp;
		vec<Lit> trans_temp;

		if (!(uri_string = mongo_config(mongo_config_file))){
            fprintf(stderr, "Failed mongo config");
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

        mongoc_client_set_appname (client, "solver");

		//Solver Config

		database = mongoc_client_get_database(client, "solvers");
		collection = mongoc_client_get_collection(client, "solvers", "config");
		document = bson_new();
        bson_oid_init(&oid, NULL);
        BSON_APPEND_OID(document, "_id", &oid);

		BSON_APPEND_DOCUMENT_BEGIN(document, "solver", &child2);
        BSON_APPEND_INT32(&child2, "ncores", nbThreads);
        bson_append_document_end(document, &child2);

		if (!mongoc_collection_insert_one(collection, document, NULL, NULL, &error)){
            fprintf (stderr, "%s\n", error.message);
        }
        else{
            fprintf(stderr, "Solver configuration sent\n\n");
        }

        bson_destroy(document);
        mongoc_collection_destroy(collection);
		mongoc_database_destroy(database);

		//Database pull

		fprintf(stderr, "Retrieving dataset...\n");


        database = mongoc_client_get_database(client, "dataset");
		query = bson_new();


		// Config
		collection = mongoc_client_get_collection(client, "dataset", "config");
		cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);

		delay(1000);

		do
		{
			delay(500);
		} while(mongoc_collection_count_documents(collection, query, NULL, NULL, NULL, &error) != 1);

		while(mongoc_cursor_next(cursor, &config)){
			if(bson_iter_init(&iter, config)){
				while(bson_iter_next(&iter)){
					key = bson_iter_key(&iter);
					if(bson_iter_init_find(&iter, config, key) && BSON_ITER_HOLDS_DOCUMENT(&iter) && bson_iter_recurse(&iter, &child)){
						while(bson_iter_next(&child)){
							value = bson_iter_value(&child);
							if(!(value->value_type == BSON_TYPE_INT32)){
								fprintf(stderr, "failed to parse document: %s in collection config", key);
								return EXIT_FAILURE;
							}
							if(strcmp(key, "nsolvers") == 0)
								nsolvers = value->value.v_int32;
							else if(strcmp(key, "items") == 0)
								n_items = value->value.v_int32;
							else if(strcmp(key, "tab_transactions") == 0)
								n_trans = value->value.v_int32;
							else if(strcmp(key, "appear_trans") == 0)
								n_appear_trans = value->value.v_int32;
						}
					}
				}
			}
		}

		mongoc_cursor_destroy(cursor);
		mongoc_collection_destroy(collection);

		fprintf(stderr, "Database configuration received\n");

		//Items
		collection = mongoc_client_get_collection(client, "dataset", "items");
		cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);

		do
		{
			delay(500);
		} while(mongoc_collection_count_documents(collection, query, NULL, NULL, NULL, &error) != 1);

		while(mongoc_cursor_next(cursor, &items)){
			if(bson_iter_init(&iter, items)){
				while(bson_iter_next(&iter)){
					key = bson_iter_key(&iter);
					if(bson_iter_init_find(&iter, items, key) && BSON_ITER_HOLDS_ARRAY(&iter) && bson_iter_recurse(&iter, &child)){
						while(bson_iter_next(&child)){
							value = bson_iter_value(&child);
							if(!(value->value_type == BSON_TYPE_INT32)){
								fprintf(stderr, "failed to parse document: %s in collection items", key);
								return EXIT_FAILURE;
							}
							if(items_temp.size() < n_items){
								items_temp.push(mkLit(value->value.v_int32, false));
							}
						}
					}
				}
			}
		}

		mongoc_cursor_destroy(cursor);
		mongoc_collection_destroy(collection);

		if(n_items != items_temp.size()){
			fprintf(stderr, "failed to retrieve all items, number of items: %d", items_temp.size());
			return EXIT_FAILURE;
		}
		fprintf(stderr, "Items received\n");

		//Tab Transactions
		collection = mongoc_client_get_collection(client, "dataset", "tab_transactions");
		cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);

		do
		{
			delay(500);
			fprintf(stderr, "On MongoDB, collection tab_transaction: n_trans: %d; n_docs: %d\n", n_trans, mongoc_collection_count_documents(collection, query, NULL, NULL, NULL, &error));
		} while(mongoc_collection_count_documents(collection, query, NULL, NULL, NULL, &error) != n_trans);

		while(mongoc_cursor_next(cursor, &tab_transactions)){
			if(bson_iter_init(&iter, tab_transactions)){
				while(bson_iter_next(&iter)){
					key = bson_iter_key(&iter);
					if(bson_iter_init_find(&iter, tab_transactions, key) && BSON_ITER_HOLDS_ARRAY(&iter) && bson_iter_recurse(&iter, &child)){
						while(bson_iter_next(&child)){
							value = bson_iter_value(&child);
							if(!(value->value_type == BSON_TYPE_INT32)){
								fprintf(stderr, "failed to parse document: %s in collection tab_transactions", key);
								return EXIT_FAILURE;
							}
							trans_temp.push(mkLit(value->value.v_int32, false));
						}
						var_ += trans_temp.size();
						for(int t = 0; t < coop.nbThreads; t++){
	  						while (var_ >= coop.solvers[t].nVars()) coop.solvers[t].newVar();
						}
						coop.addTransactions(trans_temp);
						trans_temp.clear();
					}
				}
			}
		}

		mongoc_cursor_destroy(cursor);
		mongoc_collection_destroy(collection);

		fprintf(stderr, "Tab transactions received\n");

		//Appear Trans
		collection = mongoc_client_get_collection(client, "dataset", "appear_trans");
		cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);

		do
		{
			delay(500);
			fprintf(stderr, "On MongoDB, collection appear_trans: n_appear_trans: %d; n_doc: %d\n", n_appear_trans, mongoc_collection_count_documents(collection, query, NULL, NULL, NULL, &error));
		} while(mongoc_collection_count_documents(collection, query, NULL, NULL, NULL, &error) != n_appear_trans);

		while(mongoc_cursor_next(cursor, &appear_trans)){
			if(bson_iter_init(&iter, appear_trans)){
				while(bson_iter_next(&iter)){
					key = bson_iter_key(&iter);
					if(bson_iter_init_find(&iter, appear_trans, key) && BSON_ITER_HOLDS_ARRAY(&iter) && bson_iter_recurse(&iter, &child)){
						coop.appearTrans.push();
						while(bson_iter_next(&child)){
							value = bson_iter_value(&child);
							if(!(value->value_type == BSON_TYPE_INT32)){
								fprintf(stderr, "failed to parse document: %s in collection appear_trans", key);
								return EXIT_FAILURE;
							}
							coop.appearTrans[coop.appearTrans.size()-1].push(value->value.v_int32);
						}
					}
				}
			}
		}

		mongoc_cursor_destroy(cursor);
		mongoc_collection_destroy(collection);

		fprintf(stderr, "Appear trans received\n");

		mongoc_database_destroy(database);

		for(int t = 0; t < nbThreads; t++)
		{
			items_temp.copyTo(coop.solvers[t].allItems);
			for(int i = 0; i < items_temp.size(); i++)
				coop.solvers[t].VecItems.push(var(items_temp[i]));
			coop.solvers[t].nbTrans = coop.tabTransactions.size();
		}

		fprintf(stderr, "\n");

		if (coop.solvers[0].verbosity > 0){
        	fprintf(stderr, " ===============================================[ Problem Statistics ]==================================================\n");
        	fprintf(stderr, "|                                                                                                                       |\n");
        	fprintf(stderr, "|                                                                                                                       |\n"); }
        

		fprintf(stderr, "<> instance    : %s\n", "");
		fprintf(stderr, "<> nbThreads   : %d \n\n", nbThreads);
	
		omp_set_num_threads(nbThreads);
		
     	FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : NULL;
        
        if (coop.solvers[0].verbosity > 0){
		//  printf("|  Number of variables:  %12d                                                                                   |\n", coop.solvers[0].nVars());
		}

		if (!coop.solvers[0].simplify()){
	  		if (res != NULL)
				fprintf(res, "UNSAT\n"), fclose(res);
	  		if (coop.solvers[0].verbosity > 0){
	    		fprintf(stderr, "========================================================================================================================\n");
	    		fprintf(stderr, "Solved by unit propagation\n");
	    		fprintf(stderr, "\n"); 
			}
	  		fprintf(stderr, "UNSATISFIABLE\n");
	  		exit(20);
       	}



//		+-------------------------------------------------------+
//		|                                                       |
//		|                       Kafka                           |
//		|                                                       |
//		+-------------------------------------------------------+

		rd_kafka_t *rk; //consumer instance handle
        rd_kafka_conf_t *conf; //temporary configuration object
		rd_kafka_resp_err_t err; //librdkafka API error code
        char errstr[512]; //librdkafka API error reporting buffer
		FILE *log = fopen("kafka.log", "a");
		char *message;

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
		do
		{
			if (err && err != RD_KAFKA_RESP_ERR_UNKNOWN_TOPIC_OR_PART){
            	fprintf(stderr, "%% Failed to subscribe to %d topics: %s\n", subscription->cnt, rd_kafka_err2str(err));
            	rd_kafka_topic_partition_list_destroy(subscription);
            	rd_kafka_destroy(rk);
            	return 1;
        	}
		} while ((err = rd_kafka_subscribe(rk, subscription)) == RD_KAFKA_RESP_ERR_UNKNOWN_TOPIC_OR_PART);

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
				if (rkm->err != RD_KAFKA_RESP_ERR_UNKNOWN_TOPIC_OR_PART)
                	fprintf(stderr, "%% Consumer error: %s\n", rd_kafka_message_errstr(rkm));
            	rd_kafka_message_destroy(rkm);
                continue;
            }

            /* Proper message. */
            fprintf(log, "%s | %%Message on %s [%"PRId32"] at offset %"PRId64": %s\n", gettime(), rd_kafka_topic_name(rkm->rkt), rkm->partition, rkm->offset, (const char *)rkm->payload);
			fprintf(stderr, "%s | %%Message on %s [%"PRId32"] at offset %"PRId64": %s\n", gettime(), rd_kafka_topic_name(rkm->rkt), rkm->partition, rkm->offset, (const char *)rkm->payload);

            // /* Print the message key. */
            // if((const char *)rkm->key && is_printable((const char *)rkm->key, rkm->key_len))
            //     printf(" Key: %.*s\n", (int)rkm->key_len, (const char *)rkm->key);
            // else if((const char *)rkm->key)
        	// 	printf(" Key: (%d bytes)\n", (int)rkm->key_len);

            /* Print the message value/payload. */
            if((const char *)rkm->payload && is_printable((const char *)rkm->payload, rkm->len)){
				message = (char *) malloc((int)rkm->len + 1);
				sprintf(message, (const char *)rkm->payload);
			}
                // printf(" Value: %.*s\n", (int)rkm->len, (const char *)rkm->payload);
            // else if((const char *)rkm->payload)
            //     printf(" Value: (%d bytes)\n", (int)rkm->len);

			if(strcmp(message, "end") != 0)
				coop.guiding_path.push_back(atoi(message));
			else
				run = false;

            rd_kafka_message_destroy(rkm);
        }

        /* Close the consumer: commit final offsets and leave the group. */
        fprintf(stderr, "%% Closing consumer\n");
        rd_kafka_consumer_close(rk);

        /* Destroy the consumer */
        rd_kafka_destroy(rk);

		fprintf(stderr, "\n");

		for(int i = 0; i < nbThreads; i++)
			coop.guiding_path.push_back(coop.guiding_path.at(coop.guiding_path.size() - 1) + 1);

		coop.div_begining = coop.guiding_path[0];
        
		lbool ret;
		lbool result;
		double time_elapsed = 0.0;
		clock_t begin = clock();
		time(&rawtime1);
		start_date = localtime(&rawtime1);
	
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
		time(&rawtime2);
		end_date = localtime(&rawtime2);
	
		int cpt = 0;
		// each worker print its models
		fprintf(stderr, "-----------------------------------------------\n");
		fprintf(stderr, "thread | nb models          | nb conflicts    |\n");
		fprintf(stderr, "-----------------------------------------------\n");

		int nbcls = 0;
		for(int t = 0; t < coop.nThreads(); t++){
			cpt +=  coop.solvers[t].nbModels;
			nbcls += coop.solvers[t].nbClauses;
			fprintf(stderr, "  %2d   |   %15d  | %d \n", t, coop.solvers[t].nbModels, (int)coop.solvers[t].conflicts);
		}
		fprintf(stderr, "-----------------------------------------------\n");
		fprintf(stderr, "total  | %15d    | \n", cpt);
		fprintf(stderr, "-----------------------------------------------\n");
		
		fprintf(stderr, "#total Clauses  : %15d     \n", nbcls);

		fprintf(stderr, "\n");



//		+-------------------------------------------------------+
//		|                                                       |
//		|                Models transmission                    |
//		|                                                       |
//		+-------------------------------------------------------+

		fprintf(stderr, "Sending models...\n");

		database = mongoc_client_get_database(client, "solvers");
		collection = mongoc_client_get_collection(client, "solvers", "models");
		
		for(int i = 0; i < coop.models.size(); i++){
            document = bson_new();
            bson_oid_init(&oid, NULL);
            BSON_APPEND_OID (document, "_id", &oid);
            
            sprintf(temp, "model");
            BSON_APPEND_ARRAY_BEGIN(document, temp, &child2);
            for(uint32_t j = 0; (int) j < coop.models[i].size(); j++){
                sprintf(temp, "%d", coop.models[i][j]);
                keylen = bson_uint32_to_string(i, &key, temp, sizeof(temp));
                bson_append_int32(&child2, key, -1, coop.models[i][j]);
            }
            bson_append_array_end(document, &child2);

            if (!mongoc_collection_insert_one(collection, document, NULL, NULL, &error)){
                fprintf (stderr, "%s\n", error.message);
                sent = false;
            }

            bson_destroy(document);
        }

        if(sent)
            fprintf(stderr, "Models sent\n");

        mongoc_collection_destroy(collection);

		collection = mongoc_client_get_collection(client, "solvers", "finished");
		document = bson_new();
        bson_oid_init(&oid, NULL);
        BSON_APPEND_OID(document, "_id", &oid);

		BSON_APPEND_DOCUMENT_BEGIN(document, "processing_time (in ms)", &child2);

		for(int i = 0; i < coop.processing_time.size(); i++){
			sprintf(temp, "%d", (int) coop.processing_time[i][0]);
			BSON_APPEND_DOUBLE(&child2, temp, coop.processing_time[i][1]);
		}

		bson_append_document_end(document, &child2);

		BSON_APPEND_DOCUMENT_BEGIN(document, "time_elapsed", &child2);
        BSON_APPEND_DOUBLE(&child2, "processing time (in ms)", time_elapsed);
        bson_append_document_end(document, &child2);

		BSON_APPEND_DOCUMENT_BEGIN(document, "start_time", &child2);
        sprintf(temp, "");
		strftime(temp, 30, "%H%M%S", start_date);
		BSON_APPEND_INT32(&child2, "start time", atoi(temp));
        bson_append_document_end(document, &child2);

		BSON_APPEND_DOCUMENT_BEGIN(document, "end_time", &child2);
		sprintf(temp, "");
		strftime(temp, 30, "%H%M%S", end_date);
		BSON_APPEND_INT32(&child2, "end time", atoi(temp));
		bson_append_document_end(document, &child2);

		if (!mongoc_collection_insert_one(collection, document, NULL, NULL, &error)){
            fprintf (stderr, "%s\n", error.message);
            sent = false;
        }

        bson_destroy(document);

		if(sent)
            fprintf(stderr, "Solver finish status sent\n");

		mongoc_collection_destroy(collection);
		mongoc_database_destroy(database);
		mongoc_client_destroy(client);
        mongoc_cleanup();

		fprintf(stderr, "\nTerminating...\n");
		
		while (1){
			delay(60000);
			fprintf(stderr, "Wait...\n");
		}

#ifdef NDEBUG
        exit(result == l_True ? 10 : result == l_False ? 20 : 0);     // (faster than "return", which will invoke the destructor for 'Solver')
#else
        return (result == l_True ? 10 : result == l_False ? 20 : 0);
#endif
    } catch (OutOfMemoryException&){
        fprintf(stderr, "===============================================================================\n");
        fprintf(stderr, "INDETERMINATE\n");
        exit(0);
    }
}