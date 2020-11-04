#!/bin/bash


# Little helper to rename the project name


#find . -mindepth 1 -maxdepth 100 -type d | xargs rename 's/asguard/asgard/' "$1/*"
find . -name "*" -print0 | xargs -0 rename "s/asguard/asgard/" 