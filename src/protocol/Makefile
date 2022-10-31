PROXY_PATH := $(shell cd ../../; pwd)

include $(PROXY_PATH)/build/Makefile.path
include $(PROXY_PATH)/build/Makefile.common
#
# Target
#
MODULE = libprotocol.a
BINARY_TYPE = a

#
# Sources
#

SOURCE_PATH +=	./

STATIC_LIBRARY += $(PROXY_PATH)/output/libcore.a
STATIC_LIBRARY += $(PROXY_PATH)/output/libprotocol.a
STATIC_LIBRARY += $(PROXY_PATH)/output/libkernel.a
STATIC_LIBRARY += $(PROXY_PATH)/output/libapp.a

CFLAGS +=	-I./ \
			-I../core \
			-I../kernel \
			-I../../3rdparty/st \
			-std=c++11

#
include $(PROXY_PATH)/build/Makefile.project