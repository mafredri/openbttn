package main

import (
	"bytes"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"math/rand"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"time"
)

var bttnRedirectFiles = []string{
	"output.cgi",
	"config.json",
	"config.shtml",
	"status.shtml",
	"peers.shtml",
}

var testStatuses = []int{
	http.StatusOK,
	http.StatusAccepted,
	http.StatusNotFound,
	http.StatusInternalServerError,
}

// notFoundHandler just returns a 404.
func notFoundHandler(w http.ResponseWriter, r *http.Request) {
	log.Printf("HTTP %s - %s", r.Method, r.URL.Path)

	w.WriteHeader(http.StatusNotFound)
	fmt.Fprintf(w, "404 Not Found - OpenBttn Server")
}

// testHandler returns a random HTTP status after a random delay
// (up to 4 seconds). It's useful when testing button reactions
// to different response codes and delays.
func testHandler(w http.ResponseWriter, r *http.Request) {
	log.Printf("HTTP %s - %s", r.Method, r.URL.Path)

	time.Sleep(time.Duration(rand.Intn(4000)) * time.Millisecond)

	w.WriteHeader(testStatuses[rand.Intn(len(testStatuses))])
	fmt.Fprintf(w, "Hi there, I love %s!\n - OpenBttn", r.URL.Path[6:])
}

// bttnSocketRedirectHandler redirects requests to the bttn Socket server.
func bttnSocketRedirectHandler(bttnIP string) func(w http.ResponseWriter, r *http.Request) {
	return func(w http.ResponseWriter, r *http.Request) {
		log.Printf("HTTP %s - %s", r.Method, r.URL.Path)

		url := fmt.Sprintf("http://%s:8774%s", bttnIP, r.URL.RequestURI())
		req, err := http.NewRequest(r.Method, url, r.Body)

		req.ContentLength = r.ContentLength
		req.Header.Set("Content-Type", r.Header.Get("Content-Type"))

		resp, err := http.DefaultClient.Do(req)
		if err != nil {
			panic(err)
		}
		defer resp.Body.Close()

		b, err := ioutil.ReadAll(resp.Body)
		if err != nil {
			panic(err)
		}

		w.WriteHeader(resp.StatusCode)
		w.Write(b)
	}
}

// bttnRedirectHandler requests a file from the bttn HTTP server and returns it
// as if it was a local resource to this server. This is useful for testing the
// HTML files that are to be served from the bttn.
func bttnRedirectHandler(bttnIP string) func(w http.ResponseWriter, r *http.Request) {
	return func(w http.ResponseWriter, r *http.Request) {
		log.Printf("HTTP %s - %s", r.Method, r.URL.Path)

		url := "http://" + bttnIP + r.URL.RequestURI()
		log.Printf("GET %s (from bttn)", url)

		req, err := http.Get(url)
		if err != nil {
			panic(err)
		}

		defer req.Body.Close()
		b, err := ioutil.ReadAll(req.Body)
		if err != nil {
			panic(err)
		}

		w.WriteHeader(http.StatusOK)
		w.Write(b)
	}
}

// otaHandler servers OTA files for the SPWF01SA module from the provided path.
// By serving them in 4096 byte chunks with a 300 ms delay between chunks, the
// OTA update becomes much more reliable. Without the delay, the failure rate is
// extremely high. The issue lies probably with the SPWF01SA module not handling
// the high bandwidth of a local connection.
func otaHandler(otaPath string) func(w http.ResponseWriter, r *http.Request) {
	return func(w http.ResponseWriter, r *http.Request) {
		log.Printf("HTTP %s - %s", r.Method, r.URL.Path)
		start := time.Now()

		filename := fmt.Sprintf("%s/%s", otaPath, filepath.Base(r.URL.Path))
		f, err := os.Open(filename)
		if err != nil {
			log.Printf("otaHandler: %v", err)
			w.WriteHeader(http.StatusNotFound)
			return
		}
		defer f.Close()

		s, err := f.Stat()
		if err != nil {
			panic(err)
		}

		size := strconv.Itoa(int(s.Size()))
		w.Header().Set("Content-Length", size)
		w.Header().Set("Content-Type", "application/octet-stream")
		w.WriteHeader(http.StatusOK)

		// The SPWF01SA module stores the
		// firmware in 4096 byte chunks.
		b := make([]byte, 4096)
		numBytes := 0
		keepGoing := true
		for keepGoing {
			n, err := f.Read(b)
			if err == io.EOF {
				keepGoing = false
				if n == 0 {
					break
				}
			} else if err != nil {
				panic(err)
			}

			content := bytes.NewReader(b)
			io.CopyN(w, content, int64(n))
			numBytes += n

			log.Printf("Sent %d/%s OTA bytes in %v", numBytes, size, time.Now().Sub(start))

			// Pause a bit between chunks to give some
			// time for the module to store each chunk.
			delay := 300 * time.Millisecond
			select {
			case <-r.Context().Done():
				log.Printf("Client disconnected, sent %d/%s OTA bytes in %v", numBytes, size, time.Now().Sub(start))
				return
			case <-time.After(delay):
			}
		}

		log.Println("OTA complete!")
	}
}

func main() {
	var (
		bttnIP     string
		publicPath string
		otaPath    string
	)

	flag.StringVar(&bttnIP, "ip", "192.168.0.188", "IP to bttn, used for ")
	flag.StringVar(&publicPath, "public", "../public", "Path to public HTML directory")
	flag.StringVar(&otaPath, "ota", "./ota", "Path to OTA directory")
	flag.Parse()

	http.HandleFunc("/", notFoundHandler)
	http.HandleFunc("/socket", bttnSocketRedirectHandler(bttnIP))
	http.HandleFunc("/test/", testHandler)
	http.Handle("/public/", http.StripPrefix("/public/", http.FileServer(http.Dir(publicPath))))

	for _, f := range bttnRedirectFiles {
		http.HandleFunc("/"+f, bttnRedirectHandler(bttnIP))
	}

	// Serve OTA files for the SPWF01SA Wi-Fi module.
	http.HandleFunc("/ota/", otaHandler(otaPath))

	http.ListenAndServe(":8774", nil)
}
