#!/usr/bin/python
#
# Copyright 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import fcntl
import os
from socket import *  # pylint: disable=wildcard-import
import struct
import unittest

from scapy import all as scapy

SOL_IPV6 = 41
IP_RECVERR = 11
IPV6_RECVERR = 25
IP_TRANSPARENT = 19
IPV6_TRANSPARENT = 75
IPV6_TCLASS = 67
SO_BINDTODEVICE = 25
SO_MARK = 36
IPV6_FLOWLABEL_MGR = 32
IPV6_FLOWINFO_SEND = 33

ETH_P_IP = 0x0800
ETH_P_IPV6 = 0x86dd

IPPROTO_GRE = 47

SIOCSIFHWADDR = 0x8924

IPV6_FL_A_GET = 0
IPV6_FL_A_PUT = 1
IPV6_FL_A_RENEW = 1

IPV6_FL_F_CREATE = 1
IPV6_FL_F_EXCL = 2

IPV6_FL_S_NONE = 0
IPV6_FL_S_EXCL = 1
IPV6_FL_S_ANY = 255

IPV4_PING = "\x08\x00\x00\x00\x0a\xce\x00\x03"
IPV6_PING = "\x80\x00\x00\x00\x0a\xce\x00\x03"

IPV4_ADDR = "8.8.8.8"
IPV6_ADDR = "2001:4860:4860::8888"

IPV6_SEQ_DGRAM_HEADER = ("  sl  "
                         "local_address                         "
                         "remote_address                        "
                         "st tx_queue rx_queue tr tm->when retrnsmt"
                         "   uid  timeout inode ref pointer drops\n")

# Unix group to use if we want to open sockets as non-root.
AID_INET = 3003


def LinuxVersion():
  # Example: "3.4.67-00753-gb7a556f".
  # Get the part before the dash.
  version = os.uname()[2].split("-")[0]
  # Convert it into a tuple such as (3, 4, 67). That allows comparing versions
  # using < and >, since tuples are compared lexicographically.
  version = tuple(int(i) for i in version.split("."))
  return version


LINUX_VERSION = LinuxVersion()


def SetSocketTimeout(sock, ms):
  s = ms / 1000
  us = (ms % 1000) * 1000
  sock.setsockopt(SOL_SOCKET, SO_RCVTIMEO, struct.pack("LL", s, us))


def SetSocketTos(s, tos):
  level = {AF_INET: SOL_IP, AF_INET6: SOL_IPV6}[s.family]
  option = {AF_INET: IP_TOS, AF_INET6: IPV6_TCLASS}[s.family]
  s.setsockopt(level, option, tos)


def SetNonBlocking(fd):
  flags = fcntl.fcntl(fd, fcntl.F_GETFL, 0)
  fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)


# Convenience functions to create sockets.
def Socket(family, sock_type, protocol):
  s = socket(family, sock_type, protocol)
  SetSocketTimeout(s, 1000)
  return s


def PingSocket(family):
  proto = {AF_INET: IPPROTO_ICMP, AF_INET6: IPPROTO_ICMPV6}[family]
  return Socket(family, SOCK_DGRAM, proto)


def IPv4PingSocket():
  return PingSocket(AF_INET)


def IPv6PingSocket():
  return PingSocket(AF_INET6)


def TCPSocket(family):
  s = Socket(family, SOCK_STREAM, IPPROTO_TCP)
  SetNonBlocking(s.fileno())
  return s


def IPv4TCPSocket():
  return TCPSocket(AF_INET)


def IPv6TCPSocket():
  return TCPSocket(AF_INET6)


def UDPSocket(family):
  return Socket(family, SOCK_DGRAM, IPPROTO_UDP)


def RawGRESocket(family):
  s = Socket(family, SOCK_RAW, IPPROTO_GRE)
  return s


def GetInterfaceIndex(ifname):
  s = IPv4PingSocket()
  ifr = struct.pack("16si", ifname, 0)
  ifr = fcntl.ioctl(s, scapy.SIOCGIFINDEX, ifr)
  return struct.unpack("16si", ifr)[1]


def SetInterfaceHWAddr(ifname, hwaddr):
  s = IPv4PingSocket()
  hwaddr = hwaddr.replace(":", "")
  hwaddr = hwaddr.decode("hex")
  if len(hwaddr) != 6:
    raise ValueError("Unknown hardware address length %d" % len(hwaddr))
  ifr = struct.pack("16sH6s", ifname, scapy.ARPHDR_ETHER, hwaddr)
  fcntl.ioctl(s, SIOCSIFHWADDR, ifr)


def SetInterfaceState(ifname, up):
  s = IPv4PingSocket()
  ifr = struct.pack("16sH", ifname, 0)
  ifr = fcntl.ioctl(s, scapy.SIOCGIFFLAGS, ifr)
  _, flags = struct.unpack("16sH", ifr)
  if up:
    flags |= scapy.IFF_UP
  else:
    flags &= ~scapy.IFF_UP
  ifr = struct.pack("16sH", ifname, flags)
  ifr = fcntl.ioctl(s, scapy.SIOCSIFFLAGS, ifr)


