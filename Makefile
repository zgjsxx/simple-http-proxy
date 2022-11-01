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
main:app core kernel protocol
	$(MAKE) -C $(PROJ_PATH)/src/main
clean:
	rm -rf $(PROJ_PATH)/output/myproxy
	rm -rf $(PROJ_PATH)/output/libapp.a
	rm -rf $(PROJ_PATH)/output/libcore.a
	rm -rf $(PROJ_PATH)/output/libkernel.a
	rm -rf $(PROJ_PATH)/output/protocol.a
full_clean:
	rm -rf $(PROJ_PATH)/output
	rm -rf $(PROJ_PATH)/tmpobj