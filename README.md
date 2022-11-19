# Simple Http Proxy
[![c++](https://img.shields.io/static/v1?label=build&message=passing&color=green)](https://www.python.org/)
[![qt](https://img.shields.io/static/v1?label=statethread&message=1.9&color=blue)](https://www.python.org/)
[![qt](https://img.shields.io/static/v1?label=openssl&message=1.1.1l&color=blue)](https://www.python.org/)
[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fzgjsxx%2Fsimple-http-proxy.svg?type=shield)](https://app.fossa.com/projects/git%2Bgithub.com%2Fzgjsxx%2Fsimple-http-proxy?ref=badge_shield)

simple_http_proxy is a http/https proxy, it used ossrs/state-threads as its base coroutine library.

It also has a inside http api server.

## Usage
To build simple http proxy from source:

```shell
git clone https://github.com:zgjsxx/simple-http-proxy.git &&
cd simple-http-proxy && ./configure && make &&
./output/myproxy -c conf/srs.conf
```

And then you will see the proxy is work on 0.0.0.0:8080.

After starting the proxy, you can set the proxy address in the system or install a plugin in the browser to set it.
When you visit the website by the browser, you can see the certificate has been replaced by our proxy. It means the descryption is successful.

![player](doc/img/img1.png)

## Features

Please read [FEATURES](doc/features.md).

## Ports

Please read [PORTS](doc/resources.md#ports).

## TODO

Please read [TODO](doc/todo.md).


## License
[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fzgjsxx%2Fsimple-http-proxy.svg?type=large)](https://app.fossa.com/projects/git%2Bgithub.com%2Fzgjsxx%2Fsimple-http-proxy?ref=badge_large)


NJ, 2022.12

zgjsxx
