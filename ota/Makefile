ifndef VERBOSE
.SILENT:
endif

IMG_DIR ?= ../poky/build/tmp-glibc/deploy/images/apq8009-robot-robot-perf
OS_VERSION_FILE=../poky/build/tmp-glibc/work/apq8009_robot-oe-linux-gnueabi/machine-robot-image/1.0/rootfs/etc/os-version
BUILD=../_build
BOOT_STEM=apq8009-robot-boot
SYS_STEM=apq8009-robot-sysfs
MANIFEST=$(BUILD)/manifest

BOOT_GZ=$(BUILD)/$(BOOT_STEM).img.gz
BOOT_GZ_ENC=$(BOOT_GZ).enc
BOOT_STATS=$(BUILD)/$(BOOT_STEM).stats
SYS_GZ=$(BUILD)/$(SYS_STEM).img.gz
SYS_STATS=$(BUILD)/$(SYS_STEM).stats
SYS_GZ_ENC=$(SYS_GZ).enc

GZIP_MODE=--best
GZIP_WBITS=31

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  TAR=gtar
  export COPYFILE_DISABLE=1
  # COPYFILE_DISABLE=1 causes OSX not to put '._' junk in our TAR file when files have extended attributes
else
  TAR=tar
endif

OTAKEY ?= ota_prod.key
OTAPASS ?= file:ota_test.pass
OTAENC ?= 1
ANKIDEV ?= 1
REBOOT_AFTER_INSTALL ?= 0

ifeq ($(ANKIDEV), 0)
 MANIFEST_VERSION = 0.9.2
else
 MANIFEST_VERSION = 1.0.0
endif

UPDATE_VERSION ?= $(shell cat $(OS_VERSION_FILE))

OTA_FILE=$(BUILD)/vicos-$(UPDATE_VERSION).ota

ifeq ($(DO_SIGN),1)
  ota_file_target := $(OTA_FILE)-sign
else
  ota_file_target := $(OTA_FILE)
endif

all: $(BUILD) $(ota_file_target)

clean:
	rm -rf $(BUILD)

