#!/usr/bin/env bash

set -e

read -p "WARNING: This script uses a virtual environment dedicated for asgard evaluation (../eval-venv). Continue? [y/N]" -n 1 -r
echo    # (optional) move to a new line
if [[ $REPLY =~ ^[Yy]$ ]]
then
  INVENV=$(python3 -c 'import sys; print ("1" if hasattr(sys, "real_prefix") else "0")')
  echo $INVENV
  # check if we run in venv
  if [[ $INVENV == 0 ]]
  then
    echo "activating "
    # Check if venv exists
    if [ ! -d "../eval-venv" ]; then
      python3 -m venv ../eval-venv
    fi

    #activate eval venv
    source ../eval-venv/bin/activate
  fi


  echo "uninstalling asgard-cli site-packages"
  pip install --upgrade pip
  #pip freeze | xargs pip uninstall -y
  pip uninstall asgardcli
  echo #new line
  echo "installing asgard-cli"
  python3 -m pip install --no-cache-dir -I asgardcli-0.1.0-py3-none-any.whl   # todo: pass name of whl

  echo "Creating benchmark Configuration. YOu can always redo this step via 'acli generate-bench-config'"
  acli generate-bench-config

fi


