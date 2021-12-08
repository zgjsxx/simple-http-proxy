all:
	g++ src/myproxy.cpp src/ServerLog.cpp -o myproxy -I./include  -L./import -lst -llog4cxx -Wl,-rpath=/home/work/simple_proxy_st/import/
