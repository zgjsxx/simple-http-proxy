# simple http proxy
[![c++](https://img.shields.io/static/v1?label=build&message=passing&color=green)](https://www.python.org/)
[![qt](https://img.shields.io/static/v1?label=statethread&message=1.9&color=blue)](https://www.python.org/)
[![qt](https://img.shields.io/static/v1?label=openssl&message=1.1.1l&color=blue)](https://www.python.org/)
[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fzgjsxx%2Fsimple-http-proxy.svg?type=shield)](https://app.fossa.com/projects/git%2Bgithub.com%2Fzgjsxx%2Fsimple-http-proxy?ref=badge_shield)
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
- support forward http/https traffic
- support descrypt https traffic
- support http/1.1 protocol
- support TLS1.2 protocol
- support domain block list
- support utest with googletest
- support log trace with coroutine id or client/server socket tag
- support https global tunnel and tunnel by domain
- support multiple set-cookie header
- support http keepalive  
- support url category machine learning (https://github.com/domantasm96/URL-categorization-using-machine-learning)
- support configure next hip proxy 
- support access log  
- support get a chunk data and then send a chunk data, instead of getting all body and then transfer to client
## 4.TO DO:
- async dns query
- support docker/k8s
- support content-type block
- support client ip block
- url category https://data.world/crowdflower/url-categorization
- prevent JA3 blocked
- add phsishing database https://github.com/mitchellkrogza/Phishing.Database
- support TLS1.3


## 5.test samples 

**http chunked data test sample**
http://www.httpwatch.com/httpgallery/chunked/chunkedimage.aspx

https://blog.csdn.net/qq_36256590/article/details/114707235

## 6. face issues
1. http chunked data parse
2. tls need add SNI
3. close wait issue
4. srs framework error code
5. need to support HTTP options method
6. http/https write flow


 /etc/security/limits.conf
 *               -       core            unlimited

## License
[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fzgjsxx%2Fsimple-http-proxy.svg?type=large)](https://app.fossa.com/projects/git%2Bgithub.com%2Fzgjsxx%2Fsimple-http-proxy?ref=badge_large)