PROXY_PATH := $(shell cd ../../; pwd)

include $(PROXY_PATH)/build/Makefile.path
include $(PROXY_PATH)/build/Makefile.common
#
# Target
#
MODULE = myproxy_utest
BINARY_TYPE = bin

#
# Sources
#

ADDITIONAL_SOURCE_COMPILE_PATH = ./

CFLAGS +=	-I./ \
			-I../core \
			-I../kernel \
			-I../app \
			-I../protocol \
			-I../../3rdparty/st-srs \
			-I../proxy/include \
			-I../../3rdparty/openssl/include \
			-I../../3rdparty/gtest-fit/googletest/include
	
CFLAGS += -std=c++11
CFLAGS += -fPIC

STATIC_LIBRARY += $(PROXY_PATH)/output/libapp.a
STATIC_LIBRARY += $(PROXY_PATH)/output/libprotocol.a
STATIC_LIBRARY += $(PROXY_PATH)/output/libkernel.a
STATIC_LIBRARY += $(PROXY_PATH)/output/libcore.a

# gtest
STATIC_LIBRARY += $(PROXY_PATH)/output/gtest_main.a
STATIC_LIBRARY += $(PROXY_PATH)/output/gtest.a

STATIC_LIBRARY += $(PROXY_PATH)/output/openssl/lib/libssl.a \
				  $(PROXY_PATH)/output/openssl/lib/libcrypto.a
STATIC_LIBRARY += $(PROXY_PATH)/output/libst.a
STATIC_LIBRARY += $(PROXY_PATH)/output/c-ares/lib/libcares.a
STATIC_LIBRARY += -pthread -ldl


#
include $(PROXY_PATH)/build/Makefile.project
