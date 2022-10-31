PROXY_PATH := $(shell cd ../../; pwd)

include $(PROXY_PATH)/build/Makefile.path
include $(PROXY_PATH)/build/Makefile.common
#
# Target
#
MODULE = libkernel.a
BINARY_TYPE = a

#
# Sources
#

SOURCE_PATH +=	./



CFLAGS +=	-I./ \
			-I../../3rdparty/statethread/include \
			-std=c++11

#
include $(PROXY_PATH)/build/Makefile.project