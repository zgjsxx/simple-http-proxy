PROXY_PATH := $(shell cd ../../; pwd)

include $(PROXY_PATH)/build/Makefile.path
include $(PROXY_PATH)/build/Makefile.common
#
# Target
#
MODULE = libproxy.a
BINARY_TYPE = a

#
# Sources
#

SOURCE_PATH +=	src	


STATIC_LIBRARY += $(PROXY_PATH)/output/openssl/lib/libssl.a \
				  $(PROXY_PATH)/output/openssl/lib/libcrypto.a

CFLAGS +=	-I./include \
			-I../../3rdparty/openssl/include \
			-std=c++11

#
include $(PROXY_PATH)/build/Makefile.project