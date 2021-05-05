#!/usr/bin/env bash

# Caller must specify target via -t flag
# -t|--target
#     Specifies which xlane flavour should be used for this call
#     Available targets are: lkm, plain, dpdk
# --clusterstring
#     Discovery Service is not implemented. (de-prioritized)
#     Current workaround for using a discovery service is providing the information of all possible clusters manually
#     Syntax: (IP_ADDRESS,MAC_ADDRESS,PROTOCOL_NUMBER,CLUSTER_ID),(IP_ADDRESS,MAC_ADDRESS,PROTOCOL_NUMBER,CLUSTER_ID), ...

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -t|--target) target="$2"; shift ;;
        --clusterstring) clusterstring="$2"; shift ;;
        *) echo "Unknown parameter: $1"; exit 1 ;;
    esac
    shift
done

case target in
  lkm)
    echo "NOT IMPLEMENTED."
    ;;
  dpdk)
    # https://www.geeksforgeeks.org/named-pipe-fifo-example-c-program/
    echo "NOT IMPLEMENTED."
    ;;
  plain)
    # https://www.geeksforgeeks.org/named-pipe-fifo-example-c-program/
    echo "NOT IMPLEMENTED."
    ;;
  *)
    echo "Invalid Target. Valid Targets are: dpdk, plain, lkm"
    ;;
esac

