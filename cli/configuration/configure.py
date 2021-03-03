import configparser
import json

import click
import os.path


@click.command()
@click.option('--config_path', prompt=True, default='example.asgard-bench.ini')
@click.option("--redis_base", default='../redis-bin')
@click.option('--redis_leader_port', default='6379')
@click.option('--redis_port', default='6379')
@click.option('--redis_leader_ip', prompt=True, default='10.68.235.140')
@click.option('--redis_ip', default='127.0.0.1')
@click.option('--asgard_iface', prompt=True, default='4')
@click.option('--asgard_ifacename', prompt=True, default='enp130s0f0')
@click.option('--asgard_ping_pong_instance_id', default='2')
@click.option('--asgard_hbi', default='2400000')
@click.option('--asgard_waiting_window', default='5000')
@click.option('--asgard_max_entries', default='160')
@click.option('--asgard_targets_string', default='(10.68.235.140,24:8a:07:29:25:ba,1,1),(10.68.235.142,24:8a:07:29:25:92,1,2),(10.68.235.150,24:8a:07:29:25:82,1,3')
@click.option('--asgard_pm_cpu', default='52')
@click.option('--asgard_cluster_id', prompt=True, default='1')
@click.option('--asgard_ping_pong_rounds', default='1000')
@click.option('--asgard_consensus_requests_per_second', prompt=True, default='10000')
@click.option('--asgard_consensus_request_rounds', prompt=True, default='1')
@click.option('--asgard_leader_net_queue_id',  default='53')
@click.option('--asgard_echo_net_queue_id',  default='51')
@click.option('--asgard_default_net_queue_id',  default='55')
@click.option('--asgard_multicast_delay', prompt=True, default='1000')

@click.option('--num_of_targets', prompt=True, type=click.Choice(["3", "5", "7", "9"]), required=True)