def SetInterfaceUp(ifname):
  return SetInterfaceState(ifname, True)


def SetInterfaceDown(ifname):
  return SetInterfaceState(ifname, False)


def FormatProcAddress(unformatted):
  groups = []
  for i in xrange(0, len(unformatted), 4):
    groups.append(unformatted[i:i+4])
  formatted = ":".join(groups)
  # Compress the address.
  address = inet_ntop(AF_INET6, inet_pton(AF_INET6, formatted))
  return address


def FormatSockStatAddress(address):
  if ":" in address:
    family = AF_INET6
  else:
    family = AF_INET
  binary = inet_pton(family, address)
  out = ""
  for i in xrange(0, len(binary), 4):
    out += "%08X" % struct.unpack("=L", binary[i:i+4])
  return out


def GetLinkAddress(ifname, linklocal):
  addresses = open("/proc/net/if_inet6").readlines()
  for address in addresses:
    address = [s for s in address.strip().split(" ") if s]
    if address[5] == ifname:
      if (linklocal and address[0].startswith("fe80")
          or not linklocal and not address[0].startswith("fe80")):
        # Convert the address from raw hex to something with colons in it.
        return FormatProcAddress(address[0])
  return None


def GetDefaultRoute(version=6):
  if version == 6:
    routes = open("/proc/net/ipv6_route").readlines()
    for route in routes:
      route = [s for s in route.strip().split(" ") if s]
      if (route[0] == "00000000000000000000000000000000" and route[1] == "00"
          # Routes in non-default tables end up in /proc/net/ipv6_route!!!
          and route[9] != "lo" and not route[9].startswith("nettest")):
        return FormatProcAddress(route[4]), route[9]
    raise ValueError("No IPv6 default route found")
  elif version == 4:
    routes = open("/proc/net/route").readlines()
    for route in routes:
      route = [s for s in route.strip().split("\t") if s]
      if route[1] == "00000000" and route[7] == "00000000":
        gw, iface = route[2], route[0]
        gw = inet_ntop(AF_INET, gw.decode("hex")[::-1])
        return gw, iface
    raise ValueError("No IPv4 default route found")
  else:
    raise ValueError("Don't know about IPv%s" % version)


def GetDefaultRouteInterface():
  unused_gw, iface = GetDefaultRoute()
  return iface


def MakeFlowLabelOption(addr, label):
  # struct in6_flowlabel_req {
  #         struct in6_addr flr_dst;
  #         __be32  flr_label;
  #         __u8    flr_action;
  #         __u8    flr_share;
  #         __u16   flr_flags;
  #         __u16   flr_expires;
  #         __u16   flr_linger;
  #         __u32   __flr_pad;
  #         /* Options in format of IPV6_PKTOPTIONS */
  # };
  fmt = "16sIBBHHH4s"
  assert struct.calcsize(fmt) == 32
  addr = inet_pton(AF_INET6, addr)
  assert len(addr) == 16
  label = htonl(label & 0xfffff)
  action = IPV6_FL_A_GET
  share = IPV6_FL_S_ANY
  flags = IPV6_FL_F_CREATE
  pad = "\x00" * 4
  return struct.pack(fmt, addr, label, action, share, flags, 0, 0, pad)


def SetFlowLabel(s, addr, label):
  opt = MakeFlowLabelOption(addr, label)
  s.setsockopt(SOL_IPV6, IPV6_FLOWLABEL_MGR, opt)
  # Caller also needs to do s.setsockopt(SOL_IPV6, IPV6_FLOWINFO_SEND, 1).


# Determine network configuration.
try:
  GetDefaultRoute(version=4)
  HAVE_IPV4 = True
except ValueError:
  HAVE_IPV4 = False

try:
  GetDefaultRoute(version=6)
  HAVE_IPV6 = True
except ValueError:
  HAVE_IPV6 = False


class RunAsUid(object):

  """Context guard to run a code block as a given UID."""

  def __init__(self, uid):
    self.uid = uid

  def __enter__(self):
    if self.uid:
      self.saved_uid = os.geteuid()
      self.saved_groups = os.getgroups()
      if self.uid:
        os.setgroups(self.saved_groups + [AID_INET])
        os.seteuid(self.uid)

  def __exit__(self, unused_type, unused_value, unused_traceback):
    if self.uid:
      os.seteuid(self.saved_uid)
      os.setgroups(self.saved_groups)


class NetworkTest(unittest.TestCase):

  def assertRaisesErrno(self, err_num, f, *args):
    msg = os.strerror(err_num)
    self.assertRaisesRegexp(EnvironmentError, msg, f, *args)


if __name__ == "__main__":
  unittest.main()
