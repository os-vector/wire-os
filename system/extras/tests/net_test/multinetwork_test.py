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

import errno
import os
import random
from socket import *  # pylint: disable=wildcard-import
import struct
import time           # pylint: disable=unused-import
import unittest

from scapy import all as scapy

import iproute
import multinetwork_base
import net_test

PING_IDENT = 0xff19
PING_PAYLOAD = "foobarbaz"
PING_SEQ = 3
PING_TOS = 0x83

IPV6_FLOWINFO = 11


UDP_PAYLOAD = str(scapy.DNS(rd=1,
                            id=random.randint(0, 65535),
                            qd=scapy.DNSQR(qname="wWW.GoOGle.CoM",
                                           qtype="AAAA")))


IPV4_MARK_REFLECT_SYSCTL = "/proc/sys/net/ipv4/fwmark_reflect"
IPV6_MARK_REFLECT_SYSCTL = "/proc/sys/net/ipv6/fwmark_reflect"
SYNCOOKIES_SYSCTL = "/proc/sys/net/ipv4/tcp_syncookies"
TCP_MARK_ACCEPT_SYSCTL = "/proc/sys/net/ipv4/tcp_fwmark_accept"

HAVE_MARK_REFLECT = os.path.isfile(IPV4_MARK_REFLECT_SYSCTL)
HAVE_TCP_MARK_ACCEPT = os.path.isfile(TCP_MARK_ACCEPT_SYSCTL)

# The IP[V6]UNICAST_IF socket option was added between 3.1 and 3.4.
HAVE_UNICAST_IF = net_test.LINUX_VERSION >= (3, 4, 0)


class ConfigurationError(AssertionError):
  pass


class Packets(object):

  TCP_FIN = 1
  TCP_SYN = 2
  TCP_RST = 4
  TCP_PSH = 8
  TCP_ACK = 16

  TCP_SEQ = 1692871236
  TCP_WINDOW = 14400

  @staticmethod
  def RandomPort():
    return random.randint(1025, 65535)

  @staticmethod
  def _GetIpLayer(version):
    return {4: scapy.IP, 6: scapy.IPv6}[version]

  @staticmethod
  def _SetPacketTos(packet, tos):
    if isinstance(packet, scapy.IPv6):
      packet.tc = tos
    elif isinstance(packet, scapy.IP):
      packet.tos = tos
    else:
      raise ValueError("Can't find ToS Field")

  @classmethod
  def UDP(cls, version, srcaddr, dstaddr, sport=0):
    ip = cls._GetIpLayer(version)
    # Can't just use "if sport" because None has meaning (it means unspecified).
    if sport == 0:
      sport = cls.RandomPort()
    return ("UDPv%d packet" % version,
            ip(src=srcaddr, dst=dstaddr) /
            scapy.UDP(sport=sport, dport=53) / UDP_PAYLOAD)

  @classmethod
  def UDPWithOptions(cls, version, srcaddr, dstaddr, sport=0):
    if version == 4:
      packet = (scapy.IP(src=srcaddr, dst=dstaddr, ttl=39, tos=0x83) /
                scapy.UDP(sport=sport, dport=53) /
                UDP_PAYLOAD)
    else:
      packet = (scapy.IPv6(src=srcaddr, dst=dstaddr,
                           fl=0xbeef, hlim=39, tc=0x83) /
                scapy.UDP(sport=sport, dport=53) /
                UDP_PAYLOAD)
    return ("UDPv%d packet with options" % version, packet)

  @classmethod
  def SYN(cls, dport, version, srcaddr, dstaddr, sport=0, seq=TCP_SEQ):
    ip = cls._GetIpLayer(version)
    if sport == 0:
      sport = cls.RandomPort()
    return ("TCP SYN",
            ip(src=srcaddr, dst=dstaddr) /
            scapy.TCP(sport=sport, dport=dport,
                      seq=seq, ack=0,
                      flags=cls.TCP_SYN, window=cls.TCP_WINDOW))

  @classmethod
  def RST(cls, version, srcaddr, dstaddr, packet):
    ip = cls._GetIpLayer(version)
    original = packet.getlayer("TCP")
    return ("TCP RST",
            ip(src=srcaddr, dst=dstaddr) /
            scapy.TCP(sport=original.dport, dport=original.sport,
                      ack=original.seq + 1, seq=None,
                      flags=cls.TCP_RST | cls.TCP_ACK, window=cls.TCP_WINDOW))

  @classmethod
  def SYNACK(cls, version, srcaddr, dstaddr, packet):
    ip = cls._GetIpLayer(version)
    original = packet.getlayer("TCP")
    return ("TCP SYN+ACK",
            ip(src=srcaddr, dst=dstaddr) /
            scapy.TCP(sport=original.dport, dport=original.sport,
                      ack=original.seq + 1, seq=None,
                      flags=cls.TCP_SYN | cls.TCP_ACK, window=None))

  @classmethod
  def ACK(cls, version, srcaddr, dstaddr, packet, payload=""):
    ip = cls._GetIpLayer(version)
    original = packet.getlayer("TCP")
    was_syn_or_fin = (original.flags & (cls.TCP_SYN | cls.TCP_FIN)) != 0
    ack_delta = was_syn_or_fin + len(original.payload)
    desc = "TCP data" if payload else "TCP ACK"
    flags = cls.TCP_ACK | cls.TCP_PSH if payload else cls.TCP_ACK
    return (desc,
            ip(src=srcaddr, dst=dstaddr) /
            scapy.TCP(sport=original.dport, dport=original.sport,
                      ack=original.seq + ack_delta, seq=original.ack,
                      flags=flags, window=cls.TCP_WINDOW) /
            payload)

  @classmethod
  def FIN(cls, version, srcaddr, dstaddr, packet):
    ip = cls._GetIpLayer(version)
    original = packet.getlayer("TCP")
    was_fin = (original.flags & cls.TCP_FIN) != 0
    return ("TCP FIN",
            ip(src=srcaddr, dst=dstaddr) /
            scapy.TCP(sport=original.dport, dport=original.sport,
                      ack=original.seq + was_fin, seq=original.ack,
                      flags=cls.TCP_ACK | cls.TCP_FIN, window=cls.TCP_WINDOW))

  @classmethod
  def GRE(cls, version, srcaddr, dstaddr, proto, packet):
    if version == 4:
      ip = scapy.IP(src=srcaddr, dst=dstaddr, proto=net_test.IPPROTO_GRE)
    else:
      ip = scapy.IPv6(src=srcaddr, dst=dstaddr, nh=net_test.IPPROTO_GRE)
    packet = ip / scapy.GRE(proto=proto) / packet
    return ("GRE packet", packet)

  @classmethod
  def ICMPPortUnreachable(cls, version, srcaddr, dstaddr, packet):
    if version == 4:
      # Linux hardcodes the ToS on ICMP errors to 0xc0 or greater because of
      # RFC 1812 4.3.2.5 (!).
      return ("ICMPv4 port unreachable",
              scapy.IP(src=srcaddr, dst=dstaddr, proto=1, tos=0xc0) /
              scapy.ICMPerror(type=3, code=3) / packet)
    else:
      return ("ICMPv6 port unreachable",
              scapy.IPv6(src=srcaddr, dst=dstaddr) /
              scapy.ICMPv6DestUnreach(code=4) / packet)

  @classmethod
  def ICMPPacketTooBig(cls, version, srcaddr, dstaddr, packet):
    if version == 4:
      return ("ICMPv4 fragmentation needed",
              scapy.IP(src=srcaddr, dst=dstaddr, proto=1) /
              scapy.ICMPerror(type=3, code=4, unused=1280) / str(packet)[:64])
    else:
      udp = packet.getlayer("UDP")
      udp.payload = str(udp.payload)[:1280-40-8]
      return ("ICMPv6 Packet Too Big",
              scapy.IPv6(src=srcaddr, dst=dstaddr) /
              scapy.ICMPv6PacketTooBig() / str(packet)[:1232])

  @classmethod
  def ICMPEcho(cls, version, srcaddr, dstaddr):
    ip = cls._GetIpLayer(version)
    icmp = {4: scapy.ICMP, 6: scapy.ICMPv6EchoRequest}[version]
    packet = (ip(src=srcaddr, dst=dstaddr) /
              icmp(id=PING_IDENT, seq=PING_SEQ) / PING_PAYLOAD)
    cls._SetPacketTos(packet, PING_TOS)
    return ("ICMPv%d echo" % version, packet)

  @classmethod
  def ICMPReply(cls, version, srcaddr, dstaddr, packet):
    ip = cls._GetIpLayer(version)
    # Scapy doesn't provide an ICMP echo reply constructor.
    icmpv4_reply = lambda **kwargs: scapy.ICMP(type=0, **kwargs)
    icmp = {4: icmpv4_reply, 6: scapy.ICMPv6EchoReply}[version]
    packet = (ip(src=srcaddr, dst=dstaddr) /
              icmp(id=PING_IDENT, seq=PING_SEQ) / PING_PAYLOAD)
    # IPv6 only started copying the tclass to echo replies in 3.14.
    if version == 4 or net_test.LINUX_VERSION >= (3, 14):
      cls._SetPacketTos(packet, PING_TOS)
    return ("ICMPv%d echo reply" % version, packet)

  @classmethod
  def NS(cls, srcaddr, tgtaddr, srcmac):
    solicited = inet_pton(AF_INET6, tgtaddr)
    last3bytes = tuple([ord(b) for b in solicited[-3:]])
    solicited = "ff02::1:ff%02x:%02x%02x" % last3bytes
    packet = (scapy.IPv6(src=srcaddr, dst=solicited) /
              scapy.ICMPv6ND_NS(tgt=tgtaddr) /
              scapy.ICMPv6NDOptSrcLLAddr(lladdr=srcmac))
    return ("ICMPv6 NS", packet)

  @classmethod
  def NA(cls, srcaddr, dstaddr, srcmac):
    packet = (scapy.IPv6(src=srcaddr, dst=dstaddr) /
              scapy.ICMPv6ND_NA(tgt=srcaddr, R=0, S=1, O=1) /
              scapy.ICMPv6NDOptDstLLAddr(lladdr=srcmac))
    return ("ICMPv6 NA", packet)


