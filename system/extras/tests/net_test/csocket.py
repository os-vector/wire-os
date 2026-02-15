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

"""Python wrapper for C socket calls and data structures."""

import ctypes
import ctypes.util
import os
import socket
import struct

import cstruct


# Data structures.
CMsgHdr = cstruct.Struct("cmsghdr", "@Lii", "len level type")
Iovec = cstruct.Struct("iovec", "@LL", "base len")
MsgHdr = cstruct.Struct("msghdr", "@LLLLLLi",
                        "name namelen iov iovlen control msg_controllen flags")
SockaddrIn = cstruct.Struct("sockaddr_in", "=HH4sxxxxxxxx", "family port addr")
SockaddrIn6 = cstruct.Struct("sockaddr_in6", "=HHI16sI",
                             "family port flowinfo addr scope_id")

# Constants.
CMSG_ALIGNTO = struct.calcsize("@L")  # The kernel defines this as sizeof(long).
MSG_CONFIRM = 0X800

# Find the C library.
libc = ctypes.CDLL(ctypes.util.find_library("c"), use_errno=True)


def PaddedLength(length):
  return CMSG_ALIGNTO * ((length / CMSG_ALIGNTO) + (length % CMSG_ALIGNTO != 0))


def MaybeRaiseSocketError(ret):
  if ret < 0:
    errno = ctypes.get_errno()
    raise socket.error(errno, os.strerror(errno))


def Sockaddr(addr):
  if ":" in addr[0]:
    family = socket.AF_INET6
    if len(addr) == 4:
      addr, port, flowinfo, scope_id = addr
    else:
      (addr, port), flowinfo, scope_id = addr, 0, 0
    addr = socket.inet_pton(family, addr)
    return SockaddrIn6((family, socket.ntohs(port), socket.ntohl(flowinfo),
                        addr, scope_id))
  else:
    family = socket.AF_INET
    addr, port = addr
    addr = socket.inet_pton(family, addr)
    return SockaddrIn((family, socket.ntohs(port), addr))


def _MakeMsgControl(optlist):
  """Creates a msg_control blob from a list of cmsg attributes.

  Takes a list of cmsg attributes. Each attribute is a tuple of:
   - level: An integer, e.g., SOL_IPV6.
   - type: An integer, the option identifier, e.g., IPV6_HOPLIMIT.
   - data: The option data. This is either a string or an integer. If it's an
     integer it will be written as an unsigned integer in host byte order. If
     it's a string, it's used as is.

  Data is padded to an integer multiple of CMSG_ALIGNTO.

  Args:
    optlist: A list of tuples describing cmsg options.

  Returns:
    A string, a binary blob usable as the control data for a sendmsg call.

  Raises:
    TypeError: Option data is neither an integer nor a string.
  """
  msg_control = ""

  for i, opt in enumerate(optlist):
    msg_level, msg_type, data = opt
    if isinstance(data, int):
      data = struct.pack("=I", data)
    elif not isinstance(data, str):
      raise TypeError("unknown data type for opt %i: %s" % (i, type(data)))

    datalen = len(data)
    msg_len = len(CMsgHdr) + datalen
    padding = "\x00" * (PaddedLength(datalen) - datalen)
    msg_control += CMsgHdr((msg_len, msg_level, msg_type)).Pack()
    msg_control += data + padding

  return msg_control


def Bind(s, to):
  """Python wrapper for connect."""
  ret = libc.bind(s.fileno(), to.CPointer(), len(to))
  MaybeRaiseSocketError(ret)
  return ret

def Connect(s, to):
  """Python wrapper for connect."""
  ret = libc.connect(s.fileno(), to.CPointer(), len(to))
  MaybeRaiseSocketError(ret)
  return ret


def Sendmsg(s, to, data, control, flags):
  """Python wrapper for sendmsg.

  Args:
    s: A Python socket object. Becomes sockfd.
    to: An address tuple, or a SockaddrIn[6] struct. Becomes msg->msg_name.
    data: A string, the data to write. Goes into msg->msg_iov.
    control: A list of cmsg options. Becomes msg->msg_control.
    flags: An integer. Becomes msg->msg_flags.

  Returns:
    If sendmsg succeeds, returns the number of bytes written as an integer.

  Raises:
    socket.error: If sendmsg fails.
  """
  # Create ctypes buffers and pointers from our structures. We need to hang on
  # to the underlying Python objects, because we don't want them to be garbage
  # collected and freed while we have C pointers to them.

  # Convert the destination address into a struct sockaddr.
  if to:
    if isinstance(to, tuple):
      to = Sockaddr(to)
    msg_name = to.CPointer()
    msg_namelen = len(to)
  else:
    msg_name = 0
    msg_namelen = 0

  # Convert the data to a data buffer and a struct iovec pointing at it.
  if data:
    databuf = ctypes.create_string_buffer(data)
    iov = Iovec((ctypes.addressof(databuf), len(data)))
    msg_iov = iov.CPointer()
    msg_iovlen = 1
  else:
    msg_iov = 0
    msg_iovlen = 0

  # Marshal the cmsg options.
  if control:
    control = _MakeMsgControl(control)
    controlbuf = ctypes.create_string_buffer(control)
    msg_control = ctypes.addressof(controlbuf)
    msg_controllen = len(control)
  else:
    msg_control = 0
    msg_controllen = 0

  # Assemble the struct msghdr.
  msghdr = MsgHdr((msg_name, msg_namelen, msg_iov, msg_iovlen,
                   msg_control, msg_controllen, flags)).Pack()

  # Call sendmsg.
  ret = libc.sendmsg(s.fileno(), msghdr, 0)
  MaybeRaiseSocketError(ret)

  return ret
