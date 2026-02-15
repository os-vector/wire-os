/*
**
** Copyright 2015, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

//
// Helper class to perform cpu setup (if needed) prior to a profile collection.
//
class HardwireCpuHelper {
 public:

  // The constructor for this class checks to see if the 'mpdecision'
  // service is running; if so (and if 'perform' is TRUE), then it
  // disables the service and on-lines all of the available cores/cpus
  // (anything listed in /sys/devices/system/cpu/possible). The
  // destructor will re-enable the mpdecision service if it was
  // previously disabled.
  HardwireCpuHelper(bool perform);
  virtual ~HardwireCpuHelper();

 private:
  bool mpdecision_stopped_;

  // Collect the number of available cpus/cores from /sys/devices/system/cpu/possible
  int GetNumCores();

  // Returns TRUE if the system service 'mpdecision' is running
  bool GetMpdecisionRunning();

  // Online/offline the specified cpu
  void OnlineCore(int whichCore, int onoff);

  // Enable/disable the mpdecision service via the equivalent of
  //   setprop ctl.start mpdecision
  //   setprop ctl.stop mpdecision
  void StopMpdecision();
  void RestartMpdecision();
};
