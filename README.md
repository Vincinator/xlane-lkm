# Asgard

Asgard introduces a sliced network for distributed applications on kernel level.
Failure detection, cluster management and log replication are implemented as a kernel level services
with interfaces to user space.

This repository contains source code for three asgard flavours:
- lkm (asgard implemented as loadable linux kernel module)
- dpdk (asgard implemented using dpdk)
- plain (a pure use space implementation of asgard)



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


### Installation (LKM)
Installation is done via the acli (asgard command line interface). To use acli, you need to install it.

Once installed, you can continue with the configuration chapter.

### Configuration (LKM)

Configuration is done via an node.ini file.
If you are using LKM version, you could use acli to help you generate a fresh node.ini.
```acli``` will also be used to parse this node.ini and call the lkm interfaces (procfs) to configure the lkm version.

```bash
# also creates a venv
./acli-installer.sh 

# activate the venv 
source ../eval-venv 

# prepare asgard 
acli generate-bench-config

# unload asgard
acli unload-kernel-module
```


# Example Use Cases



# Licence

All Asgard code contributions, including kernel module, custom kernel and asgardcli is published under the GNU GPLv3.

## Contributors
Riesop, Vincent <Vincent.Riesop@gmail.com>
Jahnke, Patrick <jahnke@dsp.tu-darmstadt.de>
