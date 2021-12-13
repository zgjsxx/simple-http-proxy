all:proxy simpleproxy simpleclient
proxy:
	g++ src/myproxy.cpp src/ServerLog.cpp -o myproxy \
	-I./include  \
	-I./import/openssl/include/ \
	-L./import/statethread \
	-L./import/openssl/lib \
	-lst -llog4cxx -lssl -lcrypto \
	-Wl,-rpath=/home/work/simple_proxy_st/import/statethread \
	-Wl,-rpath=/home/work/simple_proxy_st/import/openssl/lib -std=c++11

simpleproxy:
	g++ SimpleTLSServer.cpp  -o SimpleTLSServer \
	-I./include  \
	-I./import/openssl/include/ \
	-L./import/openssl/lib \
	-llog4cxx -lssl -lcrypto \
	-Wl,-rpath=/home/work/simple_proxy_st/import/openssl/lib -std=c++11
simpleclient:
	g++ SimpleTLSClient.cpp  -o SimpleTLSClient \
	-I./include  \
	-I./import/openssl/include/ \
	-L./import/openssl/lib \
	-llog4cxx -lssl -lcrypto \
	-Wl,-rpath=/home/work/simple_proxy_st/import/openssl/lib -std=c++11
clean:
	rm -rf myproxy
	rm -rf SimpleTLSServer
	rm -rf SimpleTLSClient