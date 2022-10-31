PROJ_PATH=$(shell pwd)
CXX=g++
include_dir +=$(PROJ_PATH)/src/libproxy/include \
				-I$(PROJ_PATH)/src/import/openssl/include

lib_dir=$(PROJ_PATH)/output/debug/bin/lib \
		-L$(PROJ_PATH)/src/import/openssl/lib \
		-L$(PROJ_PATH)/src/import/statethread

all:proxy main
proxy:
	$(MAKE) -C $(PROJ_PATH)/src/proxy
base:
	$(MAKE) -C $(PROJ_PATH)/src/base
main:
	$(MAKE) -C $(PROJ_PATH)/src/main

clean:
	rm -rf $(PROJ_PATH)/output

full_clean:
	rm -rf $(PROJ_PATH)/output
	rm -rf $(PROJ_PATH)/tmpobj