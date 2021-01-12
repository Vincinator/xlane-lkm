#!/usr/bin/env bash

green=`tput setaf 2`
reset=`tput sgr0`

USER_MAIL="v.riesop@sap.com"
SSHKEY_FOLDER="docker/.ssh"
SSHKEY_NAME="id_ed25519"
SSHKEY_PATH="$SSHKEY_FOLDER/$SSHKEY_NAME"

echo " ${green} Generating new sshkey for testnodes ... ${reset}"

mkdir -p $SSHKEY_FOLDER
ssh-keygen -t ed25519 -C $USER_MAIL -f $SSHKEY_FOLDER/$SSHKEY_NAME -q -N ""

cat $SSHKEY_PATH.pub >> $SSHKEY_FOLDER/authorized_keys


echo "${green} done! ${reset}"