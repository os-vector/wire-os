# branch alert

- This branch is an attempt to implement the msm8916-mainline kernel. It's 6.19.0 for early Qualcomm SoCs.
- So far, I have written a DTS, and have gotten the following working:
    - wlan mostly
    - display
    - both backlight LEDs
    - IMU probably
    - audio (and therefore qdsp)
    - body comms
    - camera! (just gc1066 for now)
- currently not working
    - userdata decryption
- Why do this?
    - notice how basically all of the Qualcomm stuff is gone
        - that's why.
- What else is there to be done?
    - vector 2.0 camera
    - wlan needs to be tuned for stability
    - sysfs still needs proper init scripts for the hardware
    - need to rewrite ankibluetoothd since that expects a downstream driver
        - hopefully bluez is stable enough
    - right now, it won't actually boot on a bot unless you're smart and know how to build lk1st
        - i need to get lk2nd building, and get that in the boot partition, with the actual kernel and stuff at a 512KiB offset


# WireOS

**The main repo for WireOS.**

WireOS serves as a nice, stable, and maintained base for custom Anki Vector firmware.

This builds the OS, the /anki programs (`wire-os-victor`), and puts it all into a final OTA. This repo can be thought of as `wire-os-oelinux`.

## Vector

[Vector is a cute, animated home robot created by Anki](https://www.youtube.com/watch?v=Qy2Z2TWAt6A). They went under in 2019. The assets were bought up by Digital Dream Labs in 2020. Eventually, Vector's code leaked, and soon after that, a universal Vector unlocking tool was made available.

## Yocto

Yocto is the toolkit this repo uses to create OS images. Yocto, in of itself, is not a distribution. It's a toolkit which helps one create replicable OS builds with only little difficulty.

This is based off of the leaked [vicos-oelinux](https://github.com/kercre123/vicos-oelinux) repo. Qualcomm provided Anki with a Yocto BSP - that's what vicos-oelinux is. It is terribly old. I updated everything so it works with the latest Yocto layers.

## Prebuilt OTA

WireOS is in the Custom Firmware category of [https://websetup.froggitti.net/](https://websetup.froggitti.net/). Put your unlocked bot into recovery mode (hold the button for 15 seconds on the charger), head to the site, choose the Custom Firmware stack, connect to the bot, choose WireOS, then go through the process.

The actual latest dev OTA is available here: [http://ota.pvic.xyz/vic/latest/dev.ota](http://ota.pvic.xyz/vic/latest/dev.ota)

## Build

- WireOS must be built on Linux, either x86_64 or aarch64.
- Minimum specs:
    -   x86_64 or aarch64 CPU
    -   4 cores
    -   8 GB of RAM
    -   100 GB of free storage
- Recommended specs:
    -   x86_64 CPU
    -   8 or more cores
    -   16 or more GB of RAM
    -   200 or more GB of free storage
- A minimum spec machine might take up to 3 hours to build a full OTA. A beefy one takes around half an hour.
- It is recommended to build WireOS on an x86_64 CPU via the Docker method.
- If you want to build on aarch64, you have to go the bare metal route. The Docker method cannot be used for aarch64 build machines yet.
- **Asahi Linux cannot be used to build WireOS.** 99% of the build happens, but it fails during one of the final in-image configuration stages due to an Asahi-specific issue with `qemu-arm`.
    -   I had success building WireOS in a Debian VM on my M3 Macbook Air using UTM. A QEMU+KVM VM in Asahi would probably work too.
- **Click an option below.**

<details>
<summary><strong>Build in Docker (recommended) (x86_64 only)</strong></summary>
<br />

- **You do not need to make a container yourself. Just follow these steps. The build script handles it for you.**

1. [Install Docker](https://docs.docker.com/engine/install/), git, and wget.

2. Configure Docker so a regular user can use it:

```
sudo groupadd docker
sudo gpasswd -a $USER docker
newgrp docker
sudo chown root:docker /var/run/docker.sock
sudo chmod 660 /var/run/docker.sock
```

3. Clone and build:

```
git clone https://github.com/os-vector/wire-os --recurse-submodules
cd wire-os
./build/build.sh -bt dev -v <build-increment>
# build-increment can be any number you want. it will be the final number of the OTA: 3.0.1.<incrememnt>.ota
```

</details>

<details>
<summary><strong>Build on bare metal (x86_64 and aarch64)</strong></summary>

- Note: Yocto flips out if you try to use Python via a pyenv. Make sure you are using the OS's native Python only.

1. Run a [distribution supported by Yocto](https://docs.yoctoproject.org/dev/ref-manual/system-requirements.html#supported-linux-distributions).
    -   I recommend Debian 12 and up or Ubuntu 22.04 and up. Anything in this list with a glibc version 2.35 or above should work.
    -   Arch Linux seems to work too.

2. Install the required packages:

- Debian/Ubuntu
```
sudo apt install -y build-essential chrpath cpio debianutils diffstat expect file git iputils-ping libacl1 locales python3 python3-git python3-jinja2 python3-pexpect python3-subunit socat unzip wget xz-utils zstd gnupg flex bison gperf zip curl zlib1g-dev libncurses5-dev x11proto-core-dev libx11-dev libxml-simple-perl libc6-dev libgl1-mesa-dev tofrodos libxml2-utils xsltproc genisoimage gawk p7zip-full android-sdk-libsparse-utils ruby subversion libssl-dev protobuf-compiler pkg-config nano ninja-build clang ccache libc++-dev rsync cmake automake libtool
```

3. Clone and build (***with -nd flag***):
```
git clone https://github.com/os-vector/wire-os --recurse-submodules
cd wire-os
./build/build.sh -nd -bt dev -v <build-increment>
# build-increment can be any number you want. it will be the final number of the OTA: 3.0.1.<incrememnt>.ota
```

</details>

### Where is my OTA?

`./_build/3.0.1.<increment>.ota`

## build.sh flags

```
-bt <build-type>
    required. build-type: [dev|oskr]
    dev is recommended. if you unlocked a bot with unlock-prod-*.ota, use that
-v <increment>
    required. increment: [any int 0-9999]
    (final file will be 3.0.1.<build increment>.ota)
-bp <password>
    boot image signing password: [string]
    not required for dev builds
-nd
    build on bare metal rather than in Docker
-ui <ui-option>
    use a different Yocto UI: [knotty|taskexp|taskexp_ncurses|ncurses|teamcity]
    default is knotty. ncurses is cool but requires you to CTRL+C after completion.
    only add this argument if you know what you are doing.
```

## Development path

- **Most work should be done in `wire-os-victor`. Generally, that's all you need to have cloned. That can be worked on on a less beefy Linux laptop or M-series MacBook. If you have a modern base WireOS OTA installed; you can clone `wire-os-victor`, make changes, build that standalone, and deploy that to your robot. This repo is more meant to be cloned to a build server, and built less often.**

## Rebuilds

- I try to make it so whenever changes are made, you don't need to do a full rebuild; however, due to this being synced up to poky's `master` branch, behavior can be unpredictable. **Due to this, I recommend doing a full rebuild each time.** You can clean your build directory by running `sudo rm -rf poky/build/tmp-glibc poky/build/cache poky/build/sstate-cache poky/build/downloads`.

## Donate

If you want to :P

[![Buy Me A Coffee](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://buymeacoffee.com/kercre123)

## Differences compared to normal Vector FW

-   New OS base
    -   Yocto Whinlatter rather than Jethro
        -   glibc 2.42 (latest as of 11-2025)
-   `victor` software compiled with Clang 20.1.8 rather than 5.0.1
    -	The code is properly fixed so there are no compile warnings
-   Rainbow eye color
    -   Can be activated in :8888/demo.html
-   Some Anki-era PRs have been merged
    -   Performances
        -   He will somewhat randomly do loosepixel and binaryeyes
    -   Better camera gamma correction
        -   He handles too-bright situations much better now
-   Picovoice Porcupine (1.5) wakeword engine
    -   Custom wake words in :8080 webserver!
-   `htop` and `rsync` are embedded
-   No more Python - update-engine was rewritten in C++
-   General bug fixes - for instance, now he won't read the EMR partition upon every single screen draw (DDL bug)
-   :8080 webserver for configuring things I don't want to integrate into a normal app
-   Cat and dog detection (basic, similar to Cozmo)
-   Smaller OTA size - a dev OTA is 153M somehow
-   New Anki boot animation, new pre-boot-anim splash screen, rainbow backpack light animations
-   TensorFlow Lite has been updated to v2.19.0 (a modern 2025 release)
	-  This means we can maybe leverage the GPU delegate at some point
	-  XNNPACK - the CPU delegate - is faster than what was there before
-   OpenCV has been updated to 4.12.0 (latest as of 11-2025)
  	-  Much better SDK streaming performance
-   [Face overlays](https://www.reddit.com/r/AnkiVector/comments/1lteb3m/_/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button)
        -  How to activate: [wire-os-victor PR #17](https://github.com/os-vector/wire-os-victor/pull/17)
-   Global SSH key: ([ssh_root_key](https://raw.githubusercontent.com/kercre123/unlocking-vector/refs/heads/main/ssh_root_key))

## Helpful scripts / aliases

-	`ddn [on/off]`
	-	Turns on/off DevDoNothing, which makes the bot stand still until shaken.
-	`reonboard`
	-	Puts him back into onboarding mode without fully clearing user data
-	`vmesg [-c|-t] <grep args>`
	-	A wrapper for cat/tail /var/log/messages.
-	`temper`
	-	Simple script which tells you CPU temps
-	`voff`
	-	Shuts the bot off, closes your SSH session before doing so
		-	(the shutdown command just restarts the bot, this is different)
-	`mrw`
	-	mount -o rw,remount /

## Proprietary software notes

-	This repo contains lots of proprietary Qualcomm code and prebuilt software.
-	After a stupid amount of work, I have most HAL programs compiling with Yocto's GCC 15. It wasn't terribly difficult since it's generally all autotools, but some jank is still involved, and it was still time-consuming.
-	The camera programs and *some* of the BLE programs are being copied in rather than compiled.
	-	Why not compile camera programs? Because I would have to add 2GB to the repo and figure out how to use the weird Qualcomm-specific toolchain.
	-	Why not compile those BLE programs? `ankibluetoothd` and `hci_qcomm_init` are able to compile under GCC 15, but there is some weird low-level issue which makes them unable to properly communicate with a BLE library. So, for now, I am just copying pre-compiled ones in. I will probably try to fix this at some point.
-   The kernel is still msm-3.18. Mainline might be possible. I was able to boot `msm8916-mainline` on a Vector and get some hardware peripherals working, but some fundamental ones (camera, Wi-Fi) will take quite a bit of work.

## How this upgrade was done

-	Much work upgrading Yocto recipes.
-	All of the software is compiling with Yocto's GCC 15 or the Clang 20.1.8 vicos-sdk toolchain, with a couple of tiny exceptions.
-	Some recipes are still somewhat old - these include wpa_supplicant and connman (I had issues with SAE - he's able to recognize SAE networks, but his WLAN driver and kernel don't know how to actually connect to it, and I was unable to disable it in modern wpa_supplicant and connman)
