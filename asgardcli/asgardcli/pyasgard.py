import json
import uuid
import subprocess
import time
import os
from time import sleep
from datetime import datetime


class ConfigurationError(Exception):
    def __init__(self, message, cmd, error):
        super().__init__(message)
        self.errors = error
        self.cmd = cmd

def singlewrite(path, value):
    if not os.path.isfile(path):
        return
    with open(path, "w") as file:
        try:
            file.write(value)
        except:
            print(f"write exception for {value} > {path}")
            pass

def doPingPong(config):

    for i in range(int(config['asgard']['ping_pong_rounds'])):
        singlewrite(config['asgard']['echo_pupu'], str(1))
        singlewrite(config['asgard']['echo_pupu'], str(2))
        singlewrite(config['asgard']['echo_pupu'], str(3))
        sleep(0.05)

    for i in range(int(config['asgard']['ping_pong_rounds'])):
        singlewrite(config['asgard']['echo_pupm'], str(1))
        singlewrite(config['asgard']['echo_pupm'], str(2))
        singlewrite(config['asgard']['echo_pupm'], str(3))
        sleep(0.05)

    for i in range(int(config['asgard']['ping_pong_rounds'])):
        singlewrite(config['asgard']['echo_pmpu'], str(1))
        sleep(0.05)

    for i in range(int(config['asgard']['ping_pong_rounds'])):
        singlewrite(config['asgard']['echo_pmpm'], str(1))
        sleep(0.05)


def enablePingPong(config):
    if not os.path.isdir(os.path.join(config['asgard']['base_path'], "proto_instances", "2")):
        # echo protocol has ID 0, Instance ID will be 2
        singlewrite(config['asgard']['ctrl_proto_instances'], "2,0")
        sleep(0.5)
    singlewrite(config['asgard']['ctrl_echo_log'], "1")
    singlewrite(config['asgard']['ctrl_echo_log2'], "1")


def setAsgardTimestamp(config, value):
    singlewrite(config['asgard']['log_user_a'], str(value))


def singleExecute(cmd):
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
    output, error = process.communicate()
    if error is not None:
        raise ConfigurationError("Failed to Run cmd", cmd, error)
    return output.rstrip().lstrip().decode("utf-8")


def loadAsgardModule(config, module_path):
    singleExecute(f"sudo insmod {module_path} ifindex={config['asgard']['iface']}")


def unloadAsgardModule(config):
    singleExecute("sudo rmmod asgard")


def cleanLogsOnNode(node_name, log_name):
    singleExecute(f"ssh dsp@{node_name} 'rm -f {log_name}'")


def downloadLogsFromTestNode(node_name, file_list, log_dir):
    try:
        singleExecute(f"scp dsp@{node_name}:{file_list} {log_dir}/")
    except ConfigurationError:
        return False
    return True


def stopAsgard(config):

    singlewrite(config['asgard']['ctrl_ts'], "0")
    singlewrite(config['asgard']['ctrl_consensus_eval'], "0")
    singlewrite(config['asgard']['ctrl_debug_path'], "0")

    # ansible sleeps 1 sec here

    singlewrite(config['asgard']['ctrl_consensus_le'], "0")
    singlewrite(config['asgard']['ctrl_ts'], "0")

    # ansible sleeps 1 sec here

    singlewrite(config['asgard']['ctrl_consensus_eval'], "0")

    # ansible sleeps 1 sec here

    singlewrite(config['asgard']['ctrl_pacemaker_path'], "0")

    time.sleep(1)

    singlewrite(config['asgard']['ctrl_proto_instances'], "-1")


def startLeaderElection(config):
    singlewrite(config['asgard']['ctrl_pacemaker_path'], "1")


