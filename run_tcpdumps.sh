#!/bin/bash

if [ $# -lt 1 ]; then
	echo "USAGE: ./run_tcpdumps.sh <out-dir> [tcpdump args]" >&2
	exit 1
fi

out="$(realpath "$1")"
if [ ! -d "$out" ]; then
	echo "out-dir is not a directory: $out" >&2
	exit 1
fi

sudo ip netns exec Host-1 tcpdump --interface eth0 ${@:2} &>"$out"/Host-1_eth0 &
echo $! > ~/.tcpdump_pids
sudo ip netns exec Host-2 tcpdump --interface eth0 ${@:2} &>"$out"/Host-2_eth0 &
echo $! >> ~/.tcpdump_pids
sudo ip netns exec Host-3 tcpdump --interface eth0 ${@:2} &>"$out"/Host-3_eth0 &
echo $! >> ~/.tcpdump_pids
sudo ip netns exec Host-4 tcpdump --interface eth0 ${@:2} &>"$out"/Host-4_eth0 &
echo $! >> ~/.tcpdump_pids
sudo ip netns exec Router-1 tcpdump --interface eth0 ${@:2} &>"$out"/Router-1_eth0 &
echo $! >> ~/.tcpdump_pids
sudo ip netns exec Router-1 tcpdump --interface eth1 ${@:2} &>"$out"/Router-1_eth1 &
echo $! >> ~/.tcpdump_pids
sudo ip netns exec Router-1 tcpdump --interface eth2 ${@:2} &>"$out"/Router-1_eth2 &
echo $! >> ~/.tcpdump_pids
sudo ip netns exec Router-2 tcpdump --interface eth0 ${@:2} &>"$out"/Router-2_eth0 &
echo $! >> ~/.tcpdump_pids
sudo ip netns exec Router-2 tcpdump --interface eth1 ${@:2} &>"$out"/Router-2_eth1 &
echo $! >> ~/.tcpdump_pids
sudo ip netns exec Router-2 tcpdump --interface eth2 ${@:2} &>"$out"/Router-2_eth2 &
echo $! >> ~/.tcpdump_pids
sudo ip netns exec Router-3 tcpdump --interface eth0 ${@:2} &>"$out"/Router-3_eth0 &
echo $! >> ~/.tcpdump_pids
sudo ip netns exec Router-3 tcpdump --interface eth1 ${@:2} &>"$out"/Router-3_eth1 &
echo $! >> ~/.tcpdump_pids
