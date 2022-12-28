#!/bin/bash

hosts='Host-1 Host-2 Host-3 Host-4'
routers='Router-1 Router-2 Router-3'
namespaces="$hosts $routers"

echo 'Creating namespaces...'
for namespace in $namespaces; do
	sudo ip netns add "$namespace"
done

echo 'Creating bridges...'
for router in $routers; do
	sudo ip netns exec "$router" ip link add vswitch type bridge
done

echo 'Creating virtual ethernet cables...'
sudo ip link add eth0 netns Host-1 type veth peer eth0 netns Router-1
sudo ip link add eth0 netns Host-2 type veth peer eth1 netns Router-1
sudo ip link add eth0 netns Host-3 type veth peer eth1 netns Router-2
sudo ip link add eth0 netns Host-4 type veth peer eth1 netns Router-3
sudo ip link add eth2 netns Router-1 type veth peer eth0 netns Router-2
sudo ip link add eth2 netns Router-2 type veth peer eth0 netns Router-3

echo 'Connecting virtual cables to bridges...'
sudo ip netns exec Router-1 ip link set eth0 master vswitch
sudo ip netns exec Router-1 ip link set eth1 master vswitch
sudo ip netns exec Router-1 ip link set eth2 master vswitch
sudo ip netns exec Router-2 ip link set eth0 master vswitch
sudo ip netns exec Router-2 ip link set eth1 master vswitch
sudo ip netns exec Router-2 ip link set eth2 master vswitch
sudo ip netns exec Router-3 ip link set eth0 master vswitch
sudo ip netns exec Router-3 ip link set eth1 master vswitch

echo 'Assigning host ip addresses...'
sudo ip netns exec Host-1 ip addr add 10.0.1.2/24 dev eth0
sudo ip netns exec Host-2 ip addr add 10.0.1.3/24 dev eth0
sudo ip netns exec Host-3 ip addr add 10.0.2.2/24 dev eth0
sudo ip netns exec Host-4 ip addr add 10.0.3.2/24 dev eth0

echo 'Setting all devices to promiscuous mode...'
for host in $hosts; do
	sudo ip netns exec "$host" ip link set eth0 promisc on
done
sudo ip netns exec Router-1 ip link set eth0 promisc on
sudo ip netns exec Router-1 ip link set eth1 promisc on
sudo ip netns exec Router-1 ip link set eth2 promisc on
sudo ip netns exec Router-2 ip link set eth0 promisc on
sudo ip netns exec Router-2 ip link set eth1 promisc on
sudo ip netns exec Router-2 ip link set eth2 promisc on
sudo ip netns exec Router-3 ip link set eth0 promisc on
sudo ip netns exec Router-3 ip link set eth1 promisc on

echo 'Turning all devices up...'
for host in $hosts; do
	sudo ip netns exec "$host" ip link set dev eth0 up
done
sudo ip netns exec Router-1 ip link set dev eth0 up
sudo ip netns exec Router-1 ip link set dev eth1 up
sudo ip netns exec Router-1 ip link set dev eth2 up
sudo ip netns exec Router-2 ip link set dev eth0 up
sudo ip netns exec Router-2 ip link set dev eth1 up
sudo ip netns exec Router-2 ip link set dev eth2 up
sudo ip netns exec Router-3 ip link set dev eth0 up
sudo ip netns exec Router-3 ip link set dev eth1 up
for router in $routers; do
	sudo ip netns exec "$router" ip link set dev vswitch up
done

echo 'Configuring routing rules...'
sudo ip netns exec Host-1 ip route add 10.0.2/24 via 10.0.1.2 dev eth0
sudo ip netns exec Host-1 ip route add 10.0.3/24 via 10.0.1.2 dev eth0
sudo ip netns exec Host-2 ip route add 10.0.2/24 via 10.0.1.3 dev eth0
sudo ip netns exec Host-2 ip route add 10.0.3/24 via 10.0.1.3 dev eth0
sudo ip netns exec Host-3 ip route add 10.0.1/24 via 10.0.2.2 dev eth0
sudo ip netns exec Host-3 ip route add 10.0.3/24 via 10.0.2.2 dev eth0
sudo ip netns exec Host-4 ip route add 10.0.1/24 via 10.0.3.2 dev eth0
sudo ip netns exec Host-4 ip route add 10.0.2/24 via 10.0.3.2 dev eth0
