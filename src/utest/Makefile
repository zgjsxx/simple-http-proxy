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
ADDITIONAL_CPP_SOURCES += srs_utest_main.cpp \
						  srs_utest_core.cpp


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


LDFLAGS +=  -L$(PROXY_PATH)/3rdparty/statethread \
			-L$(PROXY_PATH)/3rdparty/openssl/lib \
			-lst -pthread -ldl\
			-Wl,-rpath=$(PROXY_PATH)/3rdparty/st-srs \
			-Wl,-rpath=$(PROXY_PATH)/3rdparty/openssl/lib	
#
include $(PROXY_PATH)/build/Makefile.project