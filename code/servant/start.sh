cp mongo.config core/mongo.config
cp kafka.config core/librdkafka.config

./core/slave -minSupport=$MINSUPPORT -ncores=$NCORES
