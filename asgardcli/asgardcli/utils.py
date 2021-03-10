import paramiko
import time

import os
import configparser
import os.path


def load_config(config_path):
    config = None
    if os.path.exists(config_path):
        config = configparser.ConfigParser()
        config.read(config_path)
    return config


def prepareHost(ip, cmd, flags):
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(ip, username='dsp', password="")
    try:
        stdin, stdout, stderr = ssh.exec_command(
            f"cd /home/dsp/asgard-bench/ && python3.7 asgardBench.py {cmd} {flags}")
        # print(stdout.read())
    except paramiko.ssh_exception.SSHException as e:
        print(f"Failed to Execute CMD {e}")
        return
    ssh.close()
    time.sleep(1)