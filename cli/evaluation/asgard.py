# ##########
#
# NOT FINISHED - NOT WORKING YET - WORK IN PROGRESS
#
# ##########
import fnmatch
import subprocess

import click
import paramiko
import os
import configparser
import os.path
from os import path
import cli.evaluation.pyasgard.pyasgard as pyasg
import time

from cli.evaluation.interpret.tsInterpreter import vanillaAsgardEvalReport
from cli.utils import prepareHost


def doPingPong(config):
    pass


def setup_logs(cfg, log_dir):
    if not os.path.exists("/home/dsp"):
        print("ERROR: Could not find dsp home folder. (currently hardcoded home path)")
        return
    log_dir = os.path.join("/home/dsp", "logs", "plainasgard")
    if not os.path.exists(log_dir):
        os.makedirs(log_dir)


def update_ifindex(cfg, ifindex):
    if ifindex != -1:
        cfg['asgard']['iface'] = f"{ifindex}"


def load_module(cfg):
    ifindex = cfg['asgard']['iface']
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

def unload_module(cfg):
    bashCommand = f"sudo rmmod asgard"
    process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
    output, error = process.communicate()


def start_proto(cfg, proto):
    pass


def stop_proto(cfg, proto):
    pass


def configure_asgard(cfg, proto):

    do_configure_asgard(cfg)

    if not proto is None:
        pass



def dump_logs(cfg, proto):
    pass


def clean_asgard(cfg, proto):
    pass


def do_action(cfg, action, protocol):
    print(f"Parsing action: {action}, protocol: {protocol}")

    if action == 'load':
        load_module(cfg)
    elif action == 'unload':
        unload_module(cfg)
    elif action == 'start':
        if protocol is None:
            print("\tstop action requires specified protocol (via --protocol name_of_proto)")
            return
        start_proto(cfg, protocol)
    elif action == 'stop':
        if protocol is None:
            print("\tstop action requires specified protocol (via --protocol name_of_proto)")
            return
        stop_proto(cfg, protocol)
    elif action == 'configure':
        configure_asgard(cfg, protocol)
    elif action == 'dump':
        dump_logs(cfg, protocol)
    elif action == 'clean':
        clean_asgard(cfg, protocol)
    else:
        print(f"Unknown Action {action}")


def handle_actions(cfg, actions, protocol):
    actions_list = [x.strip() for x in actions.split(',')]

    for a in actions_list:
        do_action(cfg, a, protocol)


@click.command(context_settings=dict(max_content_width=120))
@click.option('--config_path', default='example.asgard-bench.ini', show_default=True,
              help="Path to the configuration file. The redis leader and local IP/Port is configured in this config, as well as paths")
@click.option('--ifindex', default=-1, show_default=True,
              help="Uses this parameter as ifindex instead of ifindex from .ini file")
@click.option('--log_dir', default="~/logs", show_default=True,
              help="Directory where the timestamp logs are downloaded to")
@click.option('--actions', '-a', default="load,configure,clean,unload", show_default=False,
                help="""[Default=load,configure,clean,unload]\n
                Comma separated list of actions to execute in specified order.
                The available actions are listed below:\n
                    - start: starts the selected protocol (requires protocol flag)\n
                    - stop: stops the selected protocol (requires protocol flag)\n
                    - dump: dumps logs for the selected protocol (requires protocol flag)\n
                    - configure: configures asgard and if specified also the selected protocol\n
                    - clean: cleans the asgard instance\n
                    - load: loads the asgard kernel module \n
                    - unload: unloads the asgard kernel module\n"""
              )
@click.option('--protocol', '-p', default=None, help="Select the target protocol")
@click.option('--multicast', '-m', default=False, show_default=True,is_flag=True,
              help="configures asgard for multicast enabled features")
