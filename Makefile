PROJ_PATH=$(shell pwd)

all:base proxy main
proxy:
	$(MAKE) -C $(PROJ_PATH)/src/proxy
base:
	$(MAKE) -C $(PROJ_PATH)/src/base
main:
	$(MAKE) -C $(PROJ_PATH)/src/main

clean:
	rm -rf $(PROJ_PATH)/output/myproxy

full_clean:
	rm -rf $(PROJ_PATH)/output
	rm -rf $(PROJ_PATH)/tmpobj