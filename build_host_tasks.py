#!/usr/bin/env python3

import paramiko

from logger import *



class build_host_task(object):

    def __init__(self, target_ip, target_user, priv_key_path, bh_task):
        self.user = target_user
        self.target_ip = target_ip
        self.task = bh_task
        self.ssh = paramiko.SSHClient()
        self.priv_key_path = priv_key_path
        self.ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())


    def test_connection(self):
        print_info("testing connection ..")
        self.ssh.connect(self.target_ip, username=self.user, key_filename=self.priv_key_path)
        print_info("done testing connection ..")
        self.ssh.close()

    def run(self):
        output=""
        self.ssh.connect(self.target_ip, username=self.user, key_filename=self.priv_key_path)
        tran = self.ssh.get_transport()
        chan = tran.open_session()
        # chan.set_combine_stderr(True)
        chan.get_pty()
        f = chan.makefile()
        chan.exec_command(self.task)

       # stdin, stdout, stderr = chan.exec_command(self.task)

        print_info(self.user + "@" + self.target_ip + ": \n  > " + self.task)

        for line in f.readlines():
            output= output + "  < " + line
        if output!="":
            print_success(output)
        else:
            print("There was no output for this command")

        self.ssh.close()

