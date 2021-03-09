#!/usr/bin/env bash

read -p "WARNING: use virtual environment dedicated for asgard evaliation. Continue? [y/N]" -n 1 -r
echo    # (optional) move to a new line
if [[ $REPLY =~ ^[Yy]$ ]]
then
  echo "uninstalling asgard-cli site-packages"
  pip freeze | xargs pip uninstall -y
  echo #new line
  echo "installing asgard-cli"
  python3 -m pip install --no-cache-dir -I asgardcli-0.1.0-py3-none-any.whl   # todo: pass name of whl
fi


