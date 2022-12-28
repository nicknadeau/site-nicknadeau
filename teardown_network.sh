#!/bin/bash

namespaces='Host-1 Host-2 Host-3 Host-4 Router-1 Router-2 Router-3'

echo 'Destroying namespaces...'
for namespace in $namespaces; do
	sudo ip netns del "$namespace"
done
