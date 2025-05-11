# Viccyware-oelinux

The main repo for building Viccyware otas.

## Submodules

- /poky/poky -> [yoctoproject/poky](https://github.com/yoctoproject/poky) (walnascar)
- /poky/meta-openembedded -> [openembedded/meta-openembedded](https://github.com/openembedded/meta-openembedded) (walnascar)
- /anki/victor -> [Viccyware](https://github.com/Switch-modder/Viccyware) (Viccyware-tester)
- /anki/wired -> [wired](https://github.com/os-vector/wired) (main)

## Build

Make sure you have Docker installed, and configured so a regular user can use it.

```
git clone https://github.com/The-Viccyware-Group/Viccyware-oelinux/ --recurse-submodules --shallow-submodules --depth=1
cd Viccyware-oelinux
./build/build.sh -bt <dev/oskr> -bp <boot-passwd> -v <build-increment>
# boot password not required for dev
# example: ./build/build.sh -bt dev -v 1
```

### Where is my OTA?

`./_build/2.0.1.1.ota`

## Differences compared to normal Vector FW

-   New OS base
    -   Yocto Walnascar rather than Jethro
        -   glibc 2.41 (latest as of 04-2025)
-   `victor` software compiled with Clang 18.1.8 rather than 5.0.1
-   Rainbow eye color
    -   Can be activated in :8888/demo.html
-   Some Anki-era PRs have been merged
    -   Performances
        -   He will somewhat randomely do loosepixel and binaryeyes
    -   Better camera gamma correction
        -   He handles too-bright situations much better now
-   Snowboy wakeword engine
    -   Custom wake words!
-   `htop` and `rsync` are embedded
-   Python 3.13 rather than Python 2
-   Fixed fault code handler
    - No more 980 or 981 after crash on Vector 2.0
-   Global SSH key ([ssh_root_key](https://raw.githubusercontent.com/kercre123/unlocking-vector/refs/heads/main/ssh_root_key))

## What isn't there yet

- delta updates
- iptables

## Viccyware was made possible though the work of some amazing community members

- [Wire](https://github.com/kercre123)
- [Yrekcaz](https://github.com/Yrekcaz)
- [Froggitti](https://github.com/froggitti)
- [ThommoMC](https://github.com/ThommoMC)
- [Gaming Time](https://github.com/gamingtimevr)
- [Raj-jyot Deol / Switch_modder](https://github.com/Switch-modder)
