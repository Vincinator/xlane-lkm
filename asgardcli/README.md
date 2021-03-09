# Asgard CLI

Controls the asgard kernel module. 


## Installation

```
# create a  separate venv for building
python3 -m venv asgard-cli-venv

# activate venv
source asgard-cli-venv/bin/activate # mind your shell flavor (bash, fish, zsh)

# install latest PyPA's build
python3 -m pip install --upgrade build 

# build asgard-cli 
python3 -m build
```

If installation went smoothly, you will have asgard_cli packages available under ```dist``` folder.

