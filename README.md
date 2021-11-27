# nicbenchmarks
Benchmarking Mellanox NICs:

Dependencies
------------
Numa:
sudo apt-get install libnuma-dev

DPDK:
1. Clone dpdk github repo: https://github.com/DPDK/dpdk/tree/releases
2. cd ./dpdk/
3. git checkout v19.11
4. git apply ../mlx5_registration.patch
5. make defconfig
6. make -j

Compile Register
----------------
Export RTE_TARGET: `export RTE_TARGET=build`
Export RTE_SDK: `export RTE_SDK=/proj/demeter-PG0/murray22/nicbenchmarks/dpdk/`
Run `make`

Execute Register
----------------
Run `ethtool -i [interface]` and take note of the bus-info

Server: sudo build/register -c 0xff -n 4 -w [bus-info value found in #5] --proc-type=auto -- --mode=SERVER --ip=[ip of the machine] 

Client: sudo build/register -c 0xff -n 4 -w [bus-info value found in #5] --proc-type=auto -- --mode=CLIENT -server_ip=[ip of the server] --ip=[ip of the machine] --server_mac=[mac address printed out by the server] --rate=120000 --message_size=1024 --time=10
