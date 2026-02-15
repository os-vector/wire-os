btsnoop_net
====
btsnoop_net exposes Bluetooth snoop logs over a local TCP socket which enables
real-time debugging of HCI data with hcidump.

This feature is enabled by  setting `BtSnoopLogOutput=true` in `bt_stack.conf`.
Once it has been enabled and the stack restarted, the stack will listen for
incoming TCP connections on port 8872.

To use this feature with hcidump on a Linux host, you can run:

```
  $ adb forward tcp:8872 tcp:8872
  $ nc localhost 8872 | hcidump -r /dev/stdin
```
