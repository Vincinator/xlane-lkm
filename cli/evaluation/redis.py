import time

import redis
import click

import binascii
import json
import os.path
from ..verification.verify import _verify_data
import subprocess
import configparser

def read_from_file(filepath):

    with open(filepath) as f:
        data = json.loads(f.read())

    return data


def load_data_into_redis_db(config, data):
    r = redis.Redis(host=config['redis']['ip'], port=config['redis']['port'], db=0)

    for entry in data:
        key = binascii.unhexlify(entry['key'])
        value = binascii.unhexlify(entry['value'])
        r.set(key, value)


def set_replica_of(config):
    process = subprocess.run([config['redis']['redis_cli_bin'], 'replicaof', config['redis']['leader_ip'], config['redis']['leader_port']],
                             stdout=subprocess.PIPE)
    process


def clean_redis_env(config):

    # Stop any running redis instances
    proc = subprocess.Popen(["pkill", "-f", config['redis']['redis_server_bin']], stdout=subprocess.PIPE)
    proc.wait()

    # remove the persisted rdb file
    proc = subprocess.Popen(["rm", "-f", config['redis']['redis_dump_rdb']], stdout=subprocess.PIPE)
    proc.wait()

    time.sleep(1)


def start_redis_server(config):
    proc = subprocess.Popen([config['redis']['redis_server_bin'], config['redis']['redis_vanilla_conf']],
                             stdout=subprocess.PIPE)
    print(proc)

    proc.wait()
    time.sleep(2)




@click.command()
@click.option('--input', default='generated_data.json')
@click.option('--config_path', default='example.asgard-bench.ini')
@click.option('--isleader', default=0)
def evalRedis(input, config_path,  isleader):

    if not os.path.exists(config_path):
        print("Config File {0} does not exist. Generate it via generate-config command".format(config_path))
        return False

    config = configparser.ConfigParser()
    config.read(config_path)

    # make sure that we start in a clean env
    clean_redis_env(config)
    print("Cleaned local Redis Env")

    start_redis_server(config)

    print("Started Redis Instance")

    if isleader == 1:

        if not os.path.exists(input):
            print("File {0} does not exist. Generate Data via the generate-data command".format(input))
            return False

        asgardBenchData = read_from_file(input)

        if not _verify_data(16, 1024, asgardBenchData):
            print("Can not use Test Data for Redis Evaluation")
            return False

        load_data_into_redis_db(config, asgardBenchData)

        print("Eval Data loaded into Redis")
    else:
        set_replica_of(config)
        print("Set Redis instane to be a replica")

