FROM flexiboy/core

ENV MINSUPPORT=500
ENV NSOLVERS=2
ENV RESET=0
ENV DATASET=../../../data/retail.dat

ENV MONGOUSER=root
ENV MONGOPASS=root
ENV MONGOADDR=mongodb
ENV MONGOPORT=27017

ENV KAFKAADDR=broker
ENV KAFKAPORT=9092

WORKDIR /Users/Flexiboy/Desktop/Projects/Distributed_SAT_Approach_FIM/

ADD ./code/master ./code/master
ADD ./data ./data

RUN cd code/master/core\
	&& echo "username=${MONGOUSER}" > mongo.config\
	&& echo "password=${MONGOPASS}" >> mongo.config\
	&& echo "address=${MONGOADDR}" >> mongo.config\
	&& echo "port=${MONGOPORT}" >> mongo.config\
	&& echo "#Kafka" > librdkafka.config\
	&& echo "bootstrap.servers=${KAFKAADDR}:${KAFKAPORT}" >> librdkafka.config\
	&& make clean\
	&& make -k CXX=g++-10

WORKDIR /Users/Flexiboy/Desktop/Projects/Distributed_SAT_Approach_FIM/code/master/core

CMD ./master -minSupport=${MINSUPPORT} -nsolvers=${NSOLVERS} -reset=${RESET} ${DATASET}
