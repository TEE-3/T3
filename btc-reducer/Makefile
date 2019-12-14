CC = g++
LIBS= -ljsoncpp -lcurl -ljsonrpccpp-common -ljsonrpccpp-client -lcrypto
btc_reducer:
	$(CC) -o btc_reducer btc_reducer.cpp bitcoinapi/bitcoinapi.cpp $(LIBS)
clean: 
	rm -rf btc_reducer *.bin
