#!/bin/bash
 
mkdir .ssh
chmod 700 .ssh
cd .ssh
ssh-keygen -t rsa -f id_rsa -N ''
touch authorized_keys
chmod 600 authorized_keys
cp id_rsa.pub authorized_keys
echo "* " `sudo cat /etc/ssh/ssh_host_ecdsa_key.pub` >known_hosts
 
cd
 
 
 
sudo apt-get update
sudo apt-get upgrade -qy
sudo apt-get install -qy  joe
sudo apt-get install -qy libboost-regex1.54-dev
sudo apt-get install -qy libboost-thread1.54-dev
sudo apt-get install -qy build-essential
sudo apt-get install -qy unp
wget http://www.mpich.org/static/downloads/3.1.4/mpich-3.1.4.tar.gz
unp mpich-3.1.4.tar.gz
cd ~/mpich-3.1.4
./configure --disable-fortran
make -j 4
sudo make install
 
cd
wget http://www.mpich.org/static/downloads/3.1.4/hydra-3.1.4.tar.gz
unp hydra-3.1.4.tar.gz
cd ~/hydra-3.1.4
./configure
make -j 4
sudo make install
 
 
cd
wget https://protobuf.googlecode.com/files/protobuf-2.5.0.tar.gz
unp protobuf-2.5.0.tar.gz
cd protobuf-2.5.0
./configure
make -j 4
sudo make install
cd