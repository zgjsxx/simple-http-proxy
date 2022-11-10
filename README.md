# simple http proxy
[![c++](https://img.shields.io/static/v1?label=build&message=passing&color=green)](https://www.python.org/)
[![qt](https://img.shields.io/static/v1?label=statethread&message=1.9&color=blue)](https://www.python.org/)
[![qt](https://img.shields.io/static/v1?label=openssl&message=1.1.1l&color=blue)](https://www.python.org/)
## 1.introdution
simple_http_proxy is a http/https proxy, it used ossrs/state-threads as its base coroutine library.

It also has a inside http api server.

## 2.build
```shell
./configure
# then run make to compile
make 
```

## 23.deploy and test

```shell
./output/myproxy -c conf/srs.conf
```

And then you will see the proxy is work on 0.0.0.0:80.
After starting the proxy, you can set the proxy address in the system or install a plugin in the browser to set it.
You can see the certificate has been replaced by our proxy. It means the descryption is successfully.

![player](doc/img/img1.png)

## 3.current support features:
- support http/https api
- support forward http request
- support http connect method
- support https descryption
- support http chunked data
- support block list
- support utest with googletest
## 4.TO DO:
- support special website to do https tunnel
- support access log  
- http keepalive  
- prevent JA3 blocked
- log socket label   
- support nex hip 
- support utest  
- add  phsishing database https://github.com/mitchellkrogza/Phishing.Database
## 5.test samples 

**http chunked data test sample**
http://www.httpwatch.com/httpgallery/chunked/chunkedimage.aspx


## 6. face issues
1. http chunked data parse
2. tls need add SNI
3. close wait issue
4. srs framework error code
5. need to support HTTP options method
6. http/https write flow