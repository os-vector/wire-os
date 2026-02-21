package main

import (
	"fmt"
	"strings"
	"sync"

	dbus "github.com/godbus/dbus/v5"
)

var errUnknownProp = dbus.NewError("org.freedesktop.DBus.Error.UnknownProperty", nil)

const (
	appPath        = dbus.ObjectPath("/com/anki/ble")
	servicePath    = dbus.ObjectPath("/com/anki/ble/service0")
	writeCharPath  = dbus.ObjectPath("/com/anki/ble/service0/char0")
	notifyCharPath = dbus.ObjectPath("/com/anki/ble/service0/char1")
	advPath        = dbus.ObjectPath("/com/anki/ble/adv0")
)

const (
	serviceUUID    = "0000FEE3-0000-1000-8000-00805F9B34FB"
	writeCharUUID  = "7D2A4BDA-D29B-4152-B725-2491478C5CD7"
	notifyCharUUID = "30619F2D-0F54-41BD-A65A-7588D8C85B45"
)

const (
	bluezService   = "org.bluez"
	gattMgrIface   = "org.bluez.GattManager1"
	advMgrIface    = "org.bluez.LEAdvertisingManager1"
	adapter1Iface  = "org.bluez.Adapter1"
	gattSvc1Iface  = "org.bluez.GattService1"
	gattChar1Iface = "org.bluez.GattCharacteristic1"
	advIface       = "org.bluez.LEAdvertisement1"
	propIface      = "org.freedesktop.DBus.Properties"
	objMgrIface    = "org.freedesktop.DBus.ObjectManager"
)

type bluez struct {
	conn        *dbus.Conn
	adapterPath string
	ipc         *ipcServer

	mu                   sync.Mutex
	connID               int32
	notifying            bool
	connectionSignalSent bool
	lastValue            []byte
	adapterName          string
	advRegistered        bool
}

func newBluez(adapterPath string) (*bluez, error) {
	conn, err := dbus.SystemBus()
	if err != nil {
		return nil, fmt.Errorf("system bus: %w", err)
	}

	b := &bluez{
		conn:        conn,
		adapterPath: adapterPath,
		connID:      -1,
	}

	if err := b.exportObjects(); err != nil {
		conn.Close()
		return nil, err
	}
	if err := b.registerGATT(); err != nil {
		conn.Close()
		return nil, err
	}
	go b.watchDeviceConnections()
	return b, nil
}

func (b *bluez) setIPC(ipc *ipcServer) {
	b.ipc = ipc
}

func (b *bluez) Close() {
	b.conn.Close()
}

func (b *bluez) exportObjects() error {
	b.conn.Export(b, appPath, objMgrIface)

	b.conn.Export(staticProps(gattSvc1Iface, map[string]dbus.Variant{
		"UUID":    dbus.MakeVariant(serviceUUID),
		"Primary": dbus.MakeVariant(true),
	}), servicePath, propIface)

	wc := &writeChar{b: b}
	b.conn.Export(wc, writeCharPath, gattChar1Iface)
	b.conn.Export(staticProps(gattChar1Iface, map[string]dbus.Variant{
		"UUID":    dbus.MakeVariant(writeCharUUID),
		"Service": dbus.MakeVariant(servicePath),
		"Flags":   dbus.MakeVariant([]string{"write", "write-without-response"}),
	}), writeCharPath, propIface)

	nc := &notifyChar{b: b}
	b.conn.Export(nc, notifyCharPath, gattChar1Iface)
	b.conn.Export(nc, notifyCharPath, propIface)

	return nil
}

func (b *bluez) registerGATT() error {
	mgr := b.conn.Object(bluezService, dbus.ObjectPath(b.adapterPath))
	call := mgr.Call(gattMgrIface+".RegisterApplication", 0,
		appPath, map[string]dbus.Variant{})
	if call.Err != nil {
		return fmt.Errorf("RegisterApplication: %w", call.Err)
	}
	fmt.Println("GATT application registered with BlueZ")
	return nil
}

