# ##########
#
# NOT FINISHED - NOT WORKING YET - WORK IN PROGRESS
#
# ##########

import time
from os import path

import redis
import click
import json
import os.path
from ..verification.verify import _verify_data
import subprocess
import configparser
import asgardcli.evaluation.pyasgard as pyasg


def read_from_file(filepath):

    with open(filepath) as f:
        data = json.loads(f.read())

    return data


def load_data_into_redis_db(config, data):
    r = redis.Redis(host=config['redis']['ip'], port=config['redis']['port'], db=0)

    for entry in data:
        key = entry['key']
        value = entry['value']

        # first time stamp here
        pyasg.setAsgardTimestamp(config, 1)
        r.set(key, value)
        # we did not receive the ack from all replicas yet, next timestamp is set in redis
        pyasg.setAsgardTimestamp(config, 2)


def set_replica_of(config):
    cmd = "{0} replicaof {1} {2}".format(config['redis']['redis_cli_bin'], config['redis']['leader_ip'], config['redis']['leader_port'] )

    process = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    process.wait()
    print("Set Node as replica")


def clean_redis_env(config):

    # Stop any running redis instances
    os.system("sudo killall redis-server")

    time.sleep(1)

    # remove the persisted rdb file
    if os.path.isfile("dump.rdb"):
        proc = subprocess.Popen(["sudo", "rm", "dump.rdb"],
                                stdout=subprocess.PIPE)
        proc.wait()

    # clear set_timestmap() redis eval traces
    if not os.path.isdir("history"):
        os.mkdir("history")

    if os.path.isfile("redis-vanilla.log"):
        timestr = time.strftime("%Y%m%d-%H%M%S")
        proc = subprocess.Popen(["sudo", "mv", "redis-vanilla.log", f"history/redis-vanilla-{timestr}.old.log"],
                                stdout=subprocess.PIPE)
        proc.wait()

    if os.path.isfile("redis-asgard.log"):
        timestr = time.strftime("%Y%m%d-%H%M%S")
        proc = subprocess.Popen(["sudo", "mv", "redis-asgard.log", f"history/redis-asgard-{timestr}.old.log"],
                                stdout=subprocess.PIPE)
        proc.wait()

    if os.path.isfile("redis_eval_throughput_ts.log"):
        proc = subprocess.Popen(["sudo", "mv", "redis_eval_throughput_ts.log", f"history/redis_eval_throughput_ts_{timestr}.old.log"],
                                stdout=subprocess.PIPE)
        proc.wait()

def startRedisAsgard(config):
    proc = subprocess.Popen(["sudo", config['redis']['redis_server_bin'], config['redis']['redis_asgard_conf']],
                             stdout=subprocess.PIPE)
    proc.wait()
    time.sleep(2)


def startRedisVanilla(config):
    proc = subprocess.Popen([config['redis']['redis_server_bin'], config['redis']['redis_vanilla_conf']],
                             stdout=subprocess.PIPE)

    proc.wait()
    time.sleep(2)


def get_timestamp_diff(ts_a, ts_b):
    return ((int(ts_b['timestamp_sec']) * 1000000000) + (int(ts_b['timestamp_nsec']))) - ((int(ts_a['timestamp_sec']) * 1000000000) + (int(ts_a['timestamp_nsec'])))


def load_and_send_requests(input, config):

    if not os.path.exists(input):
        print("File {0} does not exist. Generate Data via the generate-data command".format(input))
        return False

    data = read_from_file(input)

    if not _verify_data(16, 1024, data):
        print("Can not use Test Data for Redis Evaluation")
        return False

    load_data_into_redis_db(config, data)

    print("Eval Data loaded into Redis")


