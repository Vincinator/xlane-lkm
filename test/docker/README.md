# Asgard in docker

This guide describes a method to test asgard locally with docker containers.
The main motivation for local testing with docker is to avoid the time-consuming 
build and deploy pipeline and keep the server nodes free for evaluation.


To limit complexity and enhance readability of this guide, 
I will only go through the steps for setting up CLion.


## Steps
0. Install Docker and docker-compose (google it - trust me, its better this way.)
1. Install the CLion Docker Plugin
2. Setup the Docker Plugin : https://www.jetbrains.com/help/clion/docker.html
3. Edit Configurations and add an Docker-Image target and name the image "libasraft-tnode"
4. Build the docker image (adapt the passwords to your requirements)
5. Edit Configurations and add an Docker-Compose target for the test/docker/docker-compose.yml
7. add an the following alias
   ```alias wk="ssh -i docker/.ssh/id_ed25519 -p 2227 dsp@localhost"```
8. login to your workstation via the alias ```wk``` lookup the pw in the dockerfile
9. in your home folder you have the ansible 