func (b *bluez) watchDeviceConnections() {
	b.conn.AddMatchSignal(
		dbus.WithMatchInterface("org.freedesktop.DBus.Properties"),
		dbus.WithMatchMember("PropertiesChanged"),
		dbus.WithMatchArg(0, "org.bluez.Device1"),
	)
	ch := make(chan *dbus.Signal, 16)
	b.conn.Signal(ch)
	for sig := range ch {
		if len(sig.Body) < 2 {
			continue
		}
		changed, ok := sig.Body[1].(map[string]dbus.Variant)
		if !ok {
			continue
		}
		if v, ok := changed["Connected"]; ok {
			if v.Value().(bool) {
				fmt.Printf("Device connected: %s", sig.Path)
				b.onCentralConnected()
			} else {
				fmt.Printf("Device disconnected: %s", sig.Path)
				b.onCentralDisconnected()
			}
		}
	}
}

func (b *bluez) onCentralConnected() {
	b.mu.Lock()
	if b.connID != -1 {
		b.mu.Unlock()
		return
	}
	b.connID = 1
	b.mu.Unlock()

	fmt.Println("Device connected - waiting for StartNotify before signaling switchboard")
}

func (b *bluez) onCentralDisconnected() {
	b.mu.Lock()
	oldID := b.connID
	b.connID = -1
	b.notifying = false
	b.connectionSignalSent = false
	b.mu.Unlock()

	if b.ipc != nil && oldID != -1 {
		b.ipc.broadcast(encodeOnInboundConnectionChange(oldID, 0))
		b.ipc.broadcast(encodeOnPeripheralStateUpdate(true, -1, 0, false))
	}
}

func (b *bluez) onSetAdapterName(name string) {
	b.mu.Lock()
	b.adapterName = name
	b.mu.Unlock()

	adapter := b.conn.Object(bluezService, dbus.ObjectPath(b.adapterPath))
	adapter.Call(propIface+".Set", 0, adapter1Iface, "Alias", dbus.MakeVariant(name))
	fmt.Printf("Adapter name set to %q", name)
}

func (b *bluez) onStartAdvertising(args startAdvArgs) {
	success := b.registerAdvertisement(args)
	fmt.Printf("Advertisement registration result: %v", success)
}

func (b *bluez) registerAdvertisement(args startAdvArgs) bool {
	b.mu.Lock()
	wasRegistered := b.advRegistered
	b.mu.Unlock()

	if wasRegistered {
		fmt.Println("Advertisement already registered, unregistering first")
		b.unregisterAdvertisement()
	}

	adv := &advObject{b: b, args: args}
	b.conn.Export(adv, advPath, advIface)
	b.conn.Export(adv, advPath, propIface)

	mgr := b.conn.Object(bluezService, dbus.ObjectPath(b.adapterPath))
	call := mgr.Call(advMgrIface+".RegisterAdvertisement", 0,
		advPath, map[string]dbus.Variant{})
	if call.Err != nil {
		fmt.Printf("RegisterAdvertisement failed: %v", call.Err)
		return false
	}

	b.mu.Lock()
	b.advRegistered = true
	b.mu.Unlock()
	fmt.Println("BLE advertisement registered")
	return true
}

func (b *bluez) unregisterAdvertisement() {
	mgr := b.conn.Object(bluezService, dbus.ObjectPath(b.adapterPath))
	mgr.Call(advMgrIface+".UnregisterAdvertisement", 0, advPath)
	b.mu.Lock()
	b.advRegistered = false
	b.mu.Unlock()
}

func (b *bluez) onStopAdvertising() {
	b.unregisterAdvertisement()
	fmt.Println("BLE advertisement stopped")
}

