PROXY_PATH := $(shell cd ../../; pwd)

include $(PROXY_PATH)/build/Makefile.path
include $(PROXY_PATH)/build/Makefile.common
#
# Target
#
MODULE = myproxy
BINARY_TYPE = bin

#
# Sources
#
ADDITIONAL_CPP_SOURCES += sp_main_server.cpp
ADDITIONAL_SOURCE_PATH += ./src


CFLAGS +=	-I./ \
			-I../../3rdparty/statethread/include \
			-I../proxy/include
	
CFLAGS += -std=c++11		
CFLAGS += -fPIC

STATIC_LIBRARY += $(PROXY_PATH)/output/libproxy.a

STATIC_LIBRARY += $(PROXY_PATH)/output/openssl/lib/libssl.a \
				  $(PROXY_PATH)/output/openssl/lib/libcrypto.a

LDFLAGS +=  -L$(PROXY_PATH)/3rdparty/statethread \
			-L$(PROXY_PATH)/3rdparty/openssl/lib \
			-lst -llog4cxx  -pthread -ldl\
			-Wl,-rpath=$(PROXY_PATH)/3rdparty/statethread \
			-Wl,-rpath=$(PROXY_PATH)/3rdparty/openssl/lib	
#
include $(PROXY_PATH)/build/Makefile.project