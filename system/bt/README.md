# Fluoride Bluetooth stack

## Building and running on AOSP
Just build AOSP - Fluoride is there by default.

## Building and running on Linux

Instructions for Ubuntu, tested on 15.10 with GCC 5.2.1.

### Install required libraries

```sh
sudo apt-get install libevent-dev
```

### Install build tools

  - Install [ninja](https://ninja-build.org/) build system

```sh
sudo apt-get install ninja-build
```

or download binary from https://github.com/ninja-build/ninja/releases

  - Install [gn](https://chromium.googlesource.com/chromium/src/tools/gn/) -  meta-build system that generates NinjaBuild files.

Get sha1 of current version from [here](
https://chromium.googlesource.com/chromium/buildtools/+/master/linux64/gn.sha1) and then download corresponding executable:

```sh
wget -O gn http://storage.googleapis.com/chromium-gn/<gn.sha1>
```

i.e. if sha1 is "3491f6687bd9f19946035700eb84ce3eed18c5fa" (value from 24 Feb 2016) do

```sh
wget -O gn http://storage.googleapis.com/chromium-gn/3491f6687bd9f19946035700eb84ce3eed18c5fa
```

Then make binary executable and put it on your PATH, i.e.:

```sh
chmod a+x ./gn
sudo mv ./gn /usr/bin
```

### Download source

```sh
mkdir ~/fluoride
cd ~/fluoride
git clone https://android.googlesource.com/platform/system/bt
```

Then fetch third party dependencies:

```sh
cd ~/fluoride/bt
mkdir third_party
git clone https://github.com/google/googletest.git
git clone https://android.googlesource.com/platform/external/libchrome
git clone https://android.googlesource.com/platform/external/modp_b64
git clone https://android.googlesource.com/platform/external/tinyxml2
```

And third party dependencies of third party dependencies:

```sh
cd fluoride/bt/third_party/libchrome/base/third_party
mkdir valgrind
cd valgrind
curl https://chromium.googlesource.com/chromium/src/base/+/master/third_party/valgrind/valgrind.h?format=TEXT | base64 -d > valgrind.h
curl https://chromium.googlesource.com/chromium/src/base/+/master/third_party/valgrind/memcheck.h?format=TEXT | base64 -d > memcheck.h
```

Fluoride currently has dependency on some internal Android projects, which also need to be downloaded. This will be removed in future:

```sh
cd ~/fluoride
git clone https://android.googlesource.com/platform/system/core
git clone https://android.googlesource.com/platform/hardware/libhardware
git clone https://android.googlesource.com/platform/system/media
```

### Configure your build
We need to configure some paths to make the build successful. Run:

```sh
cd ~/fluoride/bt
gn args out/Default
```

This will prompt you to fill the contents of your "out/Default/args.gn" file. Make it look like below. Replace "/home/job" with path to your home directory, and don't use "~" in build arguments:

```sh
# Build arguments go here. Examples:
#   is_component_build = true
#   is_debug = false
# See "gn args <out_dir> --list" for available build arguments.

libhw_include_path = "/home/job/fluoride/libhardware/include"
core_include_path = "/home/job/fluoride/core/include"
audio_include_path = "/home/job/fluoride/media/audio/include"
```

Then generate your build files by calling

```sh
cd ~/fluoride/bt
gn gen out/Default
```

### Build

```sh
cd ~/fluoride/bt
ninja -C out/Default all
```

This will build all targets (the shared library, executables, tests, etc) and put them in out/Default. To build an individual target, replace "all" with the target of your choice, e.g. ```ninja -C out/Default net_test_osi```.

### Run

```sh
cd ~/fluoride/bt/out/Default
LD_LIBRARY_PATH=./ ./bluetoothtbd -create-ipc-socket=fluoride
```
