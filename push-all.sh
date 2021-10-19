docker tag master:latest flexiboy/master:latest
docker push flexiboy/master:latest
docker image tag master:latest s3.dvic.devinci.fr/clusterspeed/master:latest
docker image tag slave:latest s3.dvic.devinci.fr/clusterspeed/slave:latest
docker image push s3.dvic.devinci.fr/clusterspeed/master:latest
docker image push s3.dvic.devinci.fr/clusterspeed/slave:latest