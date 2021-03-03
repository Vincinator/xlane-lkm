import configparser
import subprocess
import glob
import os
import fnmatch
import click
from pathlib import Path



def print_config(cfg):
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


def issue_cmd(cmd):
    process = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE)
    output, error = process.communicate()
    return output, error


def write_procfs(path, value):

    if not Path(path).is_file():
        print ("{path} does not exist.")
        return

    cmd = f"sudo echo {value} > {path}"
    issue_cmd(cmd)


def read_procfs(path):

    if not Path(path).is_file():
        print ("{path} does not exist.")
        return

    cmd = f"cat {path}"
    output, error = issue_cmd(cmd)

    if error == "":
        return output
    else:
        print(f"error reading {path}: {error}")
        return ""


def start_consensus(cfg):
    print("starting consensus")


def stop_consensus(cfg):
    print("stopping consensus")


def start_pingpong(cfg):
    print("starting pingpong")


def stop_pingpong(cfg):
    print("stopping pingpong")


def start_protocol(cfg, protocol):
    if protocol == "consensus":
        start_consensus(cfg)
    elif protocol == "pingpong":
        start_pingpong(cfg)


def stop_protocol(cfg, protocol):
    if protocol == "consensus":
        stop_consensus(cfg)
    elif protocol == "pingpong":
        stop_pingpong(cfg)


def unload_module(cfg):
    bashCommand = f"sudo rmmod asgard"
    process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
    output, error = process.communicate()


def load_module(cfg):
    ifindex = cfg.getint('lkm', 'ifindex')

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


@click.command()
@click.option('--load', is_flag=True, help='load asgard kernel module')
@click.option('--unload', is_flag=True, help='unload asgard kernel module')
@click.option('--protocol', help='protocol to load')
@click.option('--start', is_flag=True, help='starts the selected protocol. requires a selected protocol')
@click.option('--stop', is_flag=True, help='stops the selected protocol. requires a selected protocol')
def main(load, unload, protocol, start, stop):
    cfg = configparser.ConfigParser()
    cfg.read('node.ini')
    print_config(cfg)

    if load:
        load_module(cfg)

    if unload:
        unload_module(cfg)

    if start:
        start_protocol(cfg, protocol)

    if stop:
        stop_protocol(cfg, protocol)

    if not load and not unload and not start and not stop:
        print("Nothing to do. Please specify via a flag what you want to do.")


if __name__ == '__main__':
    main()
