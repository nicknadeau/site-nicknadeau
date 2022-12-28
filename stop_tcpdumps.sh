#!/bin/bash

if [ -f ~/.tcpdump_pids ]; then
	pids="$(cat ~/.tcpdump_pids)"
	for pid in $pids; do
		sudo kill -SIGINT $pid
	done
	rm ~/.tcpdump_pids
fi