class InboundMarkingTest(multinetwork_base.MultiNetworkBaseTest):

  @classmethod
  def _SetInboundMarking(cls, netid, is_add):
    for version in [4, 6]:
      # Run iptables to set up incoming packet marking.
      iface = cls.GetInterfaceName(netid)
      add_del = "-A" if is_add else "-D"
      iptables = {4: "iptables", 6: "ip6tables"}[version]
      args = "%s %s INPUT -t mangle -i %s -j MARK --set-mark %d" % (
          iptables, add_del, iface, netid)
      iptables = "/sbin/" + iptables
      ret = os.spawnvp(os.P_WAIT, iptables, args.split(" "))
      if ret:
        raise ConfigurationError("Setup command failed: %s" % args)

  @classmethod
  def setUpClass(cls):
    super(InboundMarkingTest, cls).setUpClass()
    for netid in cls.tuns:
      cls._SetInboundMarking(netid, True)

  @classmethod
  def tearDownClass(cls):
    for netid in cls.tuns:
      cls._SetInboundMarking(netid, False)
    super(InboundMarkingTest, cls).tearDownClass()

  @classmethod
  def SetMarkReflectSysctls(cls, value):
    cls.SetSysctl(IPV4_MARK_REFLECT_SYSCTL, value)
    try:
      cls.SetSysctl(IPV6_MARK_REFLECT_SYSCTL, value)
    except IOError:
      # This does not exist if we use the version of the patch that uses a
      # common sysctl for IPv4 and IPv6.
      pass


