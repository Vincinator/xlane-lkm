# ##########
#
# NOT FINISHED - NOT WORKING YET - WORK IN PROGRESS
#
# ##########



import datetime
import time
import click
import os.path

from asgardcli.pyasgard import downloadLogsFromTestNode, cleanLogsOnNode
from asgardcli.utils import prepareHost
from multiprocessing import Pool


@click.command()
@click.option('--plainasgard',  default=False, show_default=True, is_flag=True)
@click.option('--plainasgardmc',  default=False, show_default=True, is_flag=True)
@click.option('--plainredis', default=False, show_default=True,is_flag=True)
@click.option('--asgardredis', default=False, show_default=True,is_flag=True)
@click.option('--etcd', default=False, show_default=True,is_flag=True)
def orchestrate(plainasgard, plainasgardmc, plainredis, asgardredis, etcd):

    test_node_ips = ['10.68.235.140','10.68.235.142', '10.68.235.150' ]

    if plainasgardmc:
        print("Evaluating Vanilla Asgard in Multicast Mode")

        cleanPrepareLeaderElectionAsgardMC(test_node_ips)

        print(f"Starting throughput evaluation on test nodes {test_node_ips} ... ")

        throughputAsgard(test_node_ips)

        print("creating local log directories")

        log_dir = os.path.join(os.getcwd(), "logs", "plainasgard_mc", datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S'))
        os.makedirs(log_dir)
        time.sleep(5)
        print(f"downloading timestamps to {log_dir} now (this may take a while) ... ")
        gotData = downloadLogsFromTestNodes(test_node_ips, "logs/plainasgard/*", log_dir)

        cleanLogsOnNodes(test_node_ips, "logs/plainasgard/*")

        cleanAsgard(test_node_ips)
        print("cleaned up environment")

    if plainasgard:
        print("Evaluating Vanilla Asgard")

        cleanPrepareLeaderElectionAsgard(test_node_ips)

        print(f"Starting throughput evaluation on test nodes {test_node_ips} ... ")

        throughputAsgard(test_node_ips)

        print("creating local log directories")

        log_dir = os.path.join(os.getcwd(), "logs", "plainasgard", datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S'))
        os.makedirs(log_dir)
        time.sleep(5)
        print(f"downloading timestamps to {log_dir} now (this may take a while) ... ")
        gotData = downloadLogsFromTestNodes(test_node_ips, "logs/plainasgard/*", log_dir)

        cleanLogsOnNodes(test_node_ips, "logs/plainasgard/*")

        cleanAsgard(test_node_ips)
        print("cleaned up environment")

    if plainredis:
        print("Evaluating Vanilla Redis")

        cleanPreparePlainRedis(test_node_ips, test_node_ips[0])

        print("generateing Requests on Leader")
        generateRequestsRedis(test_node_ips[0])

        log_dir = os.path.join(os.getcwd(), "logs", "plainredis", datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S'))
        os.makedirs(log_dir)

        print(f"download redis logs into {log_dir}")
        gotData = downloadLogsFromTestNodes(test_node_ips, "logs/asgardredis/*", log_dir)

        # cleanLogsOnNodes(test_node_ips, "logs/asgardredis/*.log")

        print("clean Redis Env")
        cleanRedis(test_node_ips)


    if asgardredis:
        print("Evaluating Vanilla Redis")

        # asgard must be running with an elected leader
        cleanPrepareLeaderElectionAsgard(test_node_ips)

        cleanPrepareAsgardRedis(test_node_ips)

        print("generateing Requests on Leader")
        generateRequestsRedis(test_node_ips[0])

        log_dir = os.path.join(os.getcwd(), "logs", datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S'))
        os.makedirs(log_dir)

        print(f"download redis logs into {log_dir}")
        gotData = downloadLogsFromTestNodes(test_node_ips, "logs/plainredis/*", log_dir)

        time.sleep(5)

        cleanRedis(test_node_ips) # MUST clean up Redis before we clean up asgard

        cleanAsgard(test_node_ips)
        print("cleaned up environment")

    if etcd:
        print("Evaluating ETCD")

def cleanLogsOnNodes(test_node_ips, log_name):
    pool = Pool()
    for ip in test_node_ips:
        pool.apply_async(cleanLogsOnNode, args=(ip,log_name))
    pool.close()
    pool.join()


def downloadLogsFromTestNodes(test_node_ips, file_list, log_dir):
    pool = Pool()
    for ip in test_node_ips:
        pool.apply_async(downloadLogsFromTestNode, args=(ip, file_list, log_dir))
    pool.close()
    pool.join()


def throughputAsgard(test_node_ips):
    pool = Pool()
    for ip in test_node_ips:
        pool.apply_async(prepareHost, args=(ip, "evalasgard", "-td"))
    pool.close()
    pool.join()


def cleanPrepareLeaderElectionAsgardMC(test_node_ips):
    pool = Pool()
    for ip in test_node_ips:
        pool.apply_async(prepareHost, args=(ip, "evalasgard", "-cpml"))
    pool.close()
    pool.join()


def cleanPrepareLeaderElectionAsgard(test_node_ips):
    pool = Pool()
    for ip in test_node_ips:
        pool.apply_async(prepareHost, args=(ip, "evalasgard", "-cpl"))
    pool.close()
    pool.join()


def generateRequestsRedis(leader_ip):
    pool = Pool()
    pool.apply_async(prepareHost, args=(leader_ip, "evalredis", "-rd"))
    pool.close()
    pool.join()

def cleanPrepareAsgardRedis(test_node_ips):
    pool = Pool()
    for ip in test_node_ips:
        pool.apply_async(prepareHost, args=(ip, "evalredis", "-cpa"))
    pool.close()
    pool.join()

def cleanPreparePlainRedis(test_node_ips, leader_ip):
    pool = Pool()
    for ip in test_node_ips:
        if ip == leader_ip:
            pool.apply_async(prepareHost, args=(ip, "evalredis", "-cpal"))
        else:
            pool.apply_async(prepareHost, args=(ip, "evalredis", "-cpa"))
    pool.close()
    pool.join()

def cleanRedis(test_node_ips):
    pool = Pool()
    for ip in test_node_ips:
        pool.apply_async(prepareHost, args=(ip, "evalredis", "-c"))
    pool.close()
    pool.join()

def cleanAsgard(test_node_ips):
    pool = Pool()
    for ip in test_node_ips:
        pool.apply_async(prepareHost, args=(ip, "evalasgard", "-c"))
    pool.close()
    pool.join()
