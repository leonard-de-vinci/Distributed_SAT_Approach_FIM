cp mongo.config core/mongo.config
cp kafka.config core/librdkafka.config

./core/master -minSupport=$MINSUPPORT -nsolvers=$NSOLVERS -reset=$RESET $DATASET
