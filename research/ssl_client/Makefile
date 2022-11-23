PROXY_PATH := $(shell cd ../../; pwd)

include $(PROXY_PATH)/build/Makefile.path
include $(PROXY_PATH)/build/Makefile.common
#
# Target
#
MODULE = ssl_client
BINARY_TYPE = bin

#
# Sources
#
ADDITIONAL_CPP_SOURCES += ssl_client.cpp


CFLAGS +=	-I./ \
			-I../../src/core \
			-I../../src/kernel \
			-I../../src/app \
			-I../../src/protocol \
			-I../../3rdparty/st-srs \
			-I../../3rdparty/openssl/include \
			-I../../3rdparty/c-ares
	
CFLAGS += -std=c++11
CFLAGS += -fPIC

STATIC_LIBRARY += $(PROXY_PATH)/output/libapp.a
STATIC_LIBRARY += $(PROXY_PATH)/output/libprotocol.a
STATIC_LIBRARY += $(PROXY_PATH)/output/libkernel.a
STATIC_LIBRARY += $(PROXY_PATH)/output/libcore.a


STATIC_LIBRARY += $(PROXY_PATH)/output/openssl/lib/libssl.a \
				  $(PROXY_PATH)/output/openssl/lib/libcrypto.a
STATIC_LIBRARY += $(PROXY_PATH)/output/libst.a
STATIC_LIBRARY += $(PROXY_PATH)/output/c-ares/lib/libcares.a
STATIC_LIBRARY += -pthread -ldl

#
include $(PROXY_PATH)/build/Makefile.project
