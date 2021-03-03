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


@click.command()
@click.option('--config_path', default='example.asgard-bench.ini', show_default=True,
              help="Path to the configuration file. The redis leader and local IP/Port is configured in this config, as well as paths")
@click.option('--prepare', '-p', default=False, show_default=True,is_flag=True,
              help="Prepare asguard in general")
@click.option('--preparemc', '-m', default=False, show_default=True,is_flag=True,
              help="prepare asguard for multicast log replication")
@click.option('--clean','-c', default=False, show_default=True,is_flag=True,
              help="If set cleans the dump.rdb file and kills redis-server instances on this node")
@click.option('--leaderelection', '-l', default=False, show_default=True,is_flag=True,
              help="If set starts a leader election")
@click.option('--pingpongenable', '-e', default=False, show_default=True, is_flag=True,
              help="If set enables ping pong protocol")
@click.option('--pingpong', default=False, show_default=True, is_flag=True,
              help="If set evaluates latencies via the echo ping pong protocol")
@click.option('--pingpongeval', default=False, show_default=True, is_flag=True,
              help="If set evaluates latencies via the echo ping pong protocol")
@click.option('--throughput', '-t', default=False, show_default=True, is_flag=True,
              help="If set evaluates latencies via the echo ping pong protocol")
@click.option('--ifindex', default=-1, show_default=True,
              help="Uses this parameter as ifindex instead of ifindex from .ini file")
@click.option('--log_dir', default="~/logs", show_default=True,
              help="Directory where the timestamp logs are downloaded to")
@click.option('--report', default=False, show_default=True, is_flag=True,
              help="Generates reports for all available data")
@click.option('--dumplogs', '-d', default=False, show_default=True, is_flag=True,
              help="persists logs to disk")
def evalAsgard(config_path, prepare, preparemc, clean, leaderelection, pingpongenable, pingpong, pingpongeval, throughput, ifindex, log_dir, report, dumplogs):

    if not os.path.exists(config_path):
        print("Config File {0} does not exist. Generate it via generate-config command".format(config_path))
        config = None
    #    return False
    else:
        config = configparser.ConfigParser()
        config.read(config_path)


    if ifindex != -1:
        config['asgard']['iface'] = f"{ifindex}"

    if clean:
        cleanAsg(config)

    if prepare:
        log_dir = os.path.join("/home/dsp", "logs", "plainasgard")
        if not os.path.exists(log_dir):
            os.makedirs(log_dir)
        prepareAsg(config)

    if preparemc:
        pyasg.prepareAsgardMc(config)

    if leaderelection:
        leaderelectionAsg(config)

    if pingpongenable:
        pyasg.enablePingPong(config)

    if throughput:
        if not path.exists(config['asgard']['base_path']):
            print("asgard is not loaded")
            return
        try:
            pyasg.feedConsensusRequests(config, dumplogs)

        except pyasg.ConfigurationError as e:
            print(f"Failed to Execute CMD {e.cmd}")
            print(f"Error Message {e.errors}")
            return

    if dumplogs:  # dump the logs to a file before we clean up asgard again!
        if not path.exists(config['asgard']['base_path']):
            print("asgard is not loaded")
            return
        run_id = pyasg.singleExecute(f"cat {config['asgard']['ctrl_proto_instance_uuid']}")
        pyasg.dump_consensus_logs(config, pyasg.getGitHash('sassy1'), run_id, config['asgard']['cluster_id'])

    if pingpong:
        if not path.exists(config['asgard']['base_path']):
            print("asgard is not loaded")
            return
        try:
            pyasg.enablePingPong(config)
            pyasg.doPingPong(config)
            results = pyasg.fetchPingPongResults(config)
            print(results)
        except pyasg.ConfigurationError as e:
            print(f"Failed to Execute CMD {e.cmd}")
            print(f"Error Message {e.errors}")
            return
    if pingpongeval:
        try:
            cleanAsg(config)
            prepareHost('10.68.235.142', 'c', '')
            prepareHost('10.68.235.150', 'c', '')
            time.sleep(1)

            prepareAsg(config)
            prepareHost('10.68.235.142', 'p', '')
            prepareHost('10.68.235.150' ,'p', '')
            time.sleep(1)

            leaderelectionAsg(config)
            prepareHost('10.68.235.142', 'l', '')
            prepareHost('10.68.235.150' ,'l', '')
            time.sleep(1)

            pyasg.enablePingPong(config)
            prepareHost('10.68.235.142', 'e', '')
            prepareHost('10.68.235.150','e', '')
            time.sleep(1)
            for lp in range(100):
                pyasg.singlewrite(config['asgard']['echo_pupu'], str(2))
                time.sleep(0.04)
                results = pyasg.fetchPingPongResults(config)
                # How to interpret the timestamps:
                # <EVENT>, timestamp in cpu cycles (2400MHz)
                # where event is defined in asgard kernel module sources as follows:
                # LOG_ECHO_RX_PING_UNI = 0,
                # LOG_ECHO_RX_PONG_UNI = 1,
                # LOG_ECHO_UNI_LATENCY_DELTA = 2, // timestamp is deltaof
                # rx_pong and tx_ping
                # LOG_ECHO_RX_PING_MULTI = 3,
                # LOG_ECHO_MULTI_LATENCY_DELTA = 4,

                print((float(results[3:])-1000)/2400/2)

            print("done eval ping pong")
            return
        except Exception as e:
            print(f"Failed to Execute CMD {e}")
            return


def leaderelectionAsg(config):
    if not path.exists(config['asgard']['base_path']):
        print("asgard is not loaded (start leader election)")
        return
    pyasg.startLeaderElection(config)
    print("Started leader election")


def prepareAsg(config):
    try:
        pyasg.configureSystem(config)
        pyasg.loadAsgardModule(config)
        pyasg.prepareAsgard(config)
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
