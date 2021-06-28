FROM flexiboy/core

WORKDIR /Users/Flexiboy/Desktop/Projects/Distributed_SAT_Approach_FIM/

ADD ./code/master ./code/master
ADD ./data ./data

RUN cd code/master/core \
	&& make clean\
	&& make -k CXX=g++-10\
	&& touch librdkafka.config\
	&& echo "# Kafka" > librdkafka.config\
	&& echo "bootstrap.servers=localhost:9092" >> librdkafka.config

CMD /usr/bin/bash
