PROJ_PATH=$(shell cd ../../;pwd)
CXX=g++
include_dir +=$(PROJ_PATH)/src/libproxy/include \
				-I$(PROJ_PATH)/src/import/openssl/include

lib_dir=$(PROJ_PATH)/output/debug/bin/lib \
		-L$(PROJ_PATH)/src/import/openssl/lib \
		-L$(PROJ_PATH)/src/import/statethread

all:libproxy proxy
libproxy:
	$(MAKE) -C $(PROJ_PATH)/src/libproxy
proxy:
	g++ $(PROJ_PATH)/src/myproxy.cpp -o myproxy -g \
	-I$(include_dir) -L$(lib_dir) \
	-lst -llog4cxx -lssl -lcrypto -lproxy \
	-Wl,-rpath=$(PROJ_PATH)/src/import/statethread \
	-Wl,-rpath=$(PROJ_PATH)/src/import/openssl/lib -std=c++11 \
	-Wl,-rpath=$(PROJ_PATH)/output/debug/bin/lib 

clean:
	rm -rf $(PROJ_PATH)/output
