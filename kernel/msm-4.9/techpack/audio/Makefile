AUDIO_8909_CONF := $(srctree)/techpack/audio/config/msm8909auto.conf
ifeq ($(CONFIG_ARCH_MSM8909), y)
include $(AUDIO_8909_CONF)
export $(shell sed -n 's/^\(CONFIG_[A-Za-z0-9_]*\)=.*/\1/p' $(AUDIO_8909_CONF))
endif
ifeq ($(CONFIG_ARCH_SDM845), y)
include $(srctree)/techpack/audio/config/sdm845auto.conf
export
endif
ifeq ($(CONFIG_ARCH_SDM670), y)
include $(srctree)/techpack/audio/config/sdm710auto_static.conf
export
endif
ifeq ($(CONFIG_ARCH_SDXPOORWILLS), y)
include $(srctree)/techpack/audio/config/sdxpoorwillsauto.conf
export
endif

subdir-ccflags-y += -Wno-error
subdir-ccflags-y += -Wno-address-of-packed-member

USERINCLUDE     += \
                -I$(srctree)/techpack/audio/include/uapi \

LINUXINCLUDE    += \
                -I$(srctree)/techpack/audio/include/uapi \
                -I$(srctree)/techpack/audio/include

ifeq ($(CONFIG_ARCH_MSM8909), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/msm8909autoconf.h
endif
ifeq ($(CONFIG_ARCH_SDM845), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdm845autoconf.h
endif
ifeq ($(CONFIG_ARCH_SDM670), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdm710autoconf.h
endif
ifeq ($(CONFIG_ARCH_SDXPOORWILLS), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdxpoorwillsautoconf.h
endif

obj-y += asoc/
obj-y += dsp/
obj-y += ipc/
obj-y += soc/
