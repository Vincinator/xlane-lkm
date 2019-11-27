****************************
ASGuard development Workflow
****************************

To better understand our development workflow,
we start by describing our development environment and it's challanges.

You need a supported NIC in order to run and test ASGuard.
Your local workstation does (most likeley) not have one of the
supported NICs. If you have one, awesome - I'm jealous!

We test asguard on a dedicated test lab. Therefore, we need to upload
a every version we want to test to the lab and deploy it there.
Additionally, we compile ASGuard on Linux machines, and utilise a build
server within our test lab.



GIT Workflow
************

We push any changes first to the dev branch of the ASGuard Module Repository.
We test the changes (KUnit, performance tests) and if the tests pass,
then we merge back into a stable branch (manually).

The stable branch commits should contain a descriptive commit message. However,
the commit messages in the dev branch should at least reference a GitHub issue.





