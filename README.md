# simple_http_proxy
[![c++](https://img.shields.io/static/v1?label=build&message=passing&color=green)](https://www.python.org/)
[![qt](https://img.shields.io/static/v1?label=statethread&message=1.9&color=blue)](https://www.python.org/)
[![qt](https://img.shields.io/static/v1?label=openssl&message=1.1.1l&color=blue)](https://www.python.org/)
## 1.introdution
simple_http_proxy is a http/https proxy, it used the coroutine with opensource library statethread.


## 2.deploy and test

```shell
./myproxy
```
And then you will see the proxy is work on 0.0.0.0:80.
After starting the proxy, you can set the proxy address in the system or install a plugin in the browser to set it.
You can see the certificate has been replaced by our proxy. It means the descryption is successfully.

![player](doc/img/img1.png)

## 3.current support features:
- support http  
- https descryption  
- http content length  
- http trunked body  
- log socket label      
- support url with ip address
- support special website to do https tunnel

## 4.TO DO:
- move http body to file, current it is in memory  
- http keepalive  
- 301 redirect body process  
- access log  
- change openssl to release version  
- prevent JA3 blocked