class OutgoingTest(multinetwork_base.MultiNetworkBaseTest):

  # How many times to run outgoing packet tests.
  ITERATIONS = 5

  def CheckPingPacket(self, version, netid, routing_mode, dstaddr, packet):
    s = self.BuildSocket(version, net_test.PingSocket, netid, routing_mode)

    myaddr = self.MyAddress(version, netid)
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s.bind((myaddr, PING_IDENT))
    net_test.SetSocketTos(s, PING_TOS)

    desc, expected = Packets.ICMPEcho(version, myaddr, dstaddr)
    msg = "IPv%d ping: expected %s on %s" % (
        version, desc, self.GetInterfaceName(netid))

    s.sendto(packet + PING_PAYLOAD, (dstaddr, 19321))

    self.ExpectPacketOn(netid, msg, expected)

  def CheckTCPSYNPacket(self, version, netid, routing_mode, dstaddr):
    s = self.BuildSocket(version, net_test.TCPSocket, netid, routing_mode)

    if version == 6 and dstaddr.startswith("::ffff"):
      version = 4
    myaddr = self.MyAddress(version, netid)
    desc, expected = Packets.SYN(53, version, myaddr, dstaddr,
                                 sport=None, seq=None)

    # Non-blocking TCP connects always return EINPROGRESS.
    self.assertRaisesErrno(errno.EINPROGRESS, s.connect, (dstaddr, 53))
    msg = "IPv%s TCP connect: expected %s on %s" % (
        version, desc, self.GetInterfaceName(netid))
    self.ExpectPacketOn(netid, msg, expected)
    s.close()

  def CheckUDPPacket(self, version, netid, routing_mode, dstaddr):
    s = self.BuildSocket(version, net_test.UDPSocket, netid, routing_mode)

    if version == 6 and dstaddr.startswith("::ffff"):
      version = 4
    myaddr = self.MyAddress(version, netid)
    desc, expected = Packets.UDP(version, myaddr, dstaddr, sport=None)
    msg = "IPv%s UDP %%s: expected %s on %s" % (
        version, desc, self.GetInterfaceName(netid))

    s.sendto(UDP_PAYLOAD, (dstaddr, 53))
    self.ExpectPacketOn(netid, msg % "sendto", expected)

    # IP_UNICAST_IF doesn't seem to work on connected sockets, so no TCP.
    if routing_mode != "ucast_oif":
      s.connect((dstaddr, 53))
      s.send(UDP_PAYLOAD)
      self.ExpectPacketOn(netid, msg % "connect/send", expected)
      s.close()

  def CheckRawGrePacket(self, version, netid, routing_mode, dstaddr):
    s = self.BuildSocket(version, net_test.RawGRESocket, netid, routing_mode)

    inner_version = {4: 6, 6: 4}[version]
    inner_src = self.MyAddress(inner_version, netid)
    inner_dst = self.GetRemoteAddress(inner_version)
    inner = str(Packets.UDP(inner_version, inner_src, inner_dst, sport=None)[1])

    ethertype = {4: net_test.ETH_P_IP, 6: net_test.ETH_P_IPV6}[inner_version]
    # A GRE header can be as simple as two zero bytes and the ethertype.
    packet = struct.pack("!i", ethertype) + inner
    myaddr = self.MyAddress(version, netid)

    s.sendto(packet, (dstaddr, IPPROTO_GRE))
    desc, expected = Packets.GRE(version, myaddr, dstaddr, ethertype, inner)
    msg = "Raw IPv%d GRE with inner IPv%d UDP: expected %s on %s" % (
        version, inner_version, desc, self.GetInterfaceName(netid))
    self.ExpectPacketOn(netid, msg, expected)

  def CheckOutgoingPackets(self, routing_mode):
    v4addr = self.IPV4_ADDR
    v6addr = self.IPV6_ADDR
    v4mapped = "::ffff:" + v4addr

    for _ in xrange(self.ITERATIONS):
      for netid in self.tuns:

        self.CheckPingPacket(4, netid, routing_mode, v4addr, self.IPV4_PING)
        # Kernel bug.
        if routing_mode != "oif":
          self.CheckPingPacket(6, netid, routing_mode, v6addr, self.IPV6_PING)

        # IP_UNICAST_IF doesn't seem to work on connected sockets, so no TCP.
        if routing_mode != "ucast_oif":
          self.CheckTCPSYNPacket(4, netid, routing_mode, v4addr)
          self.CheckTCPSYNPacket(6, netid, routing_mode, v6addr)
          self.CheckTCPSYNPacket(6, netid, routing_mode, v4mapped)

        self.CheckUDPPacket(4, netid, routing_mode, v4addr)
        self.CheckUDPPacket(6, netid, routing_mode, v6addr)
        self.CheckUDPPacket(6, netid, routing_mode, v4mapped)

        # Creating raw sockets on non-root UIDs requires properly setting
        # capabilities, which is hard to do from Python.
        # IP_UNICAST_IF is not supported on raw sockets.
        if routing_mode not in ["uid", "ucast_oif"]:
          self.CheckRawGrePacket(4, netid, routing_mode, v4addr)
          self.CheckRawGrePacket(6, netid, routing_mode, v6addr)

  def testMarkRouting(self):
    """Checks that socket marking selects the right outgoing interface."""
    self.CheckOutgoingPackets("mark")

  @unittest.skipUnless(multinetwork_base.HAVE_UID_ROUTING, "no UID routes")
  def testUidRouting(self):
    """Checks that UID routing selects the right outgoing interface."""
    self.CheckOutgoingPackets("uid")

  def testOifRouting(self):
    """Checks that oif routing selects the right outgoing interface."""
    self.CheckOutgoingPackets("oif")

  @unittest.skipUnless(HAVE_UNICAST_IF, "no support for UNICAST_IF")
  def testUcastOifRouting(self):
    """Checks that ucast oif routing selects the right outgoing interface."""
    self.CheckOutgoingPackets("ucast_oif")

  def CheckRemarking(self, version, use_connect):
    # Remarking or resetting UNICAST_IF on connected sockets does not work.
    if use_connect:
      modes = ["oif"]
    else:
      modes = ["mark", "oif"]
      if HAVE_UNICAST_IF:
        modes += ["ucast_oif"]

    for mode in modes:
      s = net_test.UDPSocket(self.GetProtocolFamily(version))

      # Figure out what packets to expect.
      unspec = {4: "0.0.0.0", 6: "::"}[version]
      sport = Packets.RandomPort()
      s.bind((unspec, sport))
      dstaddr = {4: self.IPV4_ADDR, 6: self.IPV6_ADDR}[version]
      desc, expected = Packets.UDP(version, unspec, dstaddr, sport)

      # If we're testing connected sockets, connect the socket on the first
      # netid now.
      if use_connect:
        netid = self.tuns.keys()[0]
        self.SelectInterface(s, netid, mode)
        s.connect((dstaddr, 53))
        expected.src = self.MyAddress(version, netid)

      # For each netid, select that network without closing the socket, and
      # check that the packets sent on that socket go out on the right network.
      for netid in self.tuns:
        self.SelectInterface(s, netid, mode)
        if not use_connect:
          expected.src = self.MyAddress(version, netid)
        s.sendto(UDP_PAYLOAD, (dstaddr, 53))
        connected_str = "Connected" if use_connect else "Unconnected"
        msg = "%s UDPv%d socket remarked using %s: expecting %s on %s" % (
            connected_str, version, mode, desc, self.GetInterfaceName(netid))
        self.ExpectPacketOn(netid, msg, expected)
        self.SelectInterface(s, None, mode)

  def testIPv4Remarking(self):
    """Checks that updating the mark on an IPv4 socket changes routing."""
    self.CheckRemarking(4, False)
    self.CheckRemarking(4, True)

  def testIPv6Remarking(self):
    """Checks that updating the mark on an IPv6 socket changes routing."""
    self.CheckRemarking(6, False)
    self.CheckRemarking(6, True)

  def testIPv6StickyPktinfo(self):
    for _ in xrange(self.ITERATIONS):
      for netid in self.tuns:
        s = net_test.UDPSocket(AF_INET6)

        # Set a flowlabel.
        net_test.SetFlowLabel(s, net_test.IPV6_ADDR, 0xdead)
        s.setsockopt(net_test.SOL_IPV6, net_test.IPV6_FLOWINFO_SEND, 1)

        # Set some destination options.
        nonce = "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c"
        dstopts = "".join([
            "\x11\x02",              # Next header=UDP, 24 bytes of options.
            "\x01\x06", "\x00" * 6,  # PadN, 6 bytes of padding.
            "\x8b\x0c",              # ILNP nonce, 12 bytes.
            nonce
        ])
        s.setsockopt(net_test.SOL_IPV6, IPV6_DSTOPTS, dstopts)
        s.setsockopt(net_test.SOL_IPV6, IPV6_UNICAST_HOPS, 255)

        pktinfo = multinetwork_base.MakePktInfo(6, None, self.ifindices[netid])

        # Set the sticky pktinfo option.
        s.setsockopt(net_test.SOL_IPV6, IPV6_PKTINFO, pktinfo)

        # Specify the flowlabel in the destination address.
        s.sendto(UDP_PAYLOAD, (net_test.IPV6_ADDR, 53, 0xdead, 0))

        sport = s.getsockname()[1]
        srcaddr = self.MyAddress(6, netid)
        expected = (scapy.IPv6(src=srcaddr, dst=net_test.IPV6_ADDR,
                               fl=0xdead, hlim=255) /
                    scapy.IPv6ExtHdrDestOpt(
                        options=[scapy.PadN(optdata="\x00\x00\x00\x00\x00\x00"),
                                 scapy.HBHOptUnknown(otype=0x8b,
                                                     optdata=nonce)]) /
                    scapy.UDP(sport=sport, dport=53) /
                    UDP_PAYLOAD)
        msg = "IPv6 UDP using sticky pktinfo: expected UDP packet on %s" % (
            self.GetInterfaceName(netid))
        self.ExpectPacketOn(netid, msg, expected)

  def CheckPktinfoRouting(self, version):
    for _ in xrange(self.ITERATIONS):
      for netid in self.tuns:
        family = self.GetProtocolFamily(version)
        s = net_test.UDPSocket(family)

        if version == 6:
          # Create a flowlabel so we can use it.
          net_test.SetFlowLabel(s, net_test.IPV6_ADDR, 0xbeef)

          # Specify some arbitrary options.
          cmsgs = [
              (net_test.SOL_IPV6, IPV6_HOPLIMIT, 39),
              (net_test.SOL_IPV6, IPV6_TCLASS, 0x83),
              (net_test.SOL_IPV6, IPV6_FLOWINFO, int(htonl(0xbeef))),
          ]
        else:
          # Support for setting IPv4 TOS and TTL via cmsg only appeared in 3.13.
          cmsgs = []
          s.setsockopt(net_test.SOL_IP, IP_TTL, 39)
          s.setsockopt(net_test.SOL_IP, IP_TOS, 0x83)

        dstaddr = self.GetRemoteAddress(version)
        self.SendOnNetid(version, s, dstaddr, 53, netid, UDP_PAYLOAD, cmsgs)

        sport = s.getsockname()[1]
        srcaddr = self.MyAddress(version, netid)

        desc, expected = Packets.UDPWithOptions(version, srcaddr, dstaddr,
                                                sport=sport)

        msg = "IPv%d UDP using pktinfo routing: expected %s on %s" % (
            version, desc, self.GetInterfaceName(netid))
        self.ExpectPacketOn(netid, msg, expected)

  def testIPv4PktinfoRouting(self):
    self.CheckPktinfoRouting(4)

  def testIPv6PktinfoRouting(self):
    self.CheckPktinfoRouting(6)