func (b *bluez) onSendMessage(connID int32, uuid string, reliable bool, data []byte) {
	if !strings.EqualFold(uuid, notifyCharUUID) {
		fmt.Printf("SendMessage to unknown UUID %s (ignored)", uuid)
		return
	}
	b.mu.Lock()
	b.lastValue = make([]byte, len(data))
	copy(b.lastValue, data)
	notifying := b.notifying
	b.mu.Unlock()

	if !notifying {
		fmt.Println("WARNING: Tried to send notification but client not subscribed")
		return
	}
	b.conn.Emit(notifyCharPath, propIface+".PropertiesChanged",
		gattChar1Iface,
		map[string]dbus.Variant{"Value": dbus.MakeVariant(data)},
		[]string{})
}

func (b *bluez) onDisconnect(connID int32) {
	b.onCentralDisconnected()
}

func (b *bluez) GetManagedObjects() (map[dbus.ObjectPath]map[string]map[string]dbus.Variant, *dbus.Error) {
	b.mu.Lock()
	val := b.lastValue
	notifying := b.notifying
	b.mu.Unlock()

	return map[dbus.ObjectPath]map[string]map[string]dbus.Variant{
		servicePath: {
			gattSvc1Iface: {
				"UUID":    dbus.MakeVariant(serviceUUID),
				"Primary": dbus.MakeVariant(true),
			},
		},
		writeCharPath: {
			gattChar1Iface: {
				"UUID":    dbus.MakeVariant(writeCharUUID),
				"Service": dbus.MakeVariant(servicePath),
				"Flags":   dbus.MakeVariant([]string{"write", "write-without-response"}),
			},
		},
		notifyCharPath: {
			gattChar1Iface: {
				"UUID":      dbus.MakeVariant(notifyCharUUID),
				"Service":   dbus.MakeVariant(servicePath),
				"Flags":     dbus.MakeVariant([]string{"read", "notify"}),
				"Value":     dbus.MakeVariant(val),
				"Notifying": dbus.MakeVariant(notifying),
			},
		},
	}, nil
}

type writeChar struct{ b *bluez }

func (c *writeChar) WriteValue(value []byte, opts map[string]dbus.Variant) *dbus.Error {
	c.b.mu.Lock()
	connID := c.b.connID
	c.b.mu.Unlock()
	if connID == -1 {
		fmt.Println("WriteValue before connection signal; using connID=1")
		connID = 1
	}
	if c.b.ipc != nil {
		c.b.ipc.broadcast(encodeOnReceiveMessage(connID, writeCharUUID, value))
	} else {
		fmt.Println("WARNING: WriteValue called but no IPC connection")
	}
	return nil
}

type notifyChar struct{ b *bluez }

func (c *notifyChar) ReadValue(opts map[string]dbus.Variant) ([]byte, *dbus.Error) {
	c.b.mu.Lock()
	v := c.b.lastValue
	c.b.mu.Unlock()
	return v, nil
}

func (c *notifyChar) StartNotify() *dbus.Error {
	c.b.mu.Lock()
	c.b.notifying = true

	if c.b.connID == -1 {
		c.b.connID = 1
		fmt.Println("StartNotify: device connected, assigning connID=1")
	}

	connID := c.b.connID
	alreadySent := c.b.connectionSignalSent
	hasIPC := c.b.ipc != nil

	if !alreadySent {
		c.b.connectionSignalSent = true
	}
	c.b.mu.Unlock()

	fmt.Printf("StartNotify: connID=%d, alreadySent=%v, hasIPC=%v", connID, alreadySent, hasIPC)

	c.b.conn.Emit(notifyCharPath, propIface+".PropertiesChanged",
		gattChar1Iface,
		map[string]dbus.Variant{"Notifying": dbus.MakeVariant(true)},
		[]string{})

	if !alreadySent && hasIPC {
		fmt.Printf("client subscribed to notifications, signaling switchboard (connID=%d)", connID)
		c.b.ipc.broadcast(encodeOnInboundConnectionChange(connID, 1))
	} else {
		fmt.Printf("NOT signaling switchboard: alreadySent=%v, hasIPC=%v", alreadySent, hasIPC)
	}

	return nil
}

