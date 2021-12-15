PROXY_PATH := $(shell cd ../../; pwd)

include $(PROXY_PATH)/src/build/Makefile.path
include $(PROXY_PATH)/src/build/Makefile.common
#
# Target
#
MODULE = libproxy.so
BINARY_TYPE = so

#
# Sources
#

SOURCE_PATH +=	src	



CFLAGS +=	-I./include \
			-I../import/openssl/include \
			-std=c++11

#
include $(PROXY_PATH)/src/build/Makefile.project