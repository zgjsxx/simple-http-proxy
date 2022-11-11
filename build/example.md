- [example](#example)
	- [example1](#example1)
# example

## example1
```
#├── Makefile
#├── src
#│   ├── srs_utest_kernel_utility.cpp
#│   └── srs_utest_kernel_utility.hpp
#├── srs_utest_core.cpp
#├── srs_utest_core.hpp
#├── srs_utest_main.cpp
#└── srs_utest_main.hpp
```

You can set：
ADDITIONAL_CPP_SOURCES += srs_utest_main.cpp \
						  srs_utest_core.cpp \
						  srs_utest_kernel_utility.cpp

ADDITIONAL_SOURCE_PATH += ./src


or you can 
ADDITIONAL_SOURCE_COMPILE_PATH = ./