class MarkTest(InboundMarkingTest):

  def CheckReflection(self, version, gen_packet, gen_reply):
    """Checks that replies go out on the same interface as the original.

    For each combination:
     - Calls gen_packet to generate a packet to that IP address.
     - Writes the packet generated by gen_packet on the given tun
       interface, causing the kernel to receive it.
     - Checks that the kernel's reply matches the packet generated by
       gen_reply.

    Args:
      version: An integer, 4 or 6.
      gen_packet: A function taking an IP version (an integer), a source
        address and a destination address (strings), and returning a scapy
        packet.
      gen_reply: A function taking the same arguments as gen_packet,
        plus a scapy packet, and returning a scapy packet.
    """
    for netid, iif, ip_if, myaddr, remoteaddr in self.Combinations(version):
      # Generate a test packet.
      desc, packet = gen_packet(version, remoteaddr, myaddr)

      # Test with mark reflection enabled and disabled.
      for reflect in [0, 1]:
        self.SetMarkReflectSysctls(reflect)
        # HACK: IPv6 ping replies always do a routing lookup with the
        # interface the ping came in on. So even if mark reflection is not
        # working, IPv6 ping replies will be properly reflected. Don't
        # fail when that happens.
        if reflect or desc == "ICMPv6 echo":
          reply_desc, reply = gen_reply(version, myaddr, remoteaddr, packet)
        else:
          reply_desc, reply = None, None

        msg = self._FormatMessage(iif, ip_if, "reflect=%d" % reflect,
                                  desc, reply_desc)
        self._ReceiveAndExpectResponse(netid, packet, reply, msg)

  def SYNToClosedPort(self, *args):
    return Packets.SYN(999, *args)

  @unittest.skipUnless(HAVE_MARK_REFLECT, "no mark reflection")
  def testIPv4ICMPErrorsReflectMark(self):
    self.CheckReflection(4, Packets.UDP, Packets.ICMPPortUnreachable)

  @unittest.skipUnless(HAVE_MARK_REFLECT, "no mark reflection")
  def testIPv6ICMPErrorsReflectMark(self):
    self.CheckReflection(6, Packets.UDP, Packets.ICMPPortUnreachable)

  @unittest.skipUnless(HAVE_MARK_REFLECT, "no mark reflection")
  def testIPv4PingRepliesReflectMarkAndTos(self):
    self.CheckReflection(4, Packets.ICMPEcho, Packets.ICMPReply)

  @unittest.skipUnless(HAVE_MARK_REFLECT, "no mark reflection")
  def testIPv6PingRepliesReflectMarkAndTos(self):
    self.CheckReflection(6, Packets.ICMPEcho, Packets.ICMPReply)

  @unittest.skipUnless(HAVE_MARK_REFLECT, "no mark reflection")
  def testIPv4RSTsReflectMark(self):
    self.CheckReflection(4, self.SYNToClosedPort, Packets.RST)

  @unittest.skipUnless(HAVE_MARK_REFLECT, "no mark reflection")
  def testIPv6RSTsReflectMark(self):
    self.CheckReflection(6, self.SYNToClosedPort, Packets.RST)


