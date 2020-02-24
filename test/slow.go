// usage: cat file | slow -delay 10ms
// throttles the input such that each byte is written `delay` seconds apart.

package main

import (
	"flag"
	"io"
	"os"
	"time"
)

func main() {
	var delay time.Duration
	var buf [4096]byte

	flag.DurationVar(&delay, "delay", 10*time.Millisecond, "inter-byte delay")
	flag.Parse()

	for {
		n, err := os.Stdin.Read(buf[:])
		for i := 0; i < n; i++ {
			if _, err := os.Stdout.Write(buf[i : i+1]); err != nil {
				return
			}
			time.Sleep(delay)
		}
		if err == io.EOF {
			break
		}

	}
}