deepclean: clean
	rm -f $(IMG_DIR)/*

.PHONY: clean deepclean

.PRECIOUS: $(BUILD)/%.img

$(BUILD):
	mkdir $(BUILD)

$(BUILD)/%.img: $(IMG_DIR)/%.img
	cp $< $@
	# Pad image to a multiple of 4096 bytes.  Needed for later delta updates
	dd if=/dev/zero of=$@ bs=1 count=$(shell echo $$(( 4096 - `stat -c %s $<` % 4096 ))) conv=notrunc oflag=append > /dev/null 2> /dev/null

%.img.gz: %.img
	echo "Compressing $<..."
	./pigz/amd64/pigz $(GZIP_MODE) --force --keep $<

%.img.gz.enc: %.img.gz
	openssl aes-256-ctr -pass $(OTAPASS) -md md5 -in $< -out $@ > /dev/null 2> /dev/null

$(IMG_DIR)/apq8009-robot-sysfs.ext4:
	img2simg $(IMG_DIR)/apq8009-robot-raw-sysfs.ext4 $@

$(BUILD)/%.img: $(IMG_DIR)/%.ext4
	simg2img $< $@

%.stats: %.img
	echo "bytes=`wc -c $< | awk '{ print $$1 }'`"            > $@
	echo "sha256=`shasum -a256 -b $< | awk '{ print $$1 }'`" >> $@

%.sha256: %.ini
	openssl dgst -sha256 -sign $(OTAKEY) -passin env:OTA_MANIFEST_SIGNING_KEY -out $@ $<

orange_boot_img: $(BUILD)
	curl http://vmosAlpha.ankicore.com/apq8009-robot-boot.img.gz.enc > $(BOOT_GZ_ENC)
	touch $(BOOT_GZ_ENC)
	curl http://vmosAlpha.ankicore.com/apq8009-robot-boot.stats > $(BOOT_STATS)
	touch $(BOOT_STATS)

$(MANIFEST).ini: $(BUILD) $(BOOT_STATS) $(SYS_STATS)
	echo "[META]"                                > $@
	echo "manifest_version=$(MANIFEST_VERSION)" >> $@
	echo "update_version=$(UPDATE_VERSION)"     >> $@
	echo "ankidev=$(ANKIDEV)"                   >> $@
	echo "num_images=2"                         >> $@
	echo "reboot_after_install=$(REBOOT_AFTER_INSTALL)" >> $@
	echo "[BOOT]"                               >> $@
	echo "encryption=$(OTAENC)"                 >> $@
	echo "delta=0"                              >> $@
	echo "compression=gz"                       >> $@
	echo "wbits=$(GZIP_WBITS)"                  >> $@
	cat $(BOOT_STATS)                           >> $@
	echo "[SYSTEM]"                             >> $@
	echo "encryption=$(OTAENC)"                 >> $@
	echo "delta=0"                              >> $@
	echo "compression=gz"                       >> $@
	echo "wbits=$(GZIP_WBITS)"                  >> $@
	cat $(SYS_STATS)                            >> $@

PERMS="0400"
OWNER="root:0"
GROUP="root:0"

$(OTA_FILE): $(MANIFEST).ini $(BOOT_GZ_ENC) $(SYS_GZ_ENC)
	$(TAR) --transform='flags=r;s|.enc||' -cf $@ \
	--mode=$(PERMS) \
	--owner=$(OWNER) \
	--group=$(GROUP) \
	-C $(BUILD) \
	manifest.ini \
	apq8009-robot-boot.img.gz.enc \
	apq8009-robot-sysfs.img.gz.enc

$(OTA_FILE)-sign: $(MANIFEST).sha256 $(BOOT_GZ_ENC) $(SYS_GZ_ENC)
	$(TAR) --transform='flags=r;s|.enc||' -cf $(patsubst %-sign,%, $@) \
	--mode=$(PERMS) \
	--owner=$(OWNER) \
	--group=$(GROUP) \
	-C $(BUILD) \
	manifest.ini \
	manifest.sha256 \
	apq8009-robot-boot.img.gz.enc \
	apq8009-robot-sysfs.img.gz.enc

serve: $(OTA_FILE)
	cd $(BUILD); python -m SimpleHTTPServer 5555


###############################################################################

NON_SECURE_KERNEL=$(IMG_DIR)/apq8009-robot-boot.img.nonsecure
PROD_KERNEL=$(IMG_DIR)/apq8009-robot-boot.img
BOOT_KEY ?= qtipri.encrypted.key
OSKR_BOOT_KEY ?= qtioskrpri.encrypted.key
DEV_BOOT_KEY = vble-qti.key

prodsign: $(NON_SECURE_KERNEL)
	openssl dgst -sha256 -binary $(NON_SECURE_KERNEL) > $(NON_SECURE_KERNEL).sha256
	openssl pkeyutl -sign -in $(NON_SECURE_KERNEL).sha256 -passin env:BOOT_IMAGE_SIGNING_PASSWORD -inkey $(BOOT_KEY) -out $(NON_SECURE_KERNEL).sig -pkeyopt digest:sha256 -pkeyopt rsa_padding_mode:pkcs1
	dd if=/dev/zero of=$(NON_SECURE_KERNEL).sig.padded bs=2048 count=1
	dd if=$(NON_SECURE_KERNEL).sig of=$(NON_SECURE_KERNEL).sig.padded conv=notrunc
	cat $(NON_SECURE_KERNEL) $(NON_SECURE_KERNEL).sig.padded > $(PROD_KERNEL)

oskrsign: $(NON_SECURE_KERNEL)
	echo "OSKR-signing OTA..."
	openssl dgst -sha256 -binary $(NON_SECURE_KERNEL) > $(NON_SECURE_KERNEL).sha256
	openssl pkeyutl -sign -in $(NON_SECURE_KERNEL).sha256 -passin env:BOOT_IMAGE_SIGNING_PASSWORD -inkey $(OSKR_BOOT_KEY) -out $(NON_SECURE_KERNEL).sig -pkeyopt digest:sha256 -pkeyopt rsa_padding_mode:pkcs1
	dd if=/dev/zero of=$(NON_SECURE_KERNEL).sig.padded bs=2048 count=1 > /dev/null 2> /dev/null
	dd if=$(NON_SECURE_KERNEL).sig of=$(NON_SECURE_KERNEL).sig.padded conv=notrunc > /dev/null 2> /dev/null
	cat $(NON_SECURE_KERNEL) $(NON_SECURE_KERNEL).sig.padded > $(PROD_KERNEL)

orangejuice: $(BOOT_GZ_ENC) $(BOOT_STATS)
	scp $(BOOT_GZ_ENC) $(BOOT_STATS) vmosAlpha.ankicore.com:/var/www/html/

devsign: $(NON_SECURE_KERNEL)
	echo "Dev-signing OTA..."
	openssl dgst -sha256 -binary $(NON_SECURE_KERNEL) > $(NON_SECURE_KERNEL).sha256
	openssl pkeyutl -sign -in $(NON_SECURE_KERNEL).sha256 -inkey $(DEV_BOOT_KEY) -out $(NON_SECURE_KERNEL).sig -pkeyopt digest:sha256 -pkeyopt rsa_padding_mode:pkcs1
	dd if=/dev/zero of=$(NON_SECURE_KERNEL).sig.padded bs=2048 count=1 > /dev/null 2> /dev/null
	dd if=$(NON_SECURE_KERNEL).sig of=$(NON_SECURE_KERNEL).sig.padded conv=notrunc > /dev/null 2> /dev/null
	cat $(NON_SECURE_KERNEL) $(NON_SECURE_KERNEL).sig.padded > $(PROD_KERNEL)


resign-dev: $(BUILD)/$(BOOT_STEM).img
	truncate --size=-2048 $<
	openssl dgst -sha256 -binary $< > $<.sha256
	openssl pkeyutl -sign -in $<.sha256 -inkey $(DEV_BOOT_KEY) -out $<.sig -pkeyopt digest:sha256 -pkeyopt rsa_padding_mode:pkcs1
	dd if=/dev/zero of=$<.sig.padded bs=2048 count=1
	dd if=$<.sig of=$<.sig.padded conv=notrunc
	cat $< $<.sig.padded > $<.signed
	mv $<.signed $<
	rm -rf $<.sha256 $<.sig $<.sig.padded

# Tasks to check BOOT parititon has expected signature

verify-boot-oskr:
	openssl pkeyutl -verify -in  $(NON_SECURE_KERNEL).sha256  -sigfile $(NON_SECURE_KERNEL).sig -inkey $(OSKR_BOOT_KEY).pub -pubin -pkeyopt digest:sha256 -pkeyopt rsa_padding_mode:pkcs1 | grep "Signature Verified Successfully"
	echo "BOOT CHECK PASSED. CORRECT IMG SIGNATURE"

verify-boot-prod:
	openssl pkeyutl -verify -in  $(NON_SECURE_KERNEL).sha256  -sigfile $(NON_SECURE_KERNEL).sig -inkey $(BOOT_KEY).pub -pubin -pkeyopt digest:sha256 -pkeyopt rsa_padding_mode:pkcs1 | grep "Signature Verified Successfully"
	echo "BOOT CHECK PASSED. CORRECT IMG SIGNATURE"

verify-boot-dev:
	openssl pkeyutl -verify -in  $(NON_SECURE_KERNEL).sha256  -sigfile $(NON_SECURE_KERNEL).sig -inkey vble-qti.key.pub -pubin -pkeyopt digest:sha256 -pkeyopt rsa_padding_mode:pkcs1 | grep "Signature Verified Successfully"
	echo "BOOT CHECK PASSED. CORRECT IMG SIGNATURE"

