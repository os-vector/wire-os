//
// Copyright 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

#include <functional>
#include <memory>
#include <list>

extern "C" {
#include <sys/epoll.h>
}  // extern "C"

#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "vendor_libs/test_vendor_lib/include/command_packet.h"
#include "vendor_libs/test_vendor_lib/include/event_packet.h"
#include "vendor_libs/test_vendor_lib/include/packet.h"
#include "vendor_libs/test_vendor_lib/include/packet_stream.h"

namespace test_vendor_lib {

// Manages communication channel between HCI and the controller by providing the
// socketing mechanisms for reading/writing between the HCI and the controller.
class HciTransport : public base::MessageLoopForIO::Watcher {
 public:
  HciTransport();

  virtual ~HciTransport() = default;

  void CloseHciFd();

  void CloseVendorFd();

  int GetHciFd() const;

  int GetVendorFd() const;

  // Creates the underlying socketpair to be used as a communication channel
  // between the HCI and the vendor library/controller. Returns false if an
  // error occurs.
  bool SetUp();

  // Sets the callback that is run when command packets are received.
  void RegisterCommandHandler(
      std::function<void(std::unique_ptr<CommandPacket>)> callback);

  // Posts the event onto |outbound_events_| to be written sometime in the
  // future when the vendor file descriptor is ready for writing.
  void PostEventResponse(std::unique_ptr<EventPacket> event);

  // Posts the event onto |outbound_events_| after |delay| ms. A call to
  // |PostEventResponse| with |delay| 0 is equivalent to a call to |PostEvent|.
  void PostDelayedEventResponse(std::unique_ptr<EventPacket> event,
                                base::TimeDelta delay);

 private:
  // Wrapper class for sending events on a delay. The TimeStampedEvent object
  // takes ownership of a given event packet.
  class TimeStampedEvent {
   public:
    TimeStampedEvent(std::unique_ptr<EventPacket> event, base::TimeDelta delay);

    // Using this constructor is equivalent to calling the 2-argument
    // constructor with a |delay| of 0. It is used to generate event responses
    // with no delay.
    TimeStampedEvent(std::unique_ptr<EventPacket> event);

    const base::TimeTicks& GetTimeStamp() const;

    const EventPacket& GetEvent();

   private:
    std::shared_ptr<EventPacket> event_;

    // The time associated with the event, indicating the earliest time at which
    // |event_| will be sent.
    base::TimeTicks time_stamp_;
  };

  // base::MessageLoopForIO::Watcher overrides:
  void OnFileCanReadWithoutBlocking(int fd) override;

  void OnFileCanWriteWithoutBlocking(int fd) override;

  // Reads in a command packet and calls the command ready callback,
  // |command_handler_|, passing ownership of the command packet to the handler.
  void ReceiveReadyCommand() const;

  void AddEventToOutboundEvents(std::unique_ptr<TimeStampedEvent> event);

  // Write queue for sending events to the HCI. Event packets are removed from
  // the queue and written when write-readiness is signalled by the message
  // loop. After being written, the event packets are destructed.
  std::list<std::unique_ptr<TimeStampedEvent>> outbound_events_;

  // Callback executed in ReceiveReadyCommand() to pass the incoming command
  // over to the handler for further processing.
  std::function<void(std::unique_ptr<CommandPacket>)> command_handler_;

  // For performing packet-based IO.
  PacketStream packet_stream_;

  // The two ends of the socketpair. |hci_fd_| is handed back to the HCI in
  // bt_vendor.cc and |vendor_fd_| is used by |packet_stream_| to receive/send
  // data from/to the HCI. Both file descriptors are owned and managed by the
  // transport object, although |hci_fd_| can be closed by the HCI in
  // TestVendorOp().
  std::unique_ptr<base::ScopedFD> hci_fd_;
  std::unique_ptr<base::ScopedFD> vendor_fd_;

  // This should remain the last member so it'll be destroyed and invalidate
  // its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<HciTransport> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HciTransport);
};

}  // namespace test_vendor_lib
