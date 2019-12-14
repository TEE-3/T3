all: Main

Main: Main.cpp
	g++ -std=c++11 $^ -L=$(CURDIR) -lZT -lcrypto -Wl,--rpath=$(CURDIR) -o $@

clean:
	rm -rf Main enclave.signed.so libZT.so