class TCPAcceptTest(InboundMarkingTest):

  MODE_BINDTODEVICE = "SO_BINDTODEVICE"
  MODE_INCOMING_MARK = "incoming mark"
  MODE_EXPLICIT_MARK = "explicit mark"
  MODE_UID = "uid"

  @classmethod
  def setUpClass(cls):
    super(TCPAcceptTest, cls).setUpClass()

    # Open a port so we can observe SYN+ACKs. Since it's a dual-stack socket it
    # will accept both IPv4 and IPv6 connections. We do this here instead of in
    # each test so we can use the same socket every time. That way, if a kernel
    # bug causes incoming packets to mark the listening socket instead of the
    # accepted socket, the test will fail as soon as the next address/interface
    # combination is tried.
    cls.listenport = 1234
    cls.listensocket = net_test.IPv6TCPSocket()
    cls.listensocket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    cls.listensocket.bind(("::", cls.listenport))
    cls.listensocket.listen(100)

  def BounceSocket(self, s):
    """Attempts to invalidate a socket's destination cache entry."""
    if s.family == AF_INET:
      tos = s.getsockopt(SOL_IP, IP_TOS)
      s.setsockopt(net_test.SOL_IP, IP_TOS, 53)
      s.setsockopt(net_test.SOL_IP, IP_TOS, tos)
    else:
      # UDP, 8 bytes dstopts; PAD1, 4 bytes padding; 4 bytes zeros.
      pad8 = "".join(["\x11\x00", "\x01\x04", "\x00" * 4])
      s.setsockopt(net_test.SOL_IPV6, IPV6_DSTOPTS, pad8)
      s.setsockopt(net_test.SOL_IPV6, IPV6_DSTOPTS, "")

  def _SetTCPMarkAcceptSysctl(self, value):
    self.SetSysctl(TCP_MARK_ACCEPT_SYSCTL, value)

  def CheckTCPConnection(self, mode, listensocket, netid, version,
                         myaddr, remoteaddr, packet, reply, msg):
    establishing_ack = Packets.ACK(version, remoteaddr, myaddr, reply)[1]

    # Attempt to confuse the kernel.
    self.BounceSocket(listensocket)

    self.ReceivePacketOn(netid, establishing_ack)

    # If we're using UID routing, the accept() call has to be run as a UID that
    # is routed to the specified netid, because the UID of the socket returned
    # by accept() is the effective UID of the process that calls it. It doesn't
    # need to be the same UID; any UID that selects the same interface will do.
    with net_test.RunAsUid(self.UidForNetid(netid)):
      s, _ = listensocket.accept()

    try:
      # Check that data sent on the connection goes out on the right interface.
      desc, data = Packets.ACK(version, myaddr, remoteaddr, establishing_ack,
                               payload=UDP_PAYLOAD)
      s.send(UDP_PAYLOAD)
      self.ExpectPacketOn(netid, msg + ": expecting %s" % desc, data)
      self.BounceSocket(s)

      # Keep up our end of the conversation.
      ack = Packets.ACK(version, remoteaddr, myaddr, data)[1]
      self.BounceSocket(listensocket)
      self.ReceivePacketOn(netid, ack)

      mark = self.GetSocketMark(s)
    finally:
      self.BounceSocket(s)
      s.close()

    if mode == self.MODE_INCOMING_MARK:
      self.assertEquals(netid, mark,
                        msg + ": Accepted socket: Expected mark %d, got %d" % (
                            netid, mark))
    elif mode != self.MODE_EXPLICIT_MARK:
      self.assertEquals(0, self.GetSocketMark(listensocket))

    # Check the FIN was sent on the right interface, and ack it. We don't expect
    # this to fail because by the time the connection is established things are
    # likely working, but a) extra tests are always good and b) extra packets
    # like the FIN (and retransmitted FINs) could cause later tests that expect
    # no packets to fail.
    desc, fin = Packets.FIN(version, myaddr, remoteaddr, ack)
    self.ExpectPacketOn(netid, msg + ": expecting %s after close" % desc, fin)

    desc, finack = Packets.FIN(version, remoteaddr, myaddr, fin)
    self.ReceivePacketOn(netid, finack)

    # Since we called close() earlier, the userspace socket object is gone, so
    # the socket has no UID. If we're doing UID routing, the ack might be routed
    # incorrectly. Not much we can do here.
    desc, finackack = Packets.ACK(version, myaddr, remoteaddr, finack)
    if mode != self.MODE_UID:
      self.ExpectPacketOn(netid, msg + ": expecting final ack", finackack)
    else:
      self.ClearTunQueues()

  def CheckTCP(self, version, modes):
    """Checks that incoming TCP connections work.

    Args:
      version: An integer, 4 or 6.
      modes: A list of modes to excercise.
    """
    for syncookies in [0, 2]:
      for mode in modes:
        for netid, iif, ip_if, myaddr, remoteaddr in self.Combinations(version):
          if mode == self.MODE_UID:
            listensocket = self.BuildSocket(6, net_test.TCPSocket, netid, mode)
            listensocket.listen(100)
          else:
            listensocket = self.listensocket

          listenport = listensocket.getsockname()[1]

          if HAVE_TCP_MARK_ACCEPT:
            accept_sysctl = 1 if mode == self.MODE_INCOMING_MARK else 0
            self._SetTCPMarkAcceptSysctl(accept_sysctl)

          bound_dev = iif if mode == self.MODE_BINDTODEVICE else None
          self.BindToDevice(listensocket, bound_dev)

          mark = netid if mode == self.MODE_EXPLICIT_MARK else 0
          self.SetSocketMark(listensocket, mark)

          # Generate the packet here instead of in the outer loop, so
          # subsequent TCP connections use different source ports and
          # retransmissions from old connections don't confuse subsequent
          # tests.
          desc, packet = Packets.SYN(listenport, version, remoteaddr, myaddr)

          if mode:
            reply_desc, reply = Packets.SYNACK(version, myaddr, remoteaddr,
                                               packet)
          else:
            reply_desc, reply = None, None

          extra = "mode=%s, syncookies=%d" % (mode, syncookies)
          msg = self._FormatMessage(iif, ip_if, extra, desc, reply_desc)
          reply = self._ReceiveAndExpectResponse(netid, packet, reply, msg)
          if reply:
            self.CheckTCPConnection(mode, listensocket, netid, version, myaddr,
                                    remoteaddr, packet, reply, msg)

  def testBasicTCP(self):
    self.CheckTCP(4, [None, self.MODE_BINDTODEVICE, self.MODE_EXPLICIT_MARK])
    self.CheckTCP(6, [None, self.MODE_BINDTODEVICE, self.MODE_EXPLICIT_MARK])

  @unittest.skipUnless(HAVE_TCP_MARK_ACCEPT, "fwmark writeback not supported")
  def testIPv4MarkAccept(self):
    self.CheckTCP(4, [self.MODE_INCOMING_MARK])

  @unittest.skipUnless(HAVE_TCP_MARK_ACCEPT, "fwmark writeback not supported")
  def testIPv6MarkAccept(self):
    self.CheckTCP(6, [self.MODE_INCOMING_MARK])

  @unittest.skipUnless(multinetwork_base.HAVE_UID_ROUTING, "no UID routes")
  def testIPv4UidAccept(self):
    self.CheckTCP(4, [self.MODE_UID])

  @unittest.skipUnless(multinetwork_base.HAVE_UID_ROUTING, "no UID routes")
  def testIPv6UidAccept(self):
    self.CheckTCP(6, [self.MODE_UID])

  def testIPv6ExplicitMark(self):
    self.CheckTCP(6, [self.MODE_EXPLICIT_MARK])


