PROJ_PATH=$(shell pwd)

all:app core kernel protocol main
app:
	$(MAKE) -C $(PROJ_PATH)/src/app
core:
	$(MAKE) -C $(PROJ_PATH)/src/core
kernel:
	$(MAKE) -C $(PROJ_PATH)/src/kernel
protocol:
	$(MAKE) -C $(PROJ_PATH)/src/protocol
main:
	$(MAKE) -C $(PROJ_PATH)/src/main
clean:
	rm -rf $(PROJ_PATH)/output/myproxy
full_clean:
	rm -rf $(PROJ_PATH)/output
	rm -rf $(PROJ_PATH)/tmpobj