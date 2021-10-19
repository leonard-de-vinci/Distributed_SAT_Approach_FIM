#!/usr/local/bin/zsh
cd code/master/core
make CXX=g++-10
cd ../slave/core
make CXX=g++-10
cd ../../../
cp dockerfiles/dockerfile_master dockerfile
docker build -t master .
cp dockerfiles/dockerfile_slave dockerfile
docker build -t slave .