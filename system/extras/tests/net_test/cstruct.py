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

"""A simple module for declaring C-like structures.

Example usage:

>>> # Declare a struct type by specifying name, field formats and field names.
... # Field formats are the same as those used in the struct module.
... import cstruct
>>> NLMsgHdr = cstruct.Struct("NLMsgHdr", "=LHHLL", "length type flags seq pid")
>>>
>>>
>>> # Create instances from tuples or raw bytes. Data past the end is ignored.
... n1 = NLMsgHdr((44, 32, 0x2, 0, 491))
>>> print n1
NLMsgHdr(length=44, type=32, flags=2, seq=0, pid=491)
>>>
>>> n2 = NLMsgHdr("\x2c\x00\x00\x00\x21\x00\x02\x00"
...               "\x00\x00\x00\x00\xfe\x01\x00\x00" + "junk at end")
>>> print n2
NLMsgHdr(length=44, type=33, flags=2, seq=0, pid=510)
>>>
>>> # Serialize to raw bytes.
... print n1.Pack().encode("hex")
2c0000002000020000000000eb010000
>>>
>>> # Parse the beginning of a byte stream as a struct, and return the struct
... # and the remainder of the stream for further reading.
... data = ("\x2c\x00\x00\x00\x21\x00\x02\x00"
...         "\x00\x00\x00\x00\xfe\x01\x00\x00"
...         "more data")
>>> cstruct.Read(data, NLMsgHdr)
(NLMsgHdr(length=44, type=33, flags=2, seq=0, pid=510), 'more data')
>>>
"""

import ctypes
import struct


def Struct(name, fmt, fields):
  """Function that returns struct classes."""

  class Meta(type):

    def __len__(cls):
      return cls._length

    def __init__(cls, unused_name, unused_bases, namespace):
      # Make the class object have the name that's passed in.
      type.__init__(cls, namespace["_name"], unused_bases, namespace)

  class CStruct(object):
    """Class representing a C-like structure."""

    __metaclass__ = Meta

    _name = name
    _format = fmt
    _fields = fields

    _length = struct.calcsize(_format)
    if isinstance(_fields, str):
      _fields = _fields.split(" ")

    def _SetValues(self, values):
      super(CStruct, self).__setattr__("_values", list(values))

    def _Parse(self, data):
      data = data[:self._length]
      values = list(struct.unpack(self._format, data))
      self._SetValues(values)

    def __init__(self, values):
      # Initializing from a string.
      if isinstance(values, str):
        if len(values) < self._length:
          raise TypeError("%s requires string of length %d, got %d" %
                          (self._name, self._length, len(values)))
        self._Parse(values)
      else:
        # Initializing from a tuple.
        if len(values) != len(self._fields):
          raise TypeError("%s has exactly %d fields (%d given)" %
                          (self._name, len(self._fields), len(values)))
        self._SetValues(values)

    def _FieldIndex(self, attr):
      try:
        return self._fields.index(attr)
      except ValueError:
        raise AttributeError("'%s' has no attribute '%s'" %
                             (self._name, attr))

    def __getattr__(self, name):
      return self._values[self._FieldIndex(name)]

    def __setattr__(self, name, value):
      self._values[self._FieldIndex(name)] = value

    @classmethod
    def __len__(cls):
      return cls._length

    def Pack(self):
      return struct.pack(self._format, *self._values)

    def __str__(self):
      return "%s(%s)" % (self._name, ", ".join(
          "%s=%s" % (i, v) for i, v in zip(self._fields, self._values)))

    def __repr__(self):
      return str(self)

    def CPointer(self):
      """Returns a C pointer to the serialized structure."""
      buf = ctypes.create_string_buffer(self.Pack())
      # Store the C buffer in the object so it doesn't get garbage collected.
      super(CStruct, self).__setattr__("_buffer", buf)
      return ctypes.addressof(self._buffer)

  return CStruct


def Read(data, struct_type):
  length = len(struct_type)
  return struct_type(data), data[length:]