def convert_logs_to_dict(log_path):

    topLevelStats = dict()
    allNodes = dict()

    topLevelStats['node_stats'] = allNodes

    with open(log_path) as f:
        lines = f.read().splitlines()

    for line in lines:
        node_id_str = line.split(",")[0]

        if node_id_str not in allNodes:
            allNodes[node_id_str] = dict()
            allNodes[node_id_str]['total_bulk_replication_bytes'] = 0
            allNodes[node_id_str]['total_serial_replication_bytes'] = 0
            allNodes[node_id_str]['event_list'] = []
            allNodes[node_id_str]['serial_transmissions'] = 0
            allNodes[node_id_str]['serial_transmissions_list'] = []

        curNodeStat = allNodes[node_id_str]

        event_id_str = line.split(",")[1]
        timestamp_sec_str = line.split(",")[2].split(".")[0]
        timestamp_nsec_str = line.split(",")[2].split(".")[1]

        if int(event_id_str) == 1:
            if 'first_bulk_timestamp' not in curNodeStat:
                curNodeStat['first_bulk_timestamp'] = {
                    'timestamp_sec': timestamp_sec_str,
                    'timestamp_nsec':timestamp_nsec_str
                }

            if 'first_bulk_timestamp' not in topLevelStats:
                topLevelStats['first_bulk_timestamp'] = {
                    'timestamp_sec': timestamp_sec_str,
                    'timestamp_nsec':timestamp_nsec_str
                }

        if int(event_id_str) == 5:
            curNodeStat['last_bulk_timestamp'] = {
                'timestamp_sec': timestamp_sec_str,
                'timestamp_nsec':timestamp_nsec_str
            }
            topLevelStats['last_bulk_timestamp'] = {
                'timestamp_sec': timestamp_sec_str,
                'timestamp_nsec': timestamp_nsec_str
            }

        if int(event_id_str) == 6:
            preamble_bytes_str = line.split(",")[3]
            curNodeStat['event_list'].append({
                 'event':event_id_str,
                 'timestamp_sec':timestamp_sec_str,
                 'timestamp_nsec':timestamp_nsec_str,
                 'preamble_bytes': preamble_bytes_str
                 })
        elif int(event_id_str) == 7:
            payload_bytes_str = line.split(",")[3]
            curNodeStat['event_list'].append(
                {'event':event_id_str,
                 'timestamp_sec':timestamp_sec_str,
                 'timestamp_nsec':timestamp_nsec_str,
                 'bytes': payload_bytes_str
                 })
            curNodeStat['total_bulk_replication_bytes'] += int(payload_bytes_str)
        elif int(event_id_str) == 8:
            payload_bytes_str = line.split(",")[3]
            curNodeStat['event_list'].append(
                {'event':event_id_str,
                 'timestamp_sec':timestamp_sec_str,
                 'timestamp_nsec':timestamp_nsec_str,
                 'bytes': payload_bytes_str
                 })
            curNodeStat['serial_transmissions_list'].append({''})


            curNodeStat['serial_transmissions'] += 1

        else:
            curNodeStat['event_list'].append(
                {'event':event_id_str,
                 'timestamp_sec':timestamp_sec_str,
                 'timestamp_nsec':timestamp_nsec_str,
                 })

    # Handle Bulk Mode
    for node_id in allNodes:
        curNodeStats = allNodes[node_id]
        bytes = float(curNodeStats['total_bulk_replication_bytes'])
        curNodeStats['total_bulk_replication_mega_bytes'] = bytes * 0.000001
        curNodeStats['total_bulk_replication_bytes'] = bytes
        curNodeStats['total_bulk_time_nsec'] = get_timestamp_diff(curNodeStats['first_bulk_timestamp'], curNodeStats['last_bulk_timestamp'])
        curNodeStats['total_bulk_time_sec'] = curNodeStats['total_bulk_time_nsec'] * 0.0000000001
        curNodeStats['total_bulk_throughput'] = curNodeStats['total_bulk_replication_mega_bytes'] / curNodeStats['total_bulk_time_sec']

    # Handle Serial Mode
    for node_id in allNodes:
        curNodeStats = allNodes[node_id]

    topLevelStats['total_time_nsec'] = get_timestamp_diff(topLevelStats['first_bulk_timestamp'], topLevelStats['last_bulk_timestamp'])
    topLevelStats['total_time_sec'] = topLevelStats['total_time_nsec']  * 0.0000000001

    return topLevelStats