class RATest(multinetwork_base.MultiNetworkBaseTest):

  def testDoesNotHaveObsoleteSysctl(self):
    self.assertFalse(os.path.isfile(
        "/proc/sys/net/ipv6/route/autoconf_table_offset"))

  @unittest.skipUnless(multinetwork_base.HAVE_AUTOCONF_TABLE,
                       "no support for per-table autoconf")
  def testPurgeDefaultRouters(self):

    def CheckIPv6Connectivity(expect_connectivity):
      for netid in self.NETIDS:
        s = net_test.UDPSocket(AF_INET6)
        self.SetSocketMark(s, netid)
        if expect_connectivity:
          self.assertTrue(s.sendto(UDP_PAYLOAD, (net_test.IPV6_ADDR, 1234)))
        else:
          self.assertRaisesErrno(errno.ENETUNREACH, s.sendto, UDP_PAYLOAD,
                                 (net_test.IPV6_ADDR, 1234))

    try:
      CheckIPv6Connectivity(True)
      self.SetSysctl("/proc/sys/net/ipv6/conf/all/forwarding", 1)
      CheckIPv6Connectivity(False)
    finally:
      self.SetSysctl("/proc/sys/net/ipv6/conf/all/forwarding", 0)
      for netid in self.NETIDS:
        self.SendRA(netid)
      CheckIPv6Connectivity(True)

  def testOnlinkCommunication(self):
    """Checks that on-link communication goes direct and not through routers."""
    for netid in self.tuns:
      # Send a UDP packet to a random on-link destination.
      s = net_test.UDPSocket(AF_INET6)
      iface = self.GetInterfaceName(netid)
      self.BindToDevice(s, iface)
      # dstaddr can never be our address because GetRandomDestination only fills
      # in the lower 32 bits, but our address has 0xff in the byte before that
      # (since it's constructed from the EUI-64 and so has ff:fe in the middle).
      dstaddr = self.GetRandomDestination(self.IPv6Prefix(netid))
      s.sendto(UDP_PAYLOAD, (dstaddr, 53))

      # Expect an NS for that destination on the interface.
      myaddr = self.MyAddress(6, netid)
      mymac = self.MyMacAddress(netid)
      desc, expected = Packets.NS(myaddr, dstaddr, mymac)
      msg = "Sending UDP packet to on-link destination: expecting %s" % desc
      time.sleep(0.0001)  # Required to make the test work on kernel 3.1(!)
      self.ExpectPacketOn(netid, msg, expected)

      # Send an NA.
      tgtmac = "02:00:00:00:%02x:99" % netid
      _, reply = Packets.NA(dstaddr, myaddr, tgtmac)
      # Don't use ReceivePacketOn, since that uses the router's MAC address as
      # the source. Instead, construct our own Ethernet header with source
      # MAC of tgtmac.
      reply = scapy.Ether(src=tgtmac, dst=mymac) / reply
      self.ReceiveEtherPacketOn(netid, reply)

      # Expect the kernel to send the original UDP packet now that the ND cache
      # entry has been populated.
      sport = s.getsockname()[1]
      desc, expected = Packets.UDP(6, myaddr, dstaddr, sport=sport)
      msg = "After NA response, expecting %s" % desc
      self.ExpectPacketOn(netid, msg, expected)

  # This test documents a known issue: routing tables are never deleted.
  @unittest.skipUnless(multinetwork_base.HAVE_AUTOCONF_TABLE,
                       "no support for per-table autoconf")
  def testLeftoverRoutes(self):
    def GetNumRoutes():
      return len(open("/proc/net/ipv6_route").readlines())

    num_routes = GetNumRoutes()
    for i in xrange(10, 20):
      try:
        self.tuns[i] = self.CreateTunInterface(i)
        self.SendRA(i)
        self.tuns[i].close()
      finally:
        del self.tuns[i]
    self.assertLess(num_routes, GetNumRoutes())