func (c *notifyChar) StopNotify() *dbus.Error {
	fmt.Println("StopNotify")
	c.b.mu.Lock()
	c.b.notifying = false
	c.b.mu.Unlock()
	c.b.conn.Emit(notifyCharPath, propIface+".PropertiesChanged",
		gattChar1Iface,
		map[string]dbus.Variant{"Notifying": dbus.MakeVariant(false)},
		[]string{})

	// treat stopnotify as a disconnect event
	c.b.onCentralDisconnected()

	return nil
}

func (c *notifyChar) GetAll(iface string) (map[string]dbus.Variant, *dbus.Error) {
	if iface != gattChar1Iface {
		return map[string]dbus.Variant{}, nil
	}
	c.b.mu.Lock()
	v := c.b.lastValue
	notifying := c.b.notifying
	c.b.mu.Unlock()
	return map[string]dbus.Variant{
		"UUID":      dbus.MakeVariant(notifyCharUUID),
		"Service":   dbus.MakeVariant(servicePath),
		"Flags":     dbus.MakeVariant([]string{"read", "notify"}),
		"Value":     dbus.MakeVariant(v),
		"Notifying": dbus.MakeVariant(notifying),
	}, nil
}

func (c *notifyChar) Get(iface, name string) (dbus.Variant, *dbus.Error) {
	props, _ := c.GetAll(iface)
	if v, ok := props[name]; ok {
		return v, nil
	}
	return dbus.MakeVariant(nil), errUnknownProp
}

type advObject struct {
	b    *bluez
	args startAdvArgs
}

func (a *advObject) Release() *dbus.Error {
	fmt.Println("Advertisement released by BlueZ")
	a.b.mu.Lock()
	a.b.advRegistered = false
	a.b.mu.Unlock()
	return nil
}

func (a *advObject) GetAll(iface string) (map[string]dbus.Variant, *dbus.Error) {
	if iface != advIface {
		return map[string]dbus.Variant{}, nil
	}
	props := map[string]dbus.Variant{
		"Type": dbus.MakeVariant("peripheral"),
	}
	if a.args.adv.serviceUUID != "" {
		props["ServiceUUIDs"] = dbus.MakeVariant([]string{a.args.adv.serviceUUID})
	}

	if len(a.args.adv.manufacturerData) >= 2 {
		companyID := uint16(a.args.adv.manufacturerData[0]) | uint16(a.args.adv.manufacturerData[1])<<8
		mfgPayload := a.args.adv.manufacturerData[2:]
		props["ManufacturerData"] = dbus.MakeVariant(map[uint16]dbus.Variant{
			companyID: dbus.MakeVariant(mfgPayload),
		})
	}
	a.b.mu.Lock()
	name := a.b.adapterName
	a.b.mu.Unlock()
	if name != "" {
		props["LocalName"] = dbus.MakeVariant(name)
	}
	return props, nil
}

func (a *advObject) Get(iface, name string) (dbus.Variant, *dbus.Error) {
	props, _ := a.GetAll(iface)
	if v, ok := props[name]; ok {
		return v, nil
	}
	return dbus.MakeVariant(nil), errUnknownProp
}

type staticPropsObj struct {
	iface string
	data  map[string]dbus.Variant
}

func staticProps(iface string, data map[string]dbus.Variant) *staticPropsObj {
	return &staticPropsObj{iface: iface, data: data}
}

func (p *staticPropsObj) GetAll(iface string) (map[string]dbus.Variant, *dbus.Error) {
	if iface != p.iface {
		return map[string]dbus.Variant{}, nil
	}
	return p.data, nil
}

func (p *staticPropsObj) Get(iface, name string) (dbus.Variant, *dbus.Error) {
	if iface != p.iface {
		return dbus.MakeVariant(nil), errUnknownProp
	}
	if v, ok := p.data[name]; ok {
		return v, nil
	}
	return dbus.MakeVariant(nil), errUnknownProp
}
