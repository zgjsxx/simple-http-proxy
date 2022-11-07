# simple http proxy
[![c++](https://img.shields.io/static/v1?label=build&message=passing&color=green)](https://www.python.org/)
[![qt](https://img.shields.io/static/v1?label=statethread&message=1.9&color=blue)](https://www.python.org/)
[![qt](https://img.shields.io/static/v1?label=openssl&message=1.1.1l&color=blue)](https://www.python.org/)
## 1.introdution
simple_http_proxy is a http/https proxy, it used ossrs/state-threads as its base coroutine library.

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
- support http request
- support http connect method
- support https descryption
- support http chunked data

## 4.TO DO:
- move http body to file, curret is in memory  
- http keepalive  
- 301 redirect body process  
- access log  
- change openssl to release version  
- prevent JA3 blocked
- log socket label      
- support url with ip address
- support special website to do https tunnel
- to check connection manage, current it has a lot of CLOSE_WAIT

## other 

**http chunked data test sample**
http://www.httpwatch.com/httpgallery/chunked/chunkedimage.aspx
