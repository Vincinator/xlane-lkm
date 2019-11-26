******************
Installing Asguard
******************


Requirements
************
Before you start,
please make sure you have completed the following homeworks first:

- SSH access to the server
- Your server has a supported NIC (currently only Cards using the mlx5 driver)
- Your server runs a supported Linux Distribution (deb based distros)
- (Optional) management interface if SSH is not running/working


Install the ASGuard Kernel
**************************

Get the latest ASGuard Kernel installation files for your distro.
You can also build your own installation files from the asguard-kernel repo.

TODO: reference to the building asguard guide

We have a automated build system in place,
that updates the Kernel for the target machines for you.


Install the ASGuard Module
**************************
The ASGuard Kernel module requires to be build
with the sources of the installed kernel version.

If you have NOT build the ASGModule with the correct ASGKernel,
then you will get a version error when you try to load the ASGModule.









