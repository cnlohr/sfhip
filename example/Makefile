all : example

CFLAGS:=-Os -flto -I..

CFLAGS_SIZE:=-Os -g  -flto -I..
SIZE_PREFIX:=riscv64-linux-gnu

size : example.c
	$(SIZE_PREFIX)-gcc $(CFLAGS_SIZE) -o example.rv $^
	$(SIZE_PREFIX)-objdump -t example.rv | grep hip | grep -v sfhip_send_packet | tr -s ' ' | cut -f 4 -d' ' | cut -f 2 | sed -e 's/^0*//' | tr '[:lower:]' '[:upper:]' | sed -e 's/^/+/' | xargs echo "obase=10;ibase=16;0"  | bc | xargs echo "Total Size Bytes For hip Functions:"
	$(SIZE_PREFIX)-objdump -S example.rv > rv.lst

example : example.c
	gcc $(CFLAGS) -g -o $@ $^
	objdump -t $@ | grep hip | grep -v sfhip_send_packet | tr -s ' ' | cut -f 4 -d' ' | cut -f 2 | sed -e 's/^0*//' | tr '[:lower:]' '[:upper:]' | sed -e 's/^/+/' | xargs echo "obase=10;ibase=16;0"  | bc | xargs echo "Total Size Bytes For hip Functions:"


testprom : example
	sudo ./example tap1 -

testeth : example
	sudo ./example - $(ETHERNET_DEV)

testlocal : example
	./example tap1 - 5 &
	ping 192.168.14.251 -c 2

# alternatively you just put your ethernet device after the -
# if you want to allow it out on your network.

clean :
	rm -rf example


