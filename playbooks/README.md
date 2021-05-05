# Playbooks

We have source code. We have Paper. But how can you reproduce the results?
Look no further! 


## Execute a Scenario
To execute anything on a server, we use bash scripts. 

Scripts are placed in ```00_local_execution/``` subfolder.

## Orchestrate a Scenario
To orchestrate anything on multiple servers, we use ansible scripts.

Ansible playbooks are placed in ```01_global_orchestrate/``` subfolder.


## Batch execute multiple Scenarios
To execute a set of scenarios for various setups (using lkm, dpdk or plain), we use Jenkins.

Jenkinsfiles are placed in ```10_global_batch/``` subfolder.
