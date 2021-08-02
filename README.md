# Distributed_SAT_Approach

> Authors: Imen OULED-DLALA ([@dlalaimen](https://github.com/dlalaimen)), Said JABBOUR, Nicolas TRAVERS ([@chewbii](https://github.com/chewbii)) and Julien MARTIN-PRIN ([@Flexiboy](https://github.com/Flexiboy))

## Summary

* [Project description](https://github.com/leonard-de-vinci/Distributed_SAT_Approach_FIM#project-description)
* [How does this solver work](https://github.com/leonard-de-vinci/Distributed_SAT_Approach_FIM#how-does-this-solver-work)
* [Architecture schema](https://github.com/leonard-de-vinci/Distributed_SAT_Approach_FIM#architecture-schema)

## Project description

This project is a "fork" of a previous parallel CFI (Closed Frequent Itemset) SAT solver. The goal in this project was to distribute this solver in a cloud or grid solution. 

## How does this solver work

You can either use the docker images of the master and the slave or use the apps outside docker to run this program. 

First, you'll need a kafka cluster and a mongoDB database to run this program. The easiest way to proceed is to use the [docker-compose](https://github.com/leonard-de-vinci/Distributed_SAT_Approach_FIM/blob/main/dockerfiles/docker-compose.yml) file provided in the [dockerfiles](https://github.com/leonard-de-vinci/Distributed_SAT_Approach_FIM/tree/main/dockerfiles) folder. You'll just need to removed the extra parameters set for kafka in the broker section and the master and slaves containers. 

As shown in the architecture schema below, the master will parse a dataset passed as a parameter and will transfer its parsed data to a mongoDB database. Then the solvers will retrieve this data as the master will create and push a guiding path vector to a kafka cluster. Then the solvers will retrieve a part of the guiding path (the guiding path is splitted in partitions by the master and the solver only has access to one partition) and solve the problem with the data and the guiding path. After this, the solvers will transfer the generated models and their compute time for each guiding path to mongoDB. To finish, the master will retrieve all the models and will generate a file containing all of them.

## Architecture Schema

Here is the architecture used in this project:

<img src=https://github.com/leonard-de-vinci/Distributed_SAT_Approach_FIM/blob/main/Images/Schema%20architecture%20final.png width=1000>