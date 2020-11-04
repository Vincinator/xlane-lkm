### Build Asgard Kernel Module
You need the Asgard Kernel Sources to build the Asgard Kernel Module.

#### Requirements

- Clone the asgard linux kernel to your workstation
- Build the asgard linux kernel with the `CONFIG_ASGARD_DRV=y` set in the .config
    - can be configured via menuconfig or other make generation

#### Out-of-tree build
Clone the asgard-lkm and asgard-kernel to your local worspace directory.


To build kernel modules against the latest asgard-kernel, you need to provide the path to the asgard-kernel source,
and also build the asgard-lernel once, so that the vmlinux exists in the asgard-kernel source tree.


```
ASGARD_KERNEL_SRC=../asgard-kernel/ make
```


#### In-tree build
Place the asgard-lkm sources into net/asgard of your local asgard kernel sources.
Make sure you do not commit asgard-lkm code into the asgard kernel when doing this.


Make a kernel config (configure it acording to your requirements), and add the following lines to the ```.config```:

 ```
CONFIG_ASGARD_DRV=y
CONFIG_ASGARD_MODULE=m
CONFIG_ASGARD=y
 ```

To Build the asgard kernel Module run:
```  
make net/asgard/
``` 








