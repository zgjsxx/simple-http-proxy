FROM  rockylinux:9.0
RUN yum install -y gcc make gcc-c++ perl valgrind-devel
# Build and install SRS.
COPY ./3rdparty /simple-http-proxy/3rdparty
COPY ./src /simple-http-proxy/src
COPY ./build /simple-http-proxy/build
COPY ./conf /simple-http-proxy/conf
COPY ./configure /simple-http-proxy/configure
COPY ./Makefile /simple-http-proxy/Makefile

WORKDIR /simple-http-proxy/
RUN ./configure && make

EXPOSE 80 443 8080

CMD ["./output/myproxy", "-c", "conf/srs_docker.conf"]