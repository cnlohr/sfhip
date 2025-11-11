
ETHERNET_DEV:=$(shell ip addr | grep ": e" | grep mtu | cut -f 2 -d' ' | cut -f 1 -d':')
ETHERNET_ADDR:=$(shell ip -4 -o addr show dev $(ETHERNET_DEV) | tr -s ' ' | cut -f4 -d' ')
ETHERNET_ROUTE_DEFAULT:=$(shell ip route | grep enx00e04c681031 | grep default | cut -d' ' -f3)
ETHERNET_ROUTE_BASE:=$(shell ip route | grep enx00e04c681031 | grep -v default | cut -d' ' -f1)

setupforprommerge :
	sudo ip tuntap add dev "tap1" mode "tap" user $(shell whoami) || true
	sudo ip link add name br0 type bridge || true
	sudo ip link set dev br0 up || true
	sudo ip link set $(ETHERNET_DEV) master br0 || true
	sudo ip link set tap1 master br0 || true
	sudo ip addr del $(ETHERNET_ADDR) dev $(ETHERNET_DEV) || true
	sudo ip addr change $(ETHERNET_ADDR) dev br0 || true
	sudo ip route add $(ETHERNET_ROUTE_BASE) dev br0 || true
	sudo ip route add default via $(ETHERNET_ROUTE_DEFAULT) dev br0

format:
	find . -iname '*.h' -o -iname '*.c' | xargs clang-format --style=file -i

