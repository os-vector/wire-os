//
//  Copyright (C) 2015 Google, Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at:
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#include "service/ipc/ipc_handler_linux.h"

#include <sys/socket.h>
#include <sys/un.h>

#include <base/bind.h>

#include "osi/include/socket_utils/sockets.h"
#include "service/daemon.h"
#include "service/ipc/linux_ipc_host.h"
#include "service/settings.h"

namespace ipc {

IPCHandlerLinux::IPCHandlerLinux(bluetooth::Adapter* adapter,
                               IPCManager::Delegate* delegate)
    : IPCHandler(adapter, delegate),
      running_(false),
      thread_("IPCHandlerLinux"),
      keep_running_(true) {
}

IPCHandlerLinux::~IPCHandlerLinux() {
  // This will only be set if the Settings::create_ipc_socket_path() was
  // originally provided.
  if (!socket_path_.empty())
    unlink(socket_path_.value().c_str());
}

bool IPCHandlerLinux::Run() {
  CHECK(!running_);

  const std::string& android_suffix =
      bluetooth::Daemon::Get()->GetSettings()->android_ipc_socket_suffix();
  const base::FilePath& path =
      bluetooth::Daemon::Get()->GetSettings()->create_ipc_socket_path();

  // Both flags cannot be set at the same time.
  CHECK(android_suffix.empty() || path.empty());
  if (android_suffix.empty() && path.empty()) {
    LOG(ERROR) << "No domain socket path provided";
    return false;
  }

  CHECK(base::MessageLoop::current());  // An origin event loop is required.
  origin_task_runner_ = base::MessageLoop::current()->task_runner();

  if (!android_suffix.empty()) {
    int server_fd = osi_android_get_control_socket(android_suffix.c_str());
    if (server_fd == -1) {
      LOG(ERROR) << "Unable to get Android socket from: " << android_suffix;
      return false;
    }
    LOG(INFO) << "Binding to Android server socket:" << android_suffix;
    socket_.reset(server_fd);
  } else {
    LOG(INFO) << "Creating a Unix domain socket:" << path.value();

    // TODO(armansito): This is opens the door to potentially unlinking files in
    // the current directory that we're not supposed to. For now we will have an
    // assumption that the daemon runs in a sandbox but we should generally do
    // this properly.
    unlink(path.value().c_str());

    base::ScopedFD server_socket(socket(PF_UNIX, SOCK_SEQPACKET, 0));
    if (!server_socket.is_valid()) {
      LOG(ERROR) << "Failed to open domain socket for IPC";
      return false;
    }

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, path.value().c_str(),
            sizeof(address.sun_path) - 1);
    if (bind(server_socket.get(), (struct sockaddr*)&address, sizeof(address)) <
        0) {
      LOG(ERROR) << "Failed to bind IPC socket to address: " << strerror(errno);
      return false;
    }

    socket_.swap(server_socket);
    socket_path_ = path;
  }

  CHECK(socket_.is_valid());

  running_ = true;  // Set this here before launching the thread.

  // Start an IO thread and post the listening task.
  base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
  if (!thread_.StartWithOptions(options)) {
    LOG(ERROR) << "Failed to start IPCHandlerLinux thread";
    running_ = false;
    return false;
  }

  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&IPCHandlerLinux::StartListeningOnThread, this));

  return true;
}

void IPCHandlerLinux::Stop() {
  keep_running_ = false;

  // At this moment the listening thread might be blocking on the accept
  // syscall. Shutdown and close the server socket before joining the thread to
  // interrupt accept so that the main thread doesn't keep blocking.
  shutdown(socket_.get(), SHUT_RDWR);
  socket_.reset();

  // Join and clean up the thread.
  thread_.Stop();

  // Thread exited. Notify the delegate. Post this on the event loop so that the
  // callback isn't reentrant.
  NotifyStoppedOnOriginThread();
}

void IPCHandlerLinux::StartListeningOnThread() {
  CHECK(socket_.is_valid());
  CHECK(adapter());
  CHECK(running_);

  LOG(INFO) << "Listening to incoming connections";

  int status = listen(socket_.get(), SOMAXCONN);
  if (status < 0) {
    LOG(ERROR) << "Failed to listen on domain socket: " << strerror(errno);
    origin_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&IPCHandlerLinux::ShutDownOnOriginThread, this));
    return;
  }

  NotifyStartedOnOriginThread();

  // TODO(armansito): The code below can cause the daemon to run indefinitely if
  // the thread is joined while it's in the middle of the EventLoop() call. The
  // EventLoop() won't exit until a client terminates the connection, however
  // this can be fixed by using the |thread_|'s MessageLoopForIO instead (since
  // it gets stopped along with the thread).
  // TODO(icoolidge): accept simultaneous clients
  while (keep_running_.load()) {
    int client_socket = accept4(socket_.get(), nullptr, nullptr, SOCK_NONBLOCK);
    if (client_socket < 0) {
      LOG(ERROR) << "Failed to accept client connection: " << strerror(errno);
      continue;
    }

    LOG(INFO) << "Established client connection: fd=" << client_socket;

    LinuxIPCHost ipc_host(client_socket, adapter());

    // TODO(armansito): Use |thread_|'s MessageLoopForIO instead of using a
    // custom event loop to poll from the socket.
    ipc_host.EventLoop();
  }
}

void IPCHandlerLinux::ShutDownOnOriginThread() {
  LOG(INFO) << "Shutting down IPCHandlerLinux thread";
  thread_.Stop();
  running_ = false;

  NotifyStoppedOnCurrentThread();
}

void IPCHandlerLinux::NotifyStartedOnOriginThread() {
  if (!delegate())
    return;

  origin_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IPCHandlerLinux::NotifyStartedOnCurrentThread, this));
}

void IPCHandlerLinux::NotifyStartedOnCurrentThread() {
  if (delegate())
    delegate()->OnIPCHandlerStarted(IPCManager::TYPE_LINUX);
}

void IPCHandlerLinux::NotifyStoppedOnOriginThread() {
  if (!delegate())
    return;

  origin_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IPCHandlerLinux::NotifyStoppedOnCurrentThread, this));
}

void IPCHandlerLinux::NotifyStoppedOnCurrentThread() {
  if (delegate())
    delegate()->OnIPCHandlerStopped(IPCManager::TYPE_LINUX);
}

}  // namespace ipc
