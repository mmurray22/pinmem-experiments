# Benchmarks Overview
This repository consists of various small-scale tests intended to help inform experimentation about pinned memory usage in kernel bypass networking stacks. Below, each individual experiment's purpose and how to run it is outlined: 

# /nicbenchmarks - WORK IN PROGRESS
The intended purpose of this benchmark is to measure how long it takes to register memory regions. To run the benchmark, run the following:

Dependencies
-------------
Install the following dependencies in the listed order:

Numa: 
1. `sudo apt-get update && sudo apt-get upgrade`
2. `sudo apt-get install libnuma-dev`

Mellanox Driver:
1. `wget https://www.mellanox.com/downloads/ofed/MLNX_OFED-<version>/MLNX_OFED_LINUX-<version>-<distribution>-<arch>.tgz
	Example: wget https://www.mellanox.com/downloads/ofed/MLNX_OFED-5.0-2.1.8.0/MLNX_OFED_LINUX-5.0-2.1.8.0-ubuntu18.04-x86_64.tgz`
2. `tar -xvf MLNX_OFED_LINUX-<version>-<distribution>-<arch>.tgz`
3. `cd MLNX_OFED_LINUX-<version>-<distribution>-<arch>`
4. `sudo ./mlnxofedinstall --upstream-libs --dpdk`
5. Restart the driver: `sudo /etc/init.d/openibd restart`

Enable HugePages:
1. Run `sudo /path/to/nicbenchmarks/dpdk/usertools/dpdk-setup.sh`
2. Select #49 (or #6, depends on the script)
3. Enter 2048
4. Quit

DPDK:
1. `git clone https://github.com/DPDK/dpdk.git`
2. `cd ./dpdk/`
3. `git checkout v19.11`
4. `git apply ../mlx5_registration.patch`
5. `make defconfig`
6. `make -j`

Compilation
-----------
To compile the program, execute the following steps:

1. Export RTE_TARGET: `export RTE_TARGET=build`
2. Export RTE_SDK: `export RTE_SDK=/proj/demeter-PG0/murray22/nicbenchmarks/dpdk`
3. Run `make`

Running the program
-------------------
To run the program, follow these steps:

1. Run `ethtool -i [interface]` and take note of the bus-info
2. Server: `sudo build/register -c 0xff -n 4 -w [bus-info value found in #5] --proc-type=auto -- --mode=SERVER --ip=[ip of the machine]`
	a. Example Server command: `sudo build/register -c 0xff -n 4 -w 0000:41:00.0 --proc-type=auto -- --mode=SERVER --ip=192.168.1.1 --num_mbufs=2` 
3. Client: `sudo build/register -c 0xff -n 4 -w [bus-info value found in #5] --proc-type=auto -- --mode=CLIENT -server_ip=[ip of the server] --ip=[ip of the machine] --server_mac=[mac address printed out by the server] --rate=120000 --message_size=1024 --time=10`

# /simulation - WORK IN PROGRESS

This folder contains simulation code which runs over a twitter trace [CITE TRACE] and observes various statistics related to number of pages "pinned".