class PMTUTest(InboundMarkingTest):

  PAYLOAD_SIZE = 1400

  # Socket options to change PMTU behaviour.
  IP_MTU_DISCOVER = 10
  IP_PMTUDISC_DO = 1
  IPV6_DONTFRAG = 62

  # Socket options to get the MTU.
  IP_MTU = 14
  IPV6_PATHMTU = 61

  def GetSocketMTU(self, version, s):
    if version == 6:
      ip6_mtuinfo = s.getsockopt(net_test.SOL_IPV6, self.IPV6_PATHMTU, 32)
      unused_sockaddr, mtu = struct.unpack("=28sI", ip6_mtuinfo)
      return mtu
    else:
      return s.getsockopt(net_test.SOL_IP, self.IP_MTU)

  def DisableFragmentationAndReportErrors(self, version, s):
    if version == 4:
      s.setsockopt(net_test.SOL_IP, self.IP_MTU_DISCOVER, self.IP_PMTUDISC_DO)
      s.setsockopt(net_test.SOL_IP, net_test.IP_RECVERR, 1)
    else:
      s.setsockopt(net_test.SOL_IPV6, self.IPV6_DONTFRAG, 1)
      s.setsockopt(net_test.SOL_IPV6, net_test.IPV6_RECVERR, 1)

  def CheckPMTU(self, version, use_connect, modes):

    def SendBigPacket(version, s, dstaddr, netid, payload):
      if use_connect:
        s.send(payload)
      else:
        self.SendOnNetid(version, s, dstaddr, 1234, netid, payload, [])

    for netid in self.tuns:
      for mode in modes:
        s = self.BuildSocket(version, net_test.UDPSocket, netid, mode)
        self.DisableFragmentationAndReportErrors(version, s)

        srcaddr = self.MyAddress(version, netid)
        dst_prefix, intermediate = {
            4: ("172.19.", "172.16.9.12"),
            6: ("2001:db8::", "2001:db8::1")
        }[version]
        dstaddr = self.GetRandomDestination(dst_prefix)

        if use_connect:
          s.connect((dstaddr, 1234))

        payload = self.PAYLOAD_SIZE * "a"

        # Send a packet and receive a packet too big.
        SendBigPacket(version, s, dstaddr, netid, payload)
        packets = self.ReadAllPacketsOn(netid)
        self.assertEquals(1, len(packets))
        _, toobig = Packets.ICMPPacketTooBig(version, intermediate, srcaddr,
                                             packets[0])
        self.ReceivePacketOn(netid, toobig)

        # Check that another send on the same socket returns EMSGSIZE.
        self.assertRaisesErrno(
            errno.EMSGSIZE,
            SendBigPacket, version, s, dstaddr, netid, payload)

        # If this is a connected socket, make sure the socket MTU was set.
        # Note that in IPv4 this only started working in Linux 3.6!
        if use_connect and (version == 6 or net_test.LINUX_VERSION >= (3, 6)):
          self.assertEquals(1280, self.GetSocketMTU(version, s))

        s.close()

        # Check that other sockets pick up the PMTU we have been told about by
        # connecting another socket to the same destination and getting its MTU.
        # This new socket can use any method to select its outgoing interface;
        # here we use a mark for simplicity.
        s2 = self.BuildSocket(version, net_test.UDPSocket, netid, "mark")
        s2.connect((dstaddr, 1234))
        self.assertEquals(1280, self.GetSocketMTU(version, s2))

        # Also check the MTU reported by ip route get, this time using the oif.
        routes = self.iproute.GetRoutes(dstaddr, self.ifindices[netid], 0, None)
        self.assertTrue(routes)
        route = routes[0]
        rtmsg, attributes = route
        self.assertEquals(iproute.RTN_UNICAST, rtmsg.type)
        metrics = attributes["RTA_METRICS"]
        self.assertEquals(metrics["RTAX_MTU"], 1280)

  def testIPv4BasicPMTU(self):
    self.CheckPMTU(4, True, ["mark", "oif"])
    self.CheckPMTU(4, False, ["mark", "oif"])

  def testIPv6BasicPMTU(self):
    self.CheckPMTU(6, True, ["mark", "oif"])
    self.CheckPMTU(6, False, ["mark", "oif"])

  @unittest.skipUnless(multinetwork_base.HAVE_UID_ROUTING, "no UID routes")
  def testIPv4UIDPMTU(self):
    self.CheckPMTU(4, True, ["uid"])
    self.CheckPMTU(4, False, ["uid"])

  @unittest.skipUnless(multinetwork_base.HAVE_UID_ROUTING, "no UID routes")
  def testIPv6UIDPMTU(self):
    self.CheckPMTU(6, True, ["uid"])
    self.CheckPMTU(6, False, ["uid"])

  # Making Path MTU Discovery work on unmarked  sockets requires that mark
  # reflection be enabled. Otherwise the kernel has no way to know what routing
  # table the original packet used, and thus it won't be able to clone the
  # correct route.

  @unittest.skipUnless(HAVE_MARK_REFLECT, "no mark reflection")
  def testIPv4UnmarkedSocketPMTU(self):
    self.SetMarkReflectSysctls(1)
    try:
      self.CheckPMTU(4, False, [None])
    finally:
      self.SetMarkReflectSysctls(0)

  @unittest.skipUnless(HAVE_MARK_REFLECT, "no mark reflection")
  def testIPv6UnmarkedSocketPMTU(self):
    self.SetMarkReflectSysctls(1)
    try:
      self.CheckPMTU(6, False, [None])
    finally:
      self.SetMarkReflectSysctls(0)


