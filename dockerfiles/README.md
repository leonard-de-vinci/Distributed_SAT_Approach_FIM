# Dockerfiles

> Authors: Julien MARTIN-PRIN ([@Flexiboy](https://github.com/Flexiboy))

*This page explains the docker images and the docker-compose file*

## Core Image

The core image (available at https://hub.docker.com/repository/docker/flexiboy/core) is an Ubuntu based image where every dependencies and packages needed to run the program are installed.

## Master Image

This image, based on the core image, is used to parse an input file and send data to the servant. To connect to this image, you need to type this command:

```sh
docker run -it master
```

if it's the first time you run it or:

```sh
docker start [container id]
docker exec -it master /usr/bin/bash
```

## Servant Image

As for the master, this image is based on the core image. It implements the servant code which contains the solver. This instances generates models and send it to the master(s). To run it, the command are the same as for the master image.


## Docker-compose

This file contains all the image that are necessary to run the program. It contains all the kafka core images to run a kafka cluster on local. To run it, you just need to type this command:

```sh
docker-compose up -d
```