def print_stats(stats):
    nodeStats = stats['node_stats']
    for node_id in nodeStats :
        curNodeStats = nodeStats[node_id]
        print("Stats for Node", node_id)
        print("\tBulk Mode:")
        print("\t\tThroughput:", curNodeStats['total_bulk_throughput'], "MB/sec")
        print("\t\tTime until Replication for Node Complete: ", curNodeStats['total_bulk_time_sec'], "sec")
        print("\t\tMega Bytes", curNodeStats['total_bulk_replication_mega_bytes'])
        print("")
        print("\tSerial Mode:")

    print("Total Time until Replication Complete: ", stats['total_time_sec'], "sec")


def output_stats(config):
    if not os.path.exists(config['redis']['eval_throughput_timestamps']):
        print("File {0} does not exist. Generate Throughput Logs first".format(config['redis']['eval_throughput_timestamps']))
        return False

    topLevelStats = convert_logs_to_dict(config['redis']['eval_throughput_timestamps'])

    print_stats(topLevelStats)



@click.command()
@click.option('--input', default='generated_data.json', show_default=True,
              help="Path to the evaluation json file, containing messages for replication")
@click.option('--config_path', default='example.asgard-bench.ini', show_default=True,
              help="Path to the configuration file. The redis leader and local IP/Port is configured in this config, as well as paths")
@click.option('--isleader', '-i', default=False, show_default=True, is_flag=True,
              help="Set this flag if this node should be configured as leader/master node")
@click.option('--requests', '-r', default=False, show_default=True,is_flag=True,
              help="If set data from the input file is read and requests are sent to the redis instance")
@click.option('--prepare', '-p', default=False, show_default=True,is_flag=True,
              help="If set starts a redis environment and if isleader is NOT set, this node registers as replica at the master")
@click.option('--clean', '-c', default=False, show_default=True,is_flag=True,
              help="If set cleans the dump.rdb file and kills redis-server instances on this node")
@click.option('--stats', '-s', default=False, show_default=True,is_flag=True,
              help="If set reads the eval_throughput_timestamps file (path in config.ini), and prints statistics")
@click.option('--asgard', '-a', default=False, show_default=True,is_flag=True,
              help="If set uses asgard for redis")
@click.option('--dumplogs', '-d', default=False, show_default=True, is_flag=True,
              help="persists asgard redis logs to disk")
def eval_asgard_with_redis(input, config_path, isleader, requests, prepare, clean, stats, asgard, dumplogs):

    if not os.path.exists(config_path):
        print("Config File {0} does not exist. Generate it via generate-config command".format(config_path))
        return False

    config = configparser.ConfigParser()
    config.read(config_path)

    log_dir = os.path.join("/home/dsp", "logs", "plainredis")
    if not os.path.exists(log_dir):
        os.makedirs(log_dir)

    log_dir = os.path.join("/home/dsp", "logs", "asgardredis")
    if not os.path.exists(log_dir):
        os.makedirs(log_dir)

    if clean:
        clean_redis_env(config)
        print("Cleaned local Redis Env for evaluation")

    if prepare:
        if asgard:
            startRedisAsgard(config)
        else:
            startRedisVanilla(config)
        if not isleader and not asgard:
            time.sleep(1)  # wait until booted
            set_replica_of(config)

        print("Prepared Redis Instance for evaluation")

    if requests:
        load_and_send_requests(input, config)

    if stats:
        output_stats(config)


    if dumplogs:  # dump the logs to a file before we clean up asgard again!
        if not path.exists(config['asgard']['base_path']):
            print("asgard not loaded - no asgard logs to load")
            return
        run_id = pyasg.singleExecute(f"cat {config['asgard']['ctrl_proto_instance_uuid']}")
        pyasg.dump_asgardredis_logs(config, run_id, config['asgard']['cluster_id'])

    if not stats and not clean and not prepare and not requests:
        print("To print statistics:")
        print("\t --stats")
        print("Bulk Mode: run with the following parameters")
        print("\t1: on Leader: --clean --prepare --requests --isleader")
        print("\t2: on Followers: --clean --prepare")
        print("Serial Mode: run with the following parameters")
        print("\t1: on Leader: --clean --prepare --isleader")
        print("\t2: on Followers: --clean --prepare")
        print("\t3: on Leader: --requests")

