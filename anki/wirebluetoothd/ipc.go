package main

import (
	"io"
	"log"
	"net"
	"os"
	"sync"
)

type ipcHandler interface {
	onSetAdapterName(name string)
	onStartAdvertising(args startAdvArgs)
	onStopAdvertising()
	onSendMessage(connID int32, uuid string, reliable bool, data []byte)
	onDisconnect(connID int32)
}

type ipcClient struct {
	conn net.Conn
	mu   sync.Mutex
}

func (c *ipcClient) send(msg []byte) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.conn.Write(msg)
}

type ipcServer struct {
	path     string
	listener net.Listener
	handler  ipcHandler
	mu       sync.Mutex
	clients  []*ipcClient
}

func newIPCServer(path string, h ipcHandler) *ipcServer {
	return &ipcServer{path: path, handler: h}
}

func (s *ipcServer) start() error {
	os.Remove(s.path)
	ln, err := net.Listen("unix", s.path)
	if err != nil {
		return err
	}
	s.listener = ln
	go s.accept()
	log.Printf("IPC server on %s", s.path)
	return nil
}

func (s *ipcServer) stop() {
	s.listener.Close()
}

func (s *ipcServer) accept() {
	for {
		conn, err := s.listener.Accept()
		if err != nil {
			return
		}
		log.Println("IPC client connected")
		c := &ipcClient{conn: conn}
		s.mu.Lock()
		s.clients = append(s.clients, c)
		s.mu.Unlock()
		go s.handleClient(c)
	}
}

func (s *ipcServer) removeClient(c *ipcClient) {
	c.conn.Close()
	s.mu.Lock()
	for i, cl := range s.clients {
		if cl == c {
			s.clients = append(s.clients[:i], s.clients[i+1:]...)
			break
		}
	}
	s.mu.Unlock()
	log.Println("IPC client disconnected")
}

func (s *ipcServer) handleClient(c *ipcClient) {
	defer s.removeClient(c)

	hdr := make([]byte, ipcHeaderSize)
	for {
		if _, err := io.ReadFull(c.conn, hdr); err != nil {
			return
		}
		t, length, ok := parseHeader(hdr)
		if !ok {
			log.Println("IPC: bad header, dropping client")
			return
		}
		var payload []byte
		if length > 0 {
			payload = make([]byte, length)
			if _, err := io.ReadFull(c.conn, payload); err != nil {
				return
			}
		}
		s.dispatch(c, t, payload)
	}
}

func (s *ipcServer) dispatch(c *ipcClient, t msgType, payload []byte) {
	switch t {
	case msgPing:
		c.send(encodeMessage(msgOnPingResponse, nil))

	case msgSetAdapterName:
		name := parseSetAdapterName(payload)
		log.Printf("IPC: SetAdapterName %q", name)
		s.handler.onSetAdapterName(name)

	case msgStartAdvertising:
		if args, ok := parseStartAdvertising(payload); ok {
			log.Println("IPC: StartAdvertising")
			s.handler.onStartAdvertising(args)
		} else {
			log.Printf("IPC: StartAdvertising parse failed (payload len=%d)", len(payload))
		}

	case msgStopAdvertising:
		log.Println("IPC: StopAdvertising")
		s.handler.onStopAdvertising()

	case msgSendMessage:
		connID, uuid, reliable, data, ok := parseSendMessage(payload)
		if ok {
			s.handler.onSendMessage(connID, uuid, reliable, data)
		}

	case msgDisconnect:
		if connID, ok := parseDisconnect(payload); ok {
			log.Printf("IPC: Disconnect connID=%d", connID)
			s.handler.onDisconnect(connID)
		}

	default:
		log.Printf("IPC: unhandled msg type %d (len=%d)", t, len(payload))
	}
}

func (s *ipcServer) broadcast(msg []byte) {
	s.mu.Lock()
	clients := make([]*ipcClient, len(s.clients))
	copy(clients, s.clients)
	s.mu.Unlock()
	for _, c := range clients {
		c.send(msg)
	}
}
