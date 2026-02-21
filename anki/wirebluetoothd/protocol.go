package main

import "encoding/binary"

type msgType uint32

const (
	msgInvalid                            msgType = 0
	msgPing                               msgType = 1
	msgOnPingResponse                     msgType = 2
	msgSendMessage                        msgType = 3
	msgOnInboundConnectionChange          msgType = 4
	msgOnReceiveMessage                   msgType = 5
	msgDisconnect                         msgType = 6
	msgStartAdvertising                   msgType = 7
	msgStopAdvertising                    msgType = 8
	msgOnPeripheralStateUpdate            msgType = 9
	msgStartScan                          msgType = 10
	msgStopScan                           msgType = 11
	msgOnScanResults                      msgType = 12
	msgConnectToPeripheral                msgType = 13
	msgOnOutboundConnectionChange         msgType = 14
	msgCharacteristicReadRequest          msgType = 15
	msgOnCharacteristicReadResult         msgType = 16
	msgDescriptorReadRequest              msgType = 17
	msgOnDescriptorReadResult             msgType = 18
	msgRequestConnectionParameterUpdate   msgType = 19
	msgOnRequestConnectionParameterUpdate msgType = 20
	msgSetAdapterName                     msgType = 21
	msgDisconnectByAddress                msgType = 22
)

const (
	ipcHeaderSize   = 16
	ipcVersion      = uint32(1)
	uuidSize        = 37
	adapterNameSize = 240
	mfgDataMaxSize  = 32
	svcDataMaxSize  = 32
)

var ipcMagic = [4]byte{'i', 'p', 'c', 'f'}

func encodeMessage(t msgType, payload []byte) []byte {
	buf := make([]byte, ipcHeaderSize+len(payload))
	copy(buf[0:4], ipcMagic[:])
	binary.LittleEndian.PutUint32(buf[4:], ipcVersion)
	binary.LittleEndian.PutUint32(buf[8:], uint32(t))
	binary.LittleEndian.PutUint32(buf[12:], uint32(len(payload)))
	copy(buf[ipcHeaderSize:], payload)
	return buf
}

func parseHeader(buf []byte) (t msgType, length uint32, ok bool) {
	if len(buf) < ipcHeaderSize {
		return
	}
	if buf[0] != 'i' || buf[1] != 'p' || buf[2] != 'c' || buf[3] != 'f' {
		return
	}
	if binary.LittleEndian.Uint32(buf[4:]) != ipcVersion {
		return
	}
	t = msgType(binary.LittleEndian.Uint32(buf[8:]))
	length = binary.LittleEndian.Uint32(buf[12:])
	ok = true
	return
}

// connection_id(int32) + connected(int32)
func encodeOnInboundConnectionChange(connID, connected int32) []byte {
	b := make([]byte, 8)
	binary.LittleEndian.PutUint32(b[0:], uint32(connID))
	binary.LittleEndian.PutUint32(b[4:], uint32(connected))
	return encodeMessage(msgOnInboundConnectionChange, b)
}

// advertising(bool/1) + connection_id(int32) + connected(int32) + congested(bool/1)
func encodeOnPeripheralStateUpdate(advertising bool, connID, connected int32, congested bool) []byte {
	b := make([]byte, 10)
	if advertising {
		b[0] = 1
	}
	binary.LittleEndian.PutUint32(b[1:], uint32(connID))
	binary.LittleEndian.PutUint32(b[5:], uint32(connected))
	if congested {
		b[9] = 1
	}
	return encodeMessage(msgOnPeripheralStateUpdate, b)
}

// connection_id(int32) + characteristic_uuid[37] + length(uint32) + value[]
func encodeOnReceiveMessage(connID int32, charUUID string, data []byte) []byte {
	b := make([]byte, 4+uuidSize+4+len(data))
	binary.LittleEndian.PutUint32(b[0:], uint32(connID))
	copy(b[4:4+uuidSize], []byte(charUUID))
	binary.LittleEndian.PutUint32(b[4+uuidSize:], uint32(len(data)))
	copy(b[4+uuidSize+4:], data)
	return encodeMessage(msgOnReceiveMessage, b)
}

// connection_id(int32) + characteristic_uuid[37] + reliable(bool/1) + length(uint32) + value[]
func parseSendMessage(p []byte) (connID int32, uuid string, reliable bool, data []byte, ok bool) {
	const fixed = 4 + uuidSize + 1 + 4
	if len(p) < fixed {
		return
	}
	connID = int32(binary.LittleEndian.Uint32(p[0:]))
	uuid = cstring(p[4 : 4+uuidSize])
	reliable = p[4+uuidSize] != 0
	dataLen := binary.LittleEndian.Uint32(p[4+uuidSize+1:])
	end := fixed + int(dataLen)
	if len(p) < end {
		return
	}
	data = p[fixed:end]
	ok = true
	return
}

// [0]      include_device_name  (bool)
// [1]      include_tx_power     (bool)
// [2:6]    manufacturer_data_len (int32)
// [6:38]   manufacturer_data    ([32]byte)
// [38:42]  service_data_len     (int32)
// [42:74]  service_data         ([32]byte)
// [74]     have_service_uuid    (bool)
// [75:112] service_uuid         ([37]byte)
// [112:116] min_interval        (int32)
// [116:120] max_interval        (int32)
const advDataSize = 120

type advData struct {
	includeDeviceName bool
	includeTxPower    bool
	manufacturerData  []byte
	serviceData       []byte
	serviceUUID       string
	minInterval       int32
	maxInterval       int32
}

type startAdvArgs struct {
	appearance int32
	adv        advData
	scanResp   advData
}

func parseAdvData(b []byte) advData {
	var d advData
	d.includeDeviceName = b[0] != 0
	d.includeTxPower = b[1] != 0
	mfgLen := int(int32(binary.LittleEndian.Uint32(b[2:])))
	if mfgLen > 0 && mfgLen <= mfgDataMaxSize {
		d.manufacturerData = make([]byte, mfgLen)
		copy(d.manufacturerData, b[6:6+mfgLen])
	}
	svcLen := int(int32(binary.LittleEndian.Uint32(b[38:])))
	if svcLen > 0 && svcLen <= svcDataMaxSize {
		d.serviceData = make([]byte, svcLen)
		copy(d.serviceData, b[42:42+svcLen])
	}
	if b[74] != 0 {
		d.serviceUUID = cstring(b[75 : 75+uuidSize])
	}
	d.minInterval = int32(binary.LittleEndian.Uint32(b[112:]))
	d.maxInterval = int32(binary.LittleEndian.Uint32(b[116:]))
	return d
}

func parseStartAdvertising(p []byte) (startAdvArgs, bool) {
	const need = 4 + advDataSize + advDataSize
	if len(p) < need {
		return startAdvArgs{}, false
	}
	var a startAdvArgs
	a.appearance = int32(binary.LittleEndian.Uint32(p[0:]))
	a.adv = parseAdvData(p[4:])
	a.scanResp = parseAdvData(p[4+advDataSize:])
	return a, true
}

func parseSetAdapterName(p []byte) string {
	if len(p) < adapterNameSize {
		return ""
	}
	return cstring(p[:adapterNameSize])
}

func parseDisconnect(p []byte) (int32, bool) {
	if len(p) < 4 {
		return 0, false
	}
	return int32(binary.LittleEndian.Uint32(p)), true
}

func cstring(b []byte) string {
	for i, c := range b {
		if c == 0 {
			return string(b[:i])
		}
	}
	return string(b)
}
