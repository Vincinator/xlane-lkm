# Asgard

Asgard introduces a sliced network for distributed applications on kernel level.
Failure detection, cluster management and log replication are implemented as kernel level services with interfaces to user space.

This repository contains source code for three asgard flavours:
- lkm (asgard implemented as loadable linux kernel module)
- dpdk (asgard implemented using dpdk)
- plain (a pure use space implementation of asgard)



# Compile 


## LKM

```bash
ASGARD_KERNEL_SRC=/path/to/asgard/kernel/src
./build.sh --lkm --kerneldir $ASGARD_KERNEL_SRC
```
### Flags
If ```EVAL_ONLY_STORE_TIMESTAMPS``` is set timestamps are only stored in timestamp buffer.
Otherwise, timestamps are used to calculate metric in-kernel without the need of a large timestamp buffer.


# Setup Overview

Server setup is done via Jenkins. You can checkout the Jenkinsfile definition here:
https://github.wdf.sap.corp/cerebro/lab/blob/master/deployment/asgard-module/Jenkinsfile

This Jenkinsfile just calls ansible files, one ansible file per target with targets beeing:
- lkm (asgard implemented as loadable linux kernel module)
- dpdk (asgard implemented using dpdk)
- plain (a pure use space implementation of asgard)

You could also call the ansible files individual without jenkins (or build a top level ansible script yourself).

The target servers are defined in the inventory file, which you can checkout here:
https://github.wdf.sap.corp/cerebro/lab/blob/master/deployment/asgard.testlab.small.inventory.ini

Create or adapt the inventory.ini to your needs, and dont forget to reference the correct inventory when invoking the ansible playbooks.


Once you have deployed the latest asgard software pieces, it is time to install them.








# Example Use Cases


# Development Snippets

```
curl -X POST http://cerebro:ACCESS_TOKEN@JENKINS_IP:JENKINS_PORT/job/Vincent/job/Build%20and%20Publish%20-%20ASGARD/job/master/build
```

# Licence

All Asgard code contributions, including kernel module, custom kernel and asgardcli is published under the GNU GPLv2.

## Contributors
Riesop, Vincent <Vincent.Riesop@gmail.com>
Jahnke, Patrick <jahnke@dsp.tu-darmstadt.de>
