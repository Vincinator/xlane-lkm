import matplotlib.pyplot as plt

import numpy as np


def plotHistogram(data):
    plt.figure(1)
    n, bins, patches = plt.hist(data, 50, density=1, facecolor='g', alpha=0.75)
    plt.show()


def convertListOfRAWLatenciesToListOfMicrosecLatencies(listOfRawLatencies, cpu_freq_mhz):

    reslist = []

    for rawlat in listOfRawLatencies:
        convertedVal = rawlat / cpu_freq_mhz
        reslist.append(convertedVal)
        print(convertedVal)

    return reslist


def getListOfRAWLatenciesFromListOfEvents(listOfEvents, start_descriptor, end_descriptor):

    reslist = []

    for event in listOfEvents:
        if not start_descriptor in event:
            continue
        if not end_descriptor in event:
            continue
        delta = event[end_descriptor] - event[start_descriptor]
        reslist.append(delta)

    return reslist


# Requires the CSV to be ordered in events, and first event must be 1
# 1, <ts>
# 2, <ts>
# ....
# n, <ts>
def convertAsgardOrderedCSVtoListOfEvents(filepath):

    reslist = []

    with open(filepath) as fp:

        for cnt, line in enumerate(fp):

            event = int(line.split(', ')[0])
            ts = int(line.split(', ')[1])

            if event == 1:
                reslist.append({'ts{0}'.format(event): ts})
            else:
                cur_element = reslist[-1]
                cur_element['ts{0}'.format(event)] = ts
    return reslist


def got_new_leader(line):
    if('2, ' in line): # FOLLOWER_ACCEPT_NEW_LEADER = 2,
        return 1
    if('3, ' in line): # CANDIDATE_ACCEPT_NEW_LEADER = 3,
        return 1
    if('4, ' in line): # LEADER_ACCEPT_NEW_LEADER = 4,
        return 1
    if('5, ' in line): # CANDIDATE_BECOME_LEADER = 6,
        return 1
    return 0


def vanillaAsgardGetTimestampsForTarget(config, hosts, filepath, cluster_id):
    hosts[cluster_id] = dict()
    hosts[cluster_id]['runs'] = dict()

    scan_started = 0
    scan_found_le_start = 0
    cpu_freq = int(config['asgard']['cpu_freq'])

    for line in open(f'{filepath}/{cluster_id}_leader_election.log'):
        if "RUN ID:" in line:
            cur_run_id = line.split("RUN ID: ", 1)[1].rstrip()
            hosts[cluster_id]['runs'][cur_run_id] = dict()
            hosts[cluster_id]['runs'][cur_run_id]['le_delta_microsec'] = list()
            hosts[cluster_id]['runs'][cur_run_id]['le_delta_cycles'] = list()
            hosts[cluster_id]['runs'][cur_run_id]['throughput_total'] = 0
            scan_started = 1

        if "7, " in line:
            if scan_started == 0:
                continue
            scan_found_le_start = 1
            start_le_ts = line.split("7, ", 1)[1]
            hosts[cluster_id]['runs'][cur_run_id]['le_start'] = start_le_ts

        if "14, " in line:
            if scan_started == 0:
                continue
            log_rep_start = line.split("14, ", 1)[1]
            hosts[cluster_id]['runs'][cur_run_id]['log_rep_start'] = log_rep_start

        if "13, " in line:
            if scan_started == 0:
                continue
            cur_last_log_rep_commit = line.split("13, ", 1)[1]
            hosts[cluster_id]['runs'][cur_run_id]['log_rep_last'] = cur_last_log_rep_commit

        if got_new_leader(line):
            if scan_found_le_start == 0:
                continue
            end_le_ts = line.split(", ", 1)[1]
            delta = int(end_le_ts) - int(start_le_ts)
            hosts[cluster_id]['runs'][cur_run_id]['le_delta_microsec'].append(delta / cpu_freq)
            hosts[cluster_id]['runs'][cur_run_id]['le_delta_cycles'].append(delta)
            hosts['le_delta_cycles'].append(delta)
            hosts['le_delta_microsec'].append(delta / int(config['asgard']['cpu_freq']))
            scan_started = 0
            scan_found_le_start = 0


def vanillaAsgardGetThroughputTotalForTarget(hosts, filepath, cluster_id):
    throughput_total = 0
    for line in open(f'{filepath}/{cluster_id}_throughput.log'):
        if "RUN ID:" in line:
            cur_run_id = line.split("RUN ID: ", 1)[1].rstrip()
        else:
           hosts[cluster_id]['runs'][cur_run_id]['throughput_total'] += int(line.split(", ", 1)[0])

    return throughput_total