def configureSystem(config):

    print("Configure NIC")
    singleExecute(f"sudo ethtool -K {config['asgard']['ifacename']} rx off tx off tso off gso off gro off lro off")
    singleExecute(f"sudo ethtool -C {config['asgard']['ifacename']} adaptive-rx off adaptive-tx off rx-usecs 0 rx-frames 0 rx-usecs-irq 0 rx-frames-irq 0 tx-usecs 0 tx-frames 0 tx-usecs-irq 0 tx-frames-irq 0")
    singleExecute(f"sudo ethtool -K {config['asgard']['ifacename']} ntuple on")

    print("Delete Previous Flow Rules")
    singleExecute(f"sudo ethtool --config-ntuple {config['asgard']['ifacename']} delete 1023")
    singleExecute(f"sudo ethtool --config-ntuple {config['asgard']['ifacename']} delete 1022")
    singleExecute(f"sudo ethtool --config-ntuple {config['asgard']['ifacename']} delete 1021")
    singleExecute(f"sudo ethtool --config-ntuple {config['asgard']['ifacename']} delete 1020")
    singleExecute(f"sudo ethtool --config-ntuple {config['asgard']['ifacename']} delete 1019")

    print("Distribute normal Traffic to RX Queues to Queues 0-10")
    singleExecute(f"sudo ethtool -X {config['asgard']['ifacename']} weight 1 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0")

    print("Steer default asgard Flow to asgard default queue")
    singleExecute(f"sudo ethtool -N {config['asgard']['ifacename']} flow-type udp4 dst-port 3319 action {config['asgard']['default_net_queue_id']}")
    singleExecute(f"sudo ethtool -N {config['asgard']['ifacename']} flow-type ether dst 01:00:5e:2b:d3:ea action {config['asgard']['default_net_queue_id']}")
    singleExecute(f"sudo ethtool -N {config['asgard']['ifacename']} flow-type ether dst 01:00:5e:2b:d3:ea action {config['asgard']['default_net_queue_id']}")

    print("steer leader flow")
    singleExecute(f"sudo ethtool -N {config['asgard']['ifacename']} flow-type udp4 dst-port 3320 action {config['asgard']['leader_net_queue_id']}")

    print("steer echo flow")
    singleExecute(f"sudo ethtool -N {config['asgard']['ifacename']} flow-type udp4 dst-port 3321 action {config['asgard']['echo_net_queue_id']}")

    print("get pci-bus id for iface")
    pci_bus_output = singleExecute(f"sudo ethtool -i {config['asgard']['ifacename']} | grep bus-info | cut -d ' ' -f2")

    print("get default IRQ number of asgard queue")
    irq_leader_output = singleExecute(f"grep mlx5_comp{config['asgard']['leader_net_queue_id']}@pci:{pci_bus_output} /proc/interrupts | cut -d ':' -f1")
    irq_default_output = singleExecute(f"grep mlx5_comp{config['asgard']['default_net_queue_id']}@pci:{pci_bus_output} /proc/interrupts | cut -d ':' -f1")
    irq_echo_output = singleExecute(f"grep mlx5_comp{config['asgard']['echo_net_queue_id']}@pci:{pci_bus_output} /proc/interrupts | cut -d ':' -f1")

    print("set CPU affinity of asgard RX Queue")
    singleExecute(f"sudo su -c 'echo \'800000,00000000\' > /proc/irq/{irq_leader_output}/smp_affinity'")
    singleExecute(f"sudo su -c 'echo \'800000,00000000\' > /proc/irq/{irq_default_output}/smp_affinity'")
    singleExecute(f"sudo su -c 'echo \'800000,00000000\' > /proc/irq/{irq_echo_output}/smp_affinity'")

    print("fix cpu frequency")
    singleExecute(f"sudo cpupower frequency-set --governor userspace")
    singleExecute(f"sudo cpupower --cpu all frequency-set --freq 2.4GHz")

    print("Disable Hung Task Warning")
    singleExecute(f"sudo su -c 'echo 0 > /proc/sys/kernel/hung_task_timeout_secs'")
    singleExecute(f"sudo su -c 'echo 1 > /sys/module/rcupdate/parameters/rcu_cpu_stall_suppress'")


def prepareAsgardProtocol(config, protocol_id, instance_id):
    singlewrite(config['asgard']['ctrl_proto_instances'], f"{instance_id},{protocol_id}")


def prepareConsensus(config):
    print("preparing consensus protocol for asgard")
    # TODO: Check protocol id for consensus
    prepareAsgardProtocol(config, 2, config['asgard']['consensus_instance_id'])
    singlewrite(config['asgard']['ctrl_consensus_le'], "0")
    singlewrite(config['asgard']['ctrl_consensus_le'], "2")
    singlewrite(config['asgard']['ctrl_consensus_le'], "1")
    singlewrite(config['asgard']['ctrl_max_entries'], f"2000000,2500000,70000000,90000000,{config['asgard']['val_max_entries']}")


def preparePingPong(config):
    print("preparing ping pong protocol for asgard")
    prepareAsgardProtocol(config, 3, config['asgard']['ping_pong_instance_id'])


def prepareAsgardPacemaker(config):
    print("preparing pacemaker for asgard")
    log_id = str(uuid.uuid4())

    singlewrite(config['asgard']['ctrl_pacemaker_path'], "0")
    singlewrite(config['asgard']['ctrl_debug_path'], "5")
    singlewrite(config['asgard']['ctrl_pacemaker_cluster_id'], config['asgard']['cluster_id'])
    singlewrite(config['asgard']['ctrl_pacemaker_cpu'], config['asgard']['pm_cpu'])
    singlewrite(config['asgard']['ctrl_hbi'], config['asgard']['val_hbi'])
    singlewrite(config['asgard']['ctrl_waiting_window'], config['asgard']['val_waiting_window'])

    singlewrite(config['asgard']['ctrl_proto_instance_uuid'], log_id)

    # clean protocol instances
    #singlewrite(config['asgard']['ctrl_proto_instances'], "-1")


    #singlewrite(config['asgard']['ctrl_ts'], "2")
    #singlewrite(config['asgard']['ctrl_ts'], "0")


    # singlewrite(config['asgard']['ctrl_consensus_throughput'], "2")
    # singlewrite(config['asgard']['ctrl_consensus_throughput'], "2")


    # singlewrite(config['asgard']['ctrl_consensus_throughput'], "1")


    # enableAsgardTimestamps(config)


