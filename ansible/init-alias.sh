#!/usr/bin/env bash

alias reb="ansible-playbook playbooks/rebuild-linux.yml"
alias lprov="ansible-playbook -i libasraft.local.inventory.ini playbooks/provision-tnodes.yml"
alias rprov="ansible-playbook -i libasraft.testlab.inventory.ini playbooks/provision-tnodes.yml"