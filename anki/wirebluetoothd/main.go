package main

import (
	"fmt"
	"os"
	"os/signal"
	"syscall"
)

var socket string = "/data/misc/bluetooth/abtd.socket"
var adapter string = "/org/bluez/hci0"

func main() {
	fmt.Printf("Starting — adapter %s, socket %s", adapter, socket)

	b, err := newBluez(adapter)
	if err != nil {
		panic(fmt.Sprintf("blez init: %v", err))
	}
	defer b.Close()

	srv := newIPCServer(socket, b)
	if err := srv.start(); err != nil {
		panic(fmt.Sprintf("IPC server: %v", err))
	}
	defer srv.stop()

	b.setIPC(srv)

	sig := make(chan os.Signal, 1)
	signal.Notify(sig, syscall.SIGINT, syscall.SIGTERM)
	<-sig
	fmt.Println("Shutting down")
}
