# ##########
#
# NOT FINISHED - NOT WORKING YET - WORK IN PROGRESS
#
# ##########


import os.path

from cli.evaluation.evalRedis import *
from cli.evaluation.pyasgard.pyasgard import *

@click.command()
@click.option('--input', default='generated_data.json', show_default=True,
              help="Path to the evaluation json file, containing messages for replication")
@click.option('--config_path', default='example.asgard-bench.ini', show_default=True,
              help="Path to the configuration file. The redis leader and local IP/Port is configured in this config, as well as paths")
@click.option('--isleader', default=False, show_default=True, is_flag=True,
              help="Set this flag if this node should be configured as leader/master node")
@click.option('--requests', '-r', default=False, show_default=True,is_flag=True,
              help="If set data from the input file is read and requests are sent to the redis instance")
@click.option('--prepare', '-p', default=False, show_default=True,is_flag=True,
              help="If set starts a redis environment and if isleader is NOT set, this node registers as replica at the master")
@click.option('--clean', '-c', default=False, show_default=True,is_flag=True,
              help="If set cleans the dump.rdb file and kills redis-server instances on this node")
@click.option('--stats', '-s', default=False, show_default=True,is_flag=True,
              help="If set reads the eval_throughput_timestamps file (path in config.ini), and prints statistics")
def evalRedisASG(input, config_path, isleader, requests, prepare, clean, stats):

    if not os.path.exists(config_path):
        print("Config File {0} does not exist. Generate it via generate-config command".format(config_path))
        return False

    config = configparser.ConfigParser()
    config.read(config_path)

    if clean:
        clean_redis_env(config)
        print("Cleaned local Redis Env for evaluation")

    if prepare:

        if not prepareAsgardPacemaker(config):
            print("Could not boot up asgard")
            return

        startRedisAsgard(config)

        if not isleader:
            set_replica_of(config)

        print("Prepared Redis Instance for evaluation")

    if requests:
        load_and_send_requests(input, config)

    if stats:
        output_stats(config)

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

