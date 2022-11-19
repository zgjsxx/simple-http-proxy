package main

import (
  "fmt"
  "crypto/tls"
  "net/http"
  "sync"
  _ "io/ioutil"
  "bench/Util"
)

var wg sync.WaitGroup

func testUrl(){
    var PTransport = & http.Transport {
        Proxy: http.ProxyFromEnvironment,
        TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
      }
      client := http.Client {
        Transport: PTransport,
      }
    
      for _, val := range Util.Testurl {
        req, err := http.NewRequest("GET", val, nil)
        // req.Header.Add("If-None-Match", `some value`)
        resp, err := client.Do(req)
        if err != nil {
        //   panic(err)
            fmt.Printf("err \n")
            continue
        }
        fmt.Printf("GET Response = %s \n", resp.Status)
        // defer resp.Body.Close()
      
        // bodyBytes, err := ioutil.ReadAll(resp.Body)
        // if err != nil {
        //   panic(err)
        // }
      
        // bodyString := string(bodyBytes)
        // fmt.Printf("GET Response = %s \n", string(bodyString))
      }
      wg.Done()
}

func main() {
    for i := 0; i < 3000; i++ {
        wg.Add(1)
        go testUrl()
    }

    wg.Wait()
    fmt.Println("over")

}