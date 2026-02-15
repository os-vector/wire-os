#!/usr/bin/env python
"""
This script extracts btsnooz content from bugreports and generates
a valid btsnoop log file which can be viewed using standard tools
like Wireshark.

btsnooz is a custom format designed to be included in bugreports.
It can be described as:

base64 {
  file_header
  deflate {
    repeated {
      record_header
      record_data
    }
  }
}

where the file_header and record_header are modified versions of
the btsnoop headers.
"""


import base64
import fileinput
import struct
import sys
import zlib


# Enumeration of the values the 'type' field can take in a btsnooz
# header. These values come from the Bluetooth stack's internal
# representation of packet types.
TYPE_IN_EVT = 0x10
TYPE_IN_ACL = 0x11
TYPE_IN_SCO = 0x12
TYPE_OUT_CMD = 0x20
TYPE_OUT_ACL = 0x21
TYPE_OUT_SCO = 0x22


def type_to_direction(type):
  """
  Returns the inbound/outbound direction of a packet given its type.
  0 = sent packet
  1 = received packet
  """
  if type in [TYPE_IN_EVT, TYPE_IN_ACL, TYPE_IN_SCO]:
    return 1
  return 0


def type_to_hci(type):
  """
  Returns the HCI type of a packet given its btsnooz type.
  """
  if type == TYPE_OUT_CMD:
    return '\x01'
  if type == TYPE_IN_ACL or type == TYPE_OUT_ACL:
    return '\x02'
  if type == TYPE_IN_SCO or type == TYPE_OUT_SCO:
    return '\x03'
  if type == TYPE_IN_EVT:
    return '\x04'


def decode_snooz(snooz):
  """
  Decodes all known versions of a btsnooz file into a btsnoop file.
  """
  version, last_timestamp_ms = struct.unpack_from('=bQ', snooz)

  if version != 1 and version != 2:
    sys.stderr.write('Unsupported btsnooz version: %s\n' % version)
    exit(1)

  # Oddly, the file header (9 bytes) is not compressed, but the rest is.
  decompressed = zlib.decompress(snooz[9:])

  sys.stdout.write('btsnoop\x00\x00\x00\x00\x01\x00\x00\x03\xea')

  if version == 1:
    decode_snooz_v1(decompressed, last_timestamp_ms)
  elif version == 2:
    decode_snooz_v2(decompressed, last_timestamp_ms)


def decode_snooz_v1(decompressed, last_timestamp_ms):
  """
  Decodes btsnooz v1 files into a btsnoop file.
  """
  # An unfortunate consequence of the file format design: we have to do a
  # pass of the entire file to determine the timestamp of the first packet.
  first_timestamp_ms = last_timestamp_ms + 0x00dcddb30f2f8000
  offset = 0
  while offset < len(decompressed):
    length, delta_time_ms, type = struct.unpack_from('=HIb', decompressed, offset)
    offset += 7 + length - 1
    first_timestamp_ms -= delta_time_ms

  # Second pass does the actual writing out to stdout.
  offset = 0
  while offset < len(decompressed):
    length, delta_time_ms, type = struct.unpack_from('=HIb', decompressed, offset)
    first_timestamp_ms += delta_time_ms
    offset += 7
    sys.stdout.write(struct.pack('>II', length, length))
    sys.stdout.write(struct.pack('>II', type_to_direction(type), 0))
    sys.stdout.write(struct.pack('>II', (first_timestamp_ms >> 32), (first_timestamp_ms & 0xFFFFFFFF)))
    sys.stdout.write(type_to_hci(type))
    sys.stdout.write(decompressed[offset : offset + length - 1])
    offset += length - 1


def decode_snooz_v2(decompressed, last_timestamp_ms):
  """
  Decodes btsnooz v2 files into a btsnoop file.
  """
  # An unfortunate consequence of the file format design: we have to do a
  # pass of the entire file to determine the timestamp of the first packet.
  first_timestamp_ms = last_timestamp_ms + 0x00dcddb30f2f8000
  offset = 0
  while offset < len(decompressed):
    length, packet_length, delta_time_ms, snooz_type = struct.unpack_from('=HHIb', decompressed, offset)
    offset += 9 + length - 1
    first_timestamp_ms -= delta_time_ms

  # Second pass does the actual writing out to stdout.
  offset = 0
  while offset < len(decompressed):
    length, packet_length, delta_time_ms, snooz_type = struct.unpack_from('=HHIb', decompressed, offset)
    first_timestamp_ms += delta_time_ms
    offset += 9
    sys.stdout.write(struct.pack('>II', packet_length, length))
    sys.stdout.write(struct.pack('>II', type_to_direction(snooz_type), 0))
    sys.stdout.write(struct.pack('>II', (first_timestamp_ms >> 32), (first_timestamp_ms & 0xFFFFFFFF)))
    sys.stdout.write(type_to_hci(snooz_type))
    sys.stdout.write(decompressed[offset : offset + length - 1])
    offset += length - 1


def main():
  if len(sys.argv) > 2:
    sys.stderr.write('Usage: %s [bugreport]\n' % sys.argv[0])
    exit(1)

  iterator = fileinput.input()
  found = False
  base64_string = ""
  for line in iterator:
    if found:
      if line.find('--- END:BTSNOOP_LOG_SUMMARY') != -1:
        decode_snooz(base64.standard_b64decode(base64_string))
        sys.exit(0)
      base64_string += line.strip()

    if line.find('--- BEGIN:BTSNOOP_LOG_SUMMARY') != -1:
      found = True

  if not found:
    sys.stderr.write('No btsnooz section found in bugreport.\n')
    sys.exit(1)


if __name__ == '__main__':
  main()