@unittest.skipUnless(multinetwork_base.HAVE_UID_ROUTING, "no UID routes")
class UidRoutingTest(multinetwork_base.MultiNetworkBaseTest):

  def GetRulesAtPriority(self, version, priority):
    rules = self.iproute.DumpRules(version)
    out = [(rule, attributes) for rule, attributes in rules
           if attributes.get("FRA_PRIORITY", 0) == priority]
    return out

  def CheckInitialTablesHaveNoUIDs(self, version):
    rules = []
    for priority in [0, 32766, 32767]:
      rules.extend(self.GetRulesAtPriority(version, priority))
    for _, attributes in rules:
      self.assertNotIn("FRA_UID_START", attributes)
      self.assertNotIn("FRA_UID_END", attributes)

  def testIPv4InitialTablesHaveNoUIDs(self):
    self.CheckInitialTablesHaveNoUIDs(4)

  def testIPv6InitialTablesHaveNoUIDs(self):
    self.CheckInitialTablesHaveNoUIDs(6)

  def CheckGetAndSetRules(self, version):
    def Random():
      return random.randint(1000000, 2000000)

    start, end = tuple(sorted([Random(), Random()]))
    table = Random()
    priority = Random()

    try:
      self.iproute.UidRangeRule(version, True, start, end, table,
                                priority=priority)

      rules = self.GetRulesAtPriority(version, priority)
      self.assertTrue(rules)
      _, attributes = rules[-1]
      self.assertEquals(priority, attributes["FRA_PRIORITY"])
      self.assertEquals(start, attributes["FRA_UID_START"])
      self.assertEquals(end, attributes["FRA_UID_END"])
      self.assertEquals(table, attributes["FRA_TABLE"])
    finally:
      self.iproute.UidRangeRule(version, False, start, end, table,
                                priority=priority)

  def testIPv4GetAndSetRules(self):
    self.CheckGetAndSetRules(4)

  def testIPv6GetAndSetRules(self):
    self.CheckGetAndSetRules(6)

  def ExpectNoRoute(self, addr, oif, mark, uid):
    # The lack of a route may be either an error, or an unreachable route.
    try:
      routes = self.iproute.GetRoutes(addr, oif, mark, uid)
      rtmsg, _ = routes[0]
      self.assertEquals(iproute.RTN_UNREACHABLE, rtmsg.type)
    except IOError, e:
      if int(e.errno) != -int(errno.ENETUNREACH):
        raise e

  def ExpectRoute(self, addr, oif, mark, uid):
    routes = self.iproute.GetRoutes(addr, oif, mark, uid)
    rtmsg, _ = routes[0]
    self.assertEquals(iproute.RTN_UNICAST, rtmsg.type)

  def CheckGetRoute(self, version, addr):
    self.ExpectNoRoute(addr, 0, 0, 0)
    for netid in self.NETIDS:
      uid = self.UidForNetid(netid)
      self.ExpectRoute(addr, 0, 0, uid)
    self.ExpectNoRoute(addr, 0, 0, 0)

  def testIPv4RouteGet(self):
    self.CheckGetRoute(4, net_test.IPV4_ADDR)

  def testIPv6RouteGet(self):
    self.CheckGetRoute(6, net_test.IPV6_ADDR)


class RulesTest(net_test.NetworkTest):

  RULE_PRIORITY = 99999

  def setUp(self):
    self.iproute = iproute.IPRoute()
    for version in [4, 6]:
      self.iproute.DeleteRulesAtPriority(version, self.RULE_PRIORITY)

  def tearDown(self):
    for version in [4, 6]:
      self.iproute.DeleteRulesAtPriority(version, self.RULE_PRIORITY)

  def testRuleDeletionMatchesTable(self):
    for version in [4, 6]:
      # Add rules with mark 300 pointing at tables 301 and 302.
      # This checks for a kernel bug where deletion request for tables > 256
      # ignored the table.
      self.iproute.FwmarkRule(version, True, 300, 301,
                              priority=self.RULE_PRIORITY)
      self.iproute.FwmarkRule(version, True, 300, 302,
                              priority=self.RULE_PRIORITY)
      # Delete rule with mark 300 pointing at table 302.
      self.iproute.FwmarkRule(version, False, 300, 302,
                              priority=self.RULE_PRIORITY)
      # Check that the rule pointing at table 301 is still around.
      attributes = [a for _, a in self.iproute.DumpRules(version)
                    if a.get("FRA_PRIORITY", 0) == self.RULE_PRIORITY]
      self.assertEquals(1, len(attributes))
      self.assertEquals(301, attributes[0]["FRA_TABLE"])


if __name__ == "__main__":
  unittest.main()
