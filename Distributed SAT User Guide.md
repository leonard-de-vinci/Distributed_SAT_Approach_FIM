# Distributed SAT User Guide
This is an user guide on how to setup and use the distributed SAT approach on one's machine.
### Installation and setup
Let's start by installing all necessary softwares.
- First you will need to install docker (depending on your exploitation system)
https://docs.docker.com/desktop/install/windows-install/
https://docs.docker.com/desktop/install/linux-install/
- Next you will need the docker-compose plugin (if you installed docker desktop you can skip this step as it is already included)
https://docs.docker.com/compose/install/
- Clone the repository, if you're using git you can use the following command
```git clone https://github.com/leonard-de-vinci/Distributed_SAT_Approach_FIM ```
- Finally, you will need to download the image present at this url https://hub.docker.com/r/flexiboy/core
For that you can use directly the following command : ```docker pull flexiboy/core```
- Optional (can be useful) but not required :
    - git
 On windows : https://git-scm.com/download/win
 On linux (debian based distribution) :
```sudo apt install git-all```
    - [MongoDB Compass]
    - [Visual studio code]

Now that everything is installed you need to change the paths in the dockerfiles to your directory.
In the _dockerfiles_ file, in the dockerfile\_master,dockerfile\_slave and dockerfile\_parallel you need to change every path after _WORKDIR_.
It should look like something like this :
WORKDIR /Users/Flexiboy/Desktop/Projects/Distributed_SAT_Approach_FIM/
And you need to replace it with the path where you cloned the repository.

If everything went correctly you should be done with the installation part.

### How to use

- First you need to open a new terminal
- Next go to where you cloned the repository
```cd path_to_repository/Distributed_SAT_Approach_FIM-main/ ```
- The first step is to create and launch the different images needed for the solver to run. In order to do that :
```sh
cd dockerfiles
docker-compose up -d
```
- You can check if everything is running smoothly in the docker desktop app
You can also check your database by opening a web browser and typing : ```localhost:8082```
- Next, let's get back in the main folder
```sh
cd ..
```

- Now you can build manually the different images for the master and the slaves.
Note that this a necessary step if you want to recompile the code if you made modifications
For that you can type the following command
```sh
docker build -t master -f dockerfiles/dockerfile_master .
docker build -t slave -f dockerfiles/dockerfile_slave .
```
- You can also uncomment the 2 last images in the docker-compose file  if you want to run everything at the same time
- Now that you built the all the images you can open multiple terminals (as much as you need) and launch a container in each one of them. In order to do that you can use the following command :
```sh
# For the master
docker run --name master -it master
# For the slave
docker run --name slave -it slave
```
- Note that using the _run_ command will create a container and run it if you already created the container and just want to start it you can use the following command :
```sh
docker start -i name_or_id_of_your_container
# For example for the master :
docker start -i master
```
- When you're done with your tests don't forget to shut down your containers and to go back to the dockerfiles file and do
```sh
docker-compose down
```
- Other useful commands that might help you :
```sh
# To list all containers
docker ps -a
# To remove a container
docker rm id
# or 
docker rm name
# To remove an image
docker rmi id
docker rmi name # also works
```

This concludes the user guide for the Distributed SAT solver.

[//]: # (These are reference links used in the body of this note and get stripped out when the markdown processor does its job. There is no need to format nicely because it shouldn't be seen. Thanks SO - http://stackoverflow.com/questions/4823468/store-comments-in-markdown-syntax)

   [MongoDB Compass]: https://www.mongodb.com/try/download/compass
   [Visual studio code]: https://code.visualstudio.com/