def evalAsgard(config_path, ifindex, log_dir, actions, protocol, multicast):

    config = load_config(config_path)

    if config is None:
        print(f"Config File {config_path} does not exist. Generate it via generate-config command")
        return

    # if ifindex is provided via flag, ifindex of local config instance will be updated
    update_ifindex(config, ifindex)

    setup_logs(config, log_dir)

    handle_actions(config, actions, protocol)


    # if clean:
    #     cleanAsg(config)
    #
    # if prepare:
    #     log_dir = os.path.join("/home/dsp", "logs", "plainasgard")
    #     if not os.path.exists(log_dir):
    #         os.makedirs(log_dir)
    #     prepareAsg(config)
    #
    # if preparemc:
    #     pyasg.prepareAsgardMc(config)
    #
    # if leaderelection:
    #     leaderelectionAsg(config)
    #
    # if pingpongenable:
    #     pyasg.enablePingPong(config)
    #
    # if throughput:
    #     if not path.exists(config['asgard']['base_path']):
    #         print("asgard is not loaded")
    #         return
    #     try:
    #         pyasg.feedConsensusRequests(config, dumplogs)
    #
    #     except pyasg.ConfigurationError as e:
    #         print(f"Failed to Execute CMD {e.cmd}")
    #         print(f"Error Message {e.errors}")
    #         return
    #
    # if dumplogs:  # dump the logs to a file before we clean up asgard again!
    #     if not path.exists(config['asgard']['base_path']):
    #         print("asgard is not loaded")
    #         return
    #     run_id = pyasg.singleExecute(f"cat {config['asgard']['ctrl_proto_instance_uuid']}")
    #     pyasg.dump_consensus_logs(config, pyasg.getGitHash('sassy1'), run_id, config['asgard']['cluster_id'])
    #
    # if pingpong:
    #     if not path.exists(config['asgard']['base_path']):
    #         print("asgard is not loaded")
    #         return
    #     try:
    #         pyasg.enablePingPong(config)
    #         pyasg.doPingPong(config)
    #         results = pyasg.fetchPingPongResults(config)
    #         print(results)
    #     except pyasg.ConfigurationError as e:
    #         print(f"Failed to Execute CMD {e.cmd}")
    #         print(f"Error Message {e.errors}")
    #         return
    # if pingpongeval:
    #     try:
    #         cleanAsg(config)
    #         prepareHost('10.68.235.142', 'c', '')
    #         prepareHost('10.68.235.150', 'c', '')
    #         time.sleep(1)
    #
    #         prepareAsg(config)
    #         prepareHost('10.68.235.142', 'p', '')
    #         prepareHost('10.68.235.150' ,'p', '')
    #         time.sleep(1)
    #
    #         leaderelectionAsg(config)
    #         prepareHost('10.68.235.142', 'l', '')
    #         prepareHost('10.68.235.150' ,'l', '')
    #         time.sleep(1)
    #
    #         pyasg.enablePingPong(config)
    #         prepareHost('10.68.235.142', 'e', '')
    #         prepareHost('10.68.235.150','e', '')
    #         time.sleep(1)
    #         for lp in range(100):
    #             pyasg.singlewrite(config['asgard']['echo_pupu'], str(2))
    #             time.sleep(0.04)
    #             results = pyasg.fetchPingPongResults(config)
    #             # How to interpret the timestamps:
    #             # <EVENT>, timestamp in cpu cycles (2400MHz)
    #             # where event is defined in asgard kernel module sources as follows:
    #             # LOG_ECHO_RX_PING_UNI = 0,
    #             # LOG_ECHO_RX_PONG_UNI = 1,
    #             # LOG_ECHO_UNI_LATENCY_DELTA = 2, // timestamp is deltaof
    #             # rx_pong and tx_ping
    #             # LOG_ECHO_RX_PING_MULTI = 3,
    #             # LOG_ECHO_MULTI_LATENCY_DELTA = 4,
    #
    #             print((float(results[3:])-1000)/2400/2)
    #
    #         print("done eval ping pong")
    #         return
    #     except Exception as e:
    #         print(f"Failed to Execute CMD {e}")
    #         return


def load_config(config_path):
    config = None
    if os.path.exists(config_path):
        config = configparser.ConfigParser()
        config.read(config_path)
    return config


def leaderelectionAsg(config):
    if not path.exists(config['asgard']['base_path']):
        print("asgard is not loaded (start leader election)")
        return
    pyasg.startLeaderElection(config)
    print("Started leader election")


def do_configure_asgard(config):
    try:
        pyasg.configureSystem(config)
        pyasg.prepareAsgardPacemaker(config)
        print("Prepared ASGARD Instance for evaluation")
    except pyasg.ConfigurationError as e:
        print(f"Failed to Execute CMD {e.cmd}")
        print(f"Error Message {e.errors}")


def cleanAsg(config):
    if path.exists(config['asgard']['base_path']):
        pyasg.stopAsgard(config)
        pyasg.unloadAsgardModule()
        print("Cleaned local ASGARD Env")
    else:
        print("asgard is not loaded")
