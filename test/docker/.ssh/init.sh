#!/usr/bin/env sh

# ---------
# Enter Password when prompted to copy sshkey to test machines
# ---------

ssh-copy-id cerebro@asgard01
ssh-copy-id dsp@asgard02
ssh-copy-id dsp@asgard03
ssh-copy-id dsp@asgard04