def vanillaAsgardCalculateThroughput(config, hosts, cluster_id, throughputs_mbs):
    for key, value in hosts[cluster_id]['runs'].items():
        cur_total = value['throughput_total']
        requests_per_sec = config['asgard']['consensus_requests_per_second']
        log_rep_eval_time = config['asgard']['consensus_eval_seconds']
        cpu_freq = int(config['asgard']['cpu_freq'])

        if cur_total == 0:
            continue
        print("Throughput Statistics for host ", cluster_id, "with run-id: ", key)

        cur_tdelta = (int(value['log_rep_last']) - int(value['log_rep_start'])) / cpu_freq  # in microseconds

        cur_tdelta = (cur_tdelta / 1000) / 1000  # in seconds
        print("\t total transmission time in seconds:", cur_tdelta)
        print("\t transmitted entries: ", cur_total)
        if cur_tdelta == 0:
            continue
        entries_per_sec = cur_total / cur_tdelta
        bytes_per_sec = (cur_total * 8) / cur_tdelta  # 8 bytes per entry is payload

        print("\t", bytes_per_sec, "byte/sec")
        print("\t", bytes_per_sec * 0.001, "kb/sec")
        print("\t", bytes_per_sec * 0.001 * 0.001, "mb/sec")
        throughputs_mbs = np.append(throughputs_mbs, bytes_per_sec * 0.001 * 0.001)
        failures = np.append(failures, cur_total != ((requests_per_sec * log_rep_eval_time - 1) * log_rep_eval_time))
    return throughputs_mbs


def vanillaAsgardPrintReport(config, throughputs_mbs, failures):

    max_entries_per_pkt = config['asgard']['val_max_entries']
    log_rep_eval_time = config['asgard']['consensus_eval_seconds']
    requests_per_sec = config['asgard']['consensus_requests_per_second']
    cluster_nodes = config['asgard']['number_of_test_nodes']
    repeat = config['asgard']['consensus_request_rounds']
    hbi = config['asgard']['val_hbi']
    ww = config['asgard']['val_waiting_window']

    params_str = "Eval Parameters:\n"
    params_str += "    max_entries_per_pkt = {0}\n".format(max_entries_per_pkt)

    params_str += "    log_rep_eval_time = {0}\n".format(log_rep_eval_time)
    params_str += "    requests_per_sec = {0}\n".format(requests_per_sec)
    params_str += "    log_rep_eval_time = {0}\n".format(log_rep_eval_time)
    params_str += "    cluster_nodes = {0}\n".format(cluster_nodes)
    params_str += "    repeat = {0}\n".format(repeat)
    params_str += "    hbi = {0}\n".format(hbi)
    params_str += "    ww = {0}\n".format(ww)

    # multiple nodes per run
    results_str = "Throughput Restults from {0} logs\n".format(throughputs_mbs.size)
    results_str += "    {:8.5f} mb/sec Mean\n".format(throughputs_mbs.mean())
    results_str += "    {:8.5f} mb/sec Variance\n".format(throughputs_mbs.var())
    results_str += "    {:8.5f} mb/sec MAX\n".format(throughputs_mbs.max())
    results_str += "    {:8.5f} mb/sec MIN\n".format(throughputs_mbs.min())
    results_str += "    {0} Failures\n".format(int(np.add.reduce(failures)))


def vanillaAsgardEvalReport(config, log_dir):
    hosts = dict()

    hosts['le_delta_microsec'] = list()
    hosts['le_delta_cycles'] = list()

    for cluster_id in range(1, int(config['asgard']['number_of_test_nodes']) + 1):
        vanillaAsgardGetTimestampsForTarget(config, hosts, log_dir, cluster_id)
        vanillaAsgardGetThroughputTotalForTarget(hosts, log_dir, cluster_id)

    throughput_mbs= np.array([])
    failures = np.array([])
    np.set_printoptions(suppress=True)

    for cluster_id in range(1, int(config['asgard']['number_of_test_nodes']) + 1):
        vanillaAsgardCalculateThroughput(config,hosts, cluster_id, throughput_mbs, failures)

    vanillaAsgardPrintReport(config, throughput_mbs, failures)


if __name__ == '__main__':
   reslist = convertAsgardOrderedCSVtoListOfEvents("testdata")
   rawLatencies = getListOfRAWLatenciesFromListOfEvents(reslist, "ts1", "ts2")
   realLatencies = convertListOfRAWLatenciesToListOfMicrosecLatencies(rawLatencies, 2400)

   plotHistogram(realLatencies)