def prepareAsgardMc(config):
    print("preparing multicast for asgard")

    singlewrite(config['asgard']['ctrl_multicast_enable'], "1")
    singlewrite(config['asgard']['ctrl_multicast_enable'], config['asgard']['val_multicast_delay'])


def fetchPingPongResults(config):
    f = open(config['asgard']['log_echo_log2'], "r")
    contents = f.read()
    f.close()
    return contents


def _stopTPEval(config):

    singlewrite(config['asgard']['ctrl_consensus_eval'], "0")


def getGitHash(nodeName):
    git_hash = subprocess.check_output(f"ssh {nodeName} 'uname -r' | cut -d'-' -f3", shell=True)
    git_hash = git_hash.rstrip().decode("utf-8")
    return git_hash


def dump_asgardredis_logs(config,  run_id, cluster_id):

    iface = config['asgard']['iface']
    lelog_tp = f"/home/dsp/logs/asgardredis/{cluster_id}_throughput.log"

    singleExecute(f'echo "RUN ID: { run_id }" >> {lelog_tp}')
    singleExecute(f'echo log_user_a: >> {lelog_tp}')
    singleExecute(f'cat /proc/asguard/{iface}/proto_instances/1/log_user_a >> {lelog_tp}')
    singleExecute(f'echo log_user_b: >> {lelog_tp}')
    singleExecute(f'cat /proc/asguard/{iface}/proto_instances/1/log_user_b >> {lelog_tp}')


def dump_consensus_logs(config, git_hash, run_id, cluster_id):

    max_entries_per_pkt = config['asgard']['val_max_entries']
    log_rep_eval_time = config['asgard']['consensus_eval_seconds']
    requests_per_sec = config['asgard']['consensus_requests_per_second']
    cluster_nodes = config['asgard']['number_of_test_nodes']
    repeat = config['asgard']['consensus_request_rounds']
    hbi = config['asgard']['val_hbi']
    ww = config['asgard']['val_waiting_window']
    iface = config['asgard']['iface']
    cur_date = datetime.today().strftime('%Y-%m-%d')
    cur_time = datetime.today().strftime('%H:%M:%S')
    lelog_tp = f"/home/dsp/logs/plainasgard/{cluster_id}_throughput.log"
    lelog_le = f"/home/dsp/logs/plainasgard/{cluster_id}_leader_election.log"

    singleExecute(f'echo "EVAL RESULTS" >> {lelog_le}')
    singleExecute(f'echo "RUN ID: { run_id }" >> {lelog_le}')
    singleExecute(f'echo "VERS: { git_hash }" >> {lelog_le}')
    singleExecute(f'echo "DATE: { cur_date }" >> {lelog_le}')
    singleExecute(f'echo "TIME: { cur_time }" >> {lelog_le}')
    singleExecute(f'echo "NODE: { cluster_id }" >> {lelog_le}')
    singleExecute(f'echo "CLUSTER SIZE: { cluster_nodes }" >> {lelog_le}')
    singleExecute(f'echo "MAX ENTRY PER PKT: { max_entries_per_pkt } " >> {lelog_le}')
    singleExecute(f'echo "HB INTERVAL: { hbi }" >> {lelog_le}')
    singleExecute(f'echo "WW INTERVAL: { ww }" >> {lelog_le}')
    singleExecute(f'echo "IFACE: { iface }" >> {lelog_le}')
    singleExecute(f'echo "START LOG" >> {lelog_le}')
    singleExecute(f'cat /proc/asguard/{iface}/proto_instances/1/log_consensus_le >> {lelog_le}')
    singleExecute(f'echo "END" >> {lelog_le}')

    singleExecute(f'echo "RUN ID: { run_id }" >> {lelog_tp}')
    singleExecute(f'cat /proc/asguard/{iface}/proto_instances/1/log_consensus_throughput >> {lelog_tp}')


def feedConsensusRequests(config, dumplogs):

    logs = []

    # start Consensus Requests
    singlewrite(config['asgard']['ctrl_consensus_eval'], config['asgard']['consensus_requests_per_second'])

    # wait for Configured amount of time
    sleep(int(config['asgard']['consensus_eval_seconds']))

    # Stop Consensus requests
    _stopTPEval(config)


