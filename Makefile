all : linuxtest

CFLAGS:=-Os -flto

SIZE_PREFIX:=riscv64-linux-gnu

ETHERNET_DEV:=$(shell ip addr | grep ": e" | grep mtu | cut -f 2 -d' ' | cut -f 1 -d':')
ETHERNET_ADDR:=$(shell ip -4 -o addr show dev $(ETHERNET_DEV) | tr -s ' ' | cut -f4 -d' ')
ETHERNET_ROUTE_DEFAULT:=$(shell ip route | grep enx00e04c681031 | grep default | cut -d' ' -f3)
ETHERNET_ROUTE_BASE:=$(shell ip route | grep enx00e04c681031 | grep -v default | cut -d' ' -f1)

size : linuxtest.c
	$(SIZE_PREFIX)-gcc -Os -g -o linuxtest.rv $^ -flto
	$(SIZE_PREFIX)-objdump -t linuxtest.rv | grep hip | grep -v sfhip_send_packet | tr -s ' ' | cut -f 4 -d' ' | cut -f 2 | sed -e 's/^0*//' | tr '[:lower:]' '[:upper:]' | sed -e 's/^/+/' | xargs echo "obase=10;ibase=16;0"  | bc | xargs echo "Total Size Bytes For hip Functions:"
	$(SIZE_PREFIX)-objdump -S linuxtest.rv > rv.lst

linuxtest : linuxtest.c
	gcc $(CFLAGS) -g -o $@ $^
	objdump -t $@ | grep hip | grep -v sfhip_send_packet | tr -s ' ' | cut -f 4 -d' ' | cut -f 2 | sed -e 's/^0*//' | tr '[:lower:]' '[:upper:]' | sed -e 's/^/+/' | xargs echo "obase=10;ibase=16;0"  | bc | xargs echo "Total Size Bytes For hip Functions:"

test :
	echo $(ETHERNET_ADDR)

testprom : linuxtest
	sudo ./linuxtest tap1 -

testeth : linuxtest
	sudo ./linuxtest - $(ETHERNET_DEV)

setupforprommerge : linuxtest
	sudo ip tuntap add dev "tap1" mode "tap" user $(shell whoami) || true
	sudo ip link add name br0 type bridge || true
	sudo ip link set dev br0 up || true
	sudo ip link set $(ETHERNET_DEV) master br0 || true
	sudo ip link set tap1 master br0 || true
	sudo ip addr del $(ETHERNET_ADDR) dev $(ETHERNET_DEV) || true
	sudo ip addr change $(ETHERNET_ADDR) dev br0 || true
	sudo ip route add $(ETHERNET_ROUTE_BASE) dev br0 || true
	sudo ip route add default via $(ETHERNET_ROUTE_DEFAULT) dev br0

testlocal : linuxtest
	./linuxtest tap1 - 5 &
	ping 192.168.14.251 -c 2

# alternatively you just put your ethernet device after the -
# if you want to allow it out on your network.

clean :
	rm -rf linuxtest

format:
	clang-format --style=file -i *.[ch]

