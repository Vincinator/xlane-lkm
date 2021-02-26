import configparser
import subprocess
import glob
import os
import fnmatch


# [node]
# id=1
# name=tnode1
# port=4000
# ip_addr=192.168.50.2
# hbi=1000000000
# peer_ip_id_tuple=tnode1,1;tnode2,2;tnode3,3

def main():
    cfg = configparser.ConfigParser()
    cfg.read('node.ini')

    node_id = cfg.getint('node', 'id')
    node_name = cfg.get('node', 'name')
    port = cfg.getint('node', 'port')
    hbi = cfg.getint('node', 'hbi')
    peer_ip_id_tuple = cfg.get('node', 'peer_ip_id_tuple')
    ifindex = cfg.getint('lkm', 'ifindex')

    print("Node Config:")
    print(f"\tnode_id: {node_id}")
    print(f"\tnode_name: {node_name}")
    print(f"\tport: {port}")
    print(f"\thbi: {hbi}")
    print(f"\tpeer_ip_id_tuple: {peer_ip_id_tuple}")
    print("LKM Config:")
    print(f"\tifindex: {ifindex}")

    directory = '.'
    asgard_module_name = ''
    for dirpath, dirnames, files in os.walk(directory):
        for filename in fnmatch.filter(files, 'asgard-*.ko'):
            asgard_module_name = os.path.join(dirpath, filename)
            break

    if asgard_module_name == '':
        print("asgard lkm not found\n")
        return


    bashCommand = f"sudo insmod {asgard_module_name} ifindex={ifindex}"
    process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
    output, error = process.communicate()


if __name__ == '__main__':
    main()
