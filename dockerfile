FROM flexiboy/core

WORKDIR /Users/Flexiboy/Desktop/Projects/Distributed_SAT_Approach_FIM/

ADD ./code/master ./code/master
ADD ./data ./data

RUN cd code/master/core \
	&& make clean\
	&& make -k CXX=g++-10

CMD /usr/bin/bash