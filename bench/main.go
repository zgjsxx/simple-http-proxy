package main

import (
  "bufio"
  "io"
  "os"    
  "fmt"
  "crypto/tls"
  "net/http"
  "sync"
  _ "io/ioutil"
  "bench/Util"
  "time"
  "math/rand"
)

var wg sync.WaitGroup
var num int = 100
func testUrl(){
    var PTransport = & http.Transport {
        Proxy: http.ProxyFromEnvironment,
        TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
        MaxIdleConns:          5,
    }
    client := http.Client {
        Transport: PTransport,
    }
    
    for i := 0; i < num; i++ {
        size := len(Util.Testurl)
        x1 := rand.NewSource(time.Now().UnixNano())
        y1 := rand.New(x1)
        index := y1.Intn(size)
        
        val := Util.Testurl[index]
        fmt.Printf("index = %d, test url = %s\n", index, val) 

        req, err := http.NewRequest("GET", val, nil)
        resp, err := client.Do(req)
        if err != nil {
            fmt.Printf("%s err \n", val)
            continue
            time.Sleep(1 * time.Second)
        }
        fmt.Printf("%s GET Response = %s \n", val, resp.Status)
        time.Sleep(1 * time.Second)
        defer resp.Body.Close()
    }

    wg.Done()
}

func prepare_test_url() {
    fi, err := os.Open("url.txt")
    if err != nil {
        fmt.Printf("Error: %s\n", err)
        return
    }
    defer fi.Close()

    br := bufio.NewReader(fi)
    for {
        a, _, c := br.ReadLine()
        if c == io.EOF {
            break
        }
        fmt.Println(string(a))
        Util.Testurl = append(Util.Testurl, string(a))
    }
}

func main() {
    prepare_test_url()
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go testUrl()
    }

    wg.Wait()
    fmt.Println("over")

}
