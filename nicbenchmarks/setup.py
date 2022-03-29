import subprocess


if __name__ == '__main__':
    # Update and upgrade the OS
    subprocess.run(['apt-get', 'update'])
    subprocess.run(['apt-get', 'upgrade'])

    # Install relevant packages
    subprocess.run(['apt-get', 'install', 'libnuma-dev'])

    # Install Mellanox Drivers
    subprocess.run(['/proj/demeter-PG0/murray22/MLNX_OFED_LINUX-5.0-2.1.8.0-ubuntu18.04-x86_64/mlnxofedinstall', '--upstream-libs', '--dpdk'])
    subprocess.run(['/etc/init.d/openibd', 'restart'])