def generateConfig(config_path, redis_base, redis_leader_port, redis_port, redis_leader_ip,
                   redis_ip, asgard_iface, asgard_ifacename, asgard_ping_pong_instance_id, asgard_hbi, asgard_waiting_window, asgard_max_entries,
                   asgard_targets_string, asgard_pm_cpu, asgard_cluster_id, asgard_ping_pong_rounds, asgard_consensus_requests_per_second, asgard_consensus_request_rounds,
                   asgard_leader_net_queue_id, asgard_echo_net_queue_id, asgard_default_net_queue_id, asgard_multicast_delay, num_of_targets):

    config = configparser.ConfigParser()
    config['redis'] = {}
    config['redis']['leader_port'] = redis_leader_port
    config['redis']['leader_ip'] =  redis_leader_ip
    config['redis']['port'] = redis_port
    config['redis']['ip'] =  redis_ip
    config['redis']['redis_server_bin'] = os.path.join(os.path.abspath(redis_base), 'redis-server')
    config['redis']['redis_cli_bin'] = os.path.join(os.path.abspath(redis_base), 'redis-cli')
    config['redis']['redis_vanilla_log'] = os.path.join(os.path.abspath(redis_base), 'redis-vanilla.log')
    config['redis']['redis_asgard_log'] =  os.path.join(os.path.abspath(redis_base), 'redis-asgard.log')
    config['redis']['redis_vanilla_conf'] = os.path.join(os.path.abspath(redis_base), 'redis-cluster.conf')
    config['redis']['redis_asgard_conf'] = os.path.join(os.path.abspath(redis_base), 'redis-asgard.conf')
    config['redis']['redis_dump_rdb'] =  'dump.rdb'
    config['redis']['eval_throughput_timestamps'] = 'redis_eval_throughput_ts.log'

    config['asgard'] = {}

    config['asgard']['iface'] = asgard_iface
    config['asgard']['ifacename'] = asgard_ifacename
    config['asgard']['cluster_id'] = asgard_cluster_id

    config['asgard']['number_of_test_nodes'] = num_of_targets

    if  config['asgard']['number_of_test_nodes'] == "3":
        config['asgard']['test_nodes'] = json.dumps({"nodes": ["sassy1", "sassy2", "sassy3" ]})
    elif config['asgard']['number_of_test_nodes'] == "5":
        config['asgard']['test_nodes'] = json.dumps({"nodes": ["sassy1", "sassy2", "sassy3", "sassy4", "sassy5" ]})
    elif config['asgard']['number_of_test_nodes'] == "7":
        config['asgard']['test_nodes'] = json.dumps({"nodes": ["sassy1", "sassy2", "sassy3", "sassy4", "sassy5", "sassy6", "sassy7" ]})
    elif config['asgard']['number_of_test_nodes'] == "9":
        config['asgard']['test_nodes'] =json.dumps({"nodes": ["sassy1", "sassy2", "sassy3", "sassy4", "sassy5", "sassy6", "sassy7", "sassy8", "sassy9"]})
    else:
        print("BUG: Please choose between 3,5,7 or 9 target nodes..")
        return


    config['asgard']['consensus_requests_per_second'] = asgard_consensus_requests_per_second
    config['asgard']['consensus_request_rounds'] = asgard_consensus_request_rounds
    config['asgard']['consensus_eval_seconds'] = "2"

    config['asgard']['base_path'] = os.path.join(os.path.abspath('/proc/asguard'), asgard_iface)
    config['asgard']['proto_instance_base_path'] =  os.path.join(os.path.abspath(config['asgard']['base_path']), "proto_instances")
    config['asgard']['proto_ping_pong_instance_path'] =  os.path.join(os.path.abspath(config['asgard']['base_path']), "proto_instances", asgard_ping_pong_instance_id )

    config['asgard']['leader_net_queue_id'] = asgard_leader_net_queue_id
    config['asgard']['echo_net_queue_id'] = asgard_echo_net_queue_id
    config['asgard']['default_net_queue_id'] = asgard_default_net_queue_id

    # Build path:  os.path.join(os.path.abspath(config['asgard']['proto_instance_base_path']), instance_id, name_of_endpoint)
    config['asgard']['log_user_a_endpoint'] = "log_user_a"
    config['asgard']['log_user_b_endpoint'] = "log_user_b"
    config['asgard']['log_user_c_endpoint'] = "log_user_c"
    config['asgard']['log_user_d_endpoint'] = "log_user_d"
    config['asgard']['log_consensus_throughput_endpoint'] = "log_consensus_throughput"
    config['asgard']['log_consensus_le_endpoint'] = "log_consensus_le"
    config['asgard']['ctrl_user_a_endpoint'] = "ctrl_user_a"
    config['asgard']['ctrl_user_b_endpoint'] = "ctrl_user_b"
    config['asgard']['ctrl_user_c_endpoint'] = "ctrl_user_c"
    config['asgard']['ctrl_user_d_endpoint'] = "ctrl_user_d"
    config['asgard']['ctrl_consensus_throughput_endpoint'] = "ctrl_consensus_throughput"
    config['asgard']['ctrl_consensus_le_endpoint'] =  "ctrl_consensus_le"

    config['evalLogs'] = {}

    config['evalLogs']['asg_vanilla_throughput'] = os.path.join("/home/dsp/logs/")
    config['evalLogs']['asg_vanilla_leader_election'] = os.path.join("/home/dsp/logs/")

    config['asgard']['ctrl_echo_log2'] = os.path.join(os.path.abspath(config['asgard']['proto_echo_instance_path']), "ctrl_echo_log2")
    config['asgard']['ctrl_echo_log'] = os.path.join(os.path.abspath(config['asgard']['proto_echo_instance_path']), "ctrl_echo_log")
    config['asgard']['log_echo_log2'] = os.path.join(os.path.abspath(config['asgard']['proto_echo_instance_path']), "log_echo_log2")
    config['asgard']['log_echo_log'] = os.path.join(os.path.abspath(config['asgard']['proto_echo_instance_path']), "log_echo_log")

    config['asgard']['echo_pupu'] = os.path.join(os.path.abspath(config['asgard']['proto_echo_instance_path']), "ping_unicast_pong_unicast")
    config['asgard']['echo_pmpm'] = os.path.join(os.path.abspath(config['asgard']['proto_echo_instance_path']), "ping_multicast_pong_multicast")
    config['asgard']['echo_pupm'] = os.path.join(os.path.abspath(config['asgard']['proto_echo_instance_path']), "ping_unicast_pong_multicast")
    config['asgard']['echo_pmpu'] = os.path.join(os.path.abspath(config['asgard']['proto_echo_instance_path']), "ping_multicast_pong_unicast")
    config['asgard']['ping_pong_rounds'] = asgard_ping_pong_rounds


    config['asgard']['ctrl_pacemaker_path'] = os.path.join(os.path.abspath(config['asgard']['base_path']), "pacemaker", "ctrl")
    config['asgard']['ctrl_debug_path'] = os.path.join(os.path.abspath(config['asgard']['base_path']), "debug")


    config['asgard']['module_path'] = os.path.join("/home/dsp", "asguard-bin", "asguard.ko")

    config['asgard']['ctrl_proto_instances'] = os.path.join(os.path.abspath(config['asgard']['base_path']), "proto_instances", "ctrl")
    config['asgard']['ctrl_pacemaker_targets'] = os.path.join(os.path.abspath(config['asgard']['base_path']), "pacemaker", "targets")
    config['asgard']['ctrl_pacemaker_cpu'] = os.path.join(os.path.abspath(config['asgard']['base_path']), "pacemaker", "cpu")
    config['asgard']['ctrl_pacemaker_cluster_id'] = os.path.join(os.path.abspath(config['asgard']['base_path']), "pacemaker", "cluster_id")

    config['asgard']['ctrl_ts'] = os.path.join(os.path.abspath(config['asgard']['base_path']), "ts", "ctrl")

    config['asgard']['ctrl_consensus_le'] = os.path.join(os.path.abspath(config['asgard']['base_path']), "proto_instances", asgard_proto_instance, "ctrl_consensus_le")
    config['asgard']['ctrl_consensus_throughput'] = os.path.join(os.path.abspath(config['asgard']['base_path']), "proto_instances", asgard_proto_instance, "ctrl_consensus_throughput")
    config['asgard']['ctrl_consensus_eval'] = os.path.join(os.path.abspath(config['asgard']['base_path']), "proto_instances", asgard_proto_instance, "consensus_eval_ctrl")


    config['asgard']['ctrl_consensus_eval'] = os.path.join(os.path.abspath(config['asgard']['base_path']), "proto_instances", asgard_proto_instance, "consensus_eval_ctrl")


    config['asgard']['ctrl_proto_instance_uuid'] = os.path.join(os.path.abspath(config['asgard']['base_path']), "proto_instances", asgard_proto_instance, "uuid")

    config['asgard']['ctrl_hbi'] = os.path.join(os.path.abspath(config['asgard']['base_path']), "pacemaker", "hbi")
    config['asgard']['val_hbi'] = asgard_hbi

    config['asgard']['ctrl_waiting_window'] = os.path.join(os.path.abspath(config['asgard']['base_path']), "pacemaker", "waiting_window")
    config['asgard']['val_waiting_window'] = asgard_waiting_window

    config['asgard']['ctrl_max_entries'] = os.path.join(os.path.abspath(config['asgard']['base_path']), "proto_instances", asgard_proto_instance, "le_config")
    config['asgard']['val_max_entries'] = asgard_max_entries

    config['asgard']['targets_string'] = asgard_targets_string

    config['asgard']['PM_CPU'] = asgard_pm_cpu

    config['asgard']['cpu_freq'] = "2400"

    config['asgard']['ctrl_multicast_enable'] = os.path.join(os.path.abspath(config['asgard']['base_path']),
                                                        "multicast", "enable")
    config['asgard']['ctrl_multicast_delay'] = os.path.join(os.path.abspath(config['asgard']['base_path']),
                                                        "multicast", "delay")
    config['asgard']['val_multicast_delay'] = asgard_multicast_delay


    with open(config_path, 'w') as configfile:
        config.write(configfile)







