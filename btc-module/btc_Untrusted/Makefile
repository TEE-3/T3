CC = g++
LIBS = -ljsoncpp -lcurl -ljsonrpccpp-common -ljsonrpccpp-client -lcrypto -pthread -lrt

all: Main

ifeq ($(ORAM_TESTING), 1)
				echo "ORAM TESTING INITIATED"
        FLAGS += -DORAM_TESTING
else ifeq ($(ZT_TESTS), 1)
        FLAGS += -DZT_TESTS
endif

Main: Main.cpp bitcoinapi/bitcoinapi.cpp
	$(CC) -std=c++11 $(FLAGS) $^ -L=$(CURDIR) -lZT $(LIBS) -Wl,--rpath=$(CURDIR) -o $@

clean:
	rm -rf Main enclave.signed.so libZT.so *.bin