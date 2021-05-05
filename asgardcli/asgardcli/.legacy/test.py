import time

import paramiko

from asgardcli.utils import prepareHost

prepareHost('10.68.235.140', 'c')
prepareHost('10.68.235.142', 'c')
prepareHost('10.68.235.150', 'c')

prepareHost('10.68.235.140', 'p')
prepareHost('10.68.235.142', 'p')
prepareHost('10.68.235.150', 'p')

prepareHost('10.68.235.140', 'm')
prepareHost('10.68.235.142', 'm')
prepareHost('10.68.235.150', 'm')

prepareHost('10.68.235.140', 'l')
prepareHost('10.68.235.142', 'l')
prepareHost('10.68.235.150', 'l')


prepareHost('10.68.235.140', 't')
prepareHost('10.68.235.142', 't')
prepareHost('10.68.235.150', 't')


exit(0)

test = "2, 68036"

fest = (int(test[3:])-1000)/2400

print ((int(test[3:])-1000)/2400)

exit(0)

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('10.68.235.142', username='dsp', password='dsp1234')
stdin, stdout, stderr = ssh.exec_command('cd /home/dsp/asgard-bench/ && python3.7 asg-cli.py evalasgard -cpl')
time.sleep(4)
print (stdout.read())

chan = ssh.get_transport().open_session()
try:
    print("ssh")
    chan.exec_command('cd /home/dsp/asgard-bench/ && pyasg evalasgard -cpl')
    time.sleep(4)
    print()
except paramiko.ssh_exception.SSHException as e:
    print(f"Failed to Execute CMD {e.cmd}")
    print(f"Error Message {e.errors}")
print("fertig")
