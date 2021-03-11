#!/usr/bin/env bash

read -p "WARNING: This scipr uses a virtual environment dedicated for asgard evaliation (../eval-venv). Continue? [y/N]" -n 1 -r
echo    # (optional) move to a new line
if [[ $REPLY =~ ^[Yy]$ ]]
then

  # check if we run in venv
  if [[ "$VIRTUAL_ENV" 0= "" ]]
  then
    # Check if venv exists
    if [ ! -d "../eval-venv" ]; then
      python3 -m venv ../eval-venv
    fi

    #activate eval venv
    . asgardcli/asgard-cli-venv/bin/activate
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


