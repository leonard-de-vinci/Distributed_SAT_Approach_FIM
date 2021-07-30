FROM flexiboy/core

EXPOSE 27017/tcp
EXPOSE 27017/udp
EXPOSE 29092/tcp
EXPOSE 29092/udp

ENV MINSUPPORT=500
ENV NSOLVERS=2
ENV RESET=0
ENV DATASET=../../../data/retail.dat

WORKDIR /Users/Flexiboy/Desktop/Projects/Distributed_SAT_Approach_FIM/

ADD ./code/master ./code/master
ADD ./data ./data

RUN cd code/master/core\
	&& make clean\
	&& make -k CXX=g++-10

WORKDIR /Users/Flexiboy/Desktop/Projects/Distributed_SAT_Approach_FIM/code/master/core

CMD ./master -minSupport=${MINSUPPORT} -nsolvers=${NSOLVERS} -reset=${RESET} ${DATASET}