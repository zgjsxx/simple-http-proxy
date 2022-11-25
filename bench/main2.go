package main

import (
  _ "bytes"
  _ "io/ioutil"
  "log"
  "net/http"
  "time"
  "bufio"
  "io"
  "os"
  "fmt"
  "crypto/tls"
  _ "sync"
  _ "io/ioutil"
  "bench/Util"
  "math/rand"
)
var num int = 1000000
func httpClient() *http.Client {
    client := &http.Client{
        Transport: &http.Transport{
                Proxy: http.ProxyFromEnvironment,
                TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
                MaxIdleConns:          50,
        },
        Timeout: 10 * time.Second,
    }

    return client
}

func prepare_test_url() {
    fi, err := os.Open("url2.txt")
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

func sendRequest(client *http.Client) {
    for i := 0; i < num; i++ {
        size := len(Util.Testurl)
        x1 := rand.NewSource(time.Now().UnixNano())
        y1 := rand.New(x1)
        index := y1.Intn(size)

        val := Util.Testurl[index]
        fmt.Printf("index = %d, test url = %s\n", index, val)

        req, err := http.NewRequest("GET", val, nil)
        if err != nil {
                        log.Fatalf("Error Occured. %+v", err)
        }

        response, err := client.Do(req)
        if err != nil {
                        log.Fatalf("Error sending request to API endpoint. %+v", err)
        }
        fmt.Printf("%s GET Response = %s \n", val, response.Status)
        // Close the connection to reuse it
        defer response.Body.Close()
    }
    return
}

func main() {
    prepare_test_url()
    c := httpClient()
    sendRequest(c)
}
