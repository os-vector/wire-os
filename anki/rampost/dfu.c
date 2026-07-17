#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <string.h>

#include "messages.h"
#include "spine_hal.h"
#include "rampost.h"
#include "das.h"

#define VERSTRING_LEN 16

#define FRAME_WAIT_MS 500 //todo hoist to rampost.h
#define ERASE_WAIT_MS  5000 //up to 5 sec for erase
#define DFU_PACKET_WAIT_MS 1000 //up to 1 sec for write
#define VALIDATE_WAIT_MS  5000 //up to 5 sec for validate

enum VersionStatus {
  version_UNKNOWN,
  version_OLDER,
  version_OK,
  version_NEWER,
};


FILE* gImgFilep = NULL;


void dfu_cleanup(void)
{
  if (gImgFilep) {
    fclose(gImgFilep);
    gImgFilep = NULL;
  }
}

void show_error(RampostErr err)
{
  DAS_LOG(DAS_ERROR, "dfu.error", "%d", err);
}


static void DAS_PRINTHEX(const int level, const char* event, const uint8_t* bytes, int len)
{
  if (level >= DEBUG_LEVEL) {
    const long long log_time = uptime_ms();
    int i;
    printf("\n@rampost.%s\x1f", event);
    for (i = 0; i < len; i++) {
      printf("%02x", bytes[i]);
    }
    printf("\x1f");
    for (i = 0; i < len; i++) {
      printf("%c", isprint(bytes[i]) ? bytes[i] : '*');
    }
    printf("\x1f\x1f\x1f\x1f\x1f\x1f\x1f%lld\n", log_time);
  }
}

enum VersionStatus  CheckVersion(const uint8_t desiredVersion[], const uint8_t firmwareVersion[])
{
  DAS_PRINTHEX(DAS_EVENT, "dfu.installed_version", firmwareVersion, VERSTRING_LEN);
  DAS_PRINTHEX(DAS_EVENT, "dfu.desired_version  ", desiredVersion, VERSTRING_LEN);

  uint8_t i;
  for (i = 0; i < VERSTRING_LEN; i++) {
    if (desiredVersion[i] != firmwareVersion[i]) {
      return (desiredVersion[i] < firmwareVersion[i]) ? version_NEWER : version_OLDER;
    }
  }
  return version_OK;  // all digits same
}


int IsGoodAck(struct AckMessage* msg)
{
  if (msg->status <= 0) {
    DAS_LOG(DAS_DEBUG, "dfu.nack", "%d", msg->status);
    return 0;
  }
  return 1;
}


bool SendData(const struct WriteDFU* packet)
{

  const struct SpineMessageHeader* hdr;

  //wait for boot frame to be sure we are ready to continue
  hdr = hal_wait_for_frame(PAYLOAD_BOOT_FRAME, FRAME_WAIT_MS);
  if (!hdr) {
    //No boot frames means something wrong
    DAS_LOG(DAS_ERROR, "dfu.send_data_noframe", "Not seeing boot frames");
    return false;
  }

  hal_send_frame(PAYLOAD_DFU_PACKET, packet, sizeof(struct WriteDFU));

  hdr = hal_wait_for_frame(PAYLOAD_ACK, DFU_PACKET_WAIT_MS);
  if (!hdr) {
    DAS_LOG(DAS_ERROR, "dfu.send_data_timeout", "No response to DFU_PACKET");
    return false;
  }
  else if (!IsGoodAck((struct AckMessage*)(hdr + 1))) {
    //Noone should send NACK
    DAS_LOG(DAS_ERROR, "dfu.send_data_nak", "Got NACK %d in response",
            ((struct AckMessage*)(hdr + 1))->status);
    return false;
  }
  return true;
}


static const char* const ERASED_INDICATOR = "-----Erased-----";
static uint8_t gInstalledVersion[VERSTRING_LEN] = {0};

const uint8_t* dfu_get_version()
{

  DAS_LOG(DAS_EVENT, "dfu.request_version", "...");
  const struct SpineMessageHeader* hdr;

  hal_send_frame(PAYLOAD_VERSION, NULL, 0);

  // Worst case, version should be returned 3 packets later. we're waiting for a full 200 here
  hdr = hal_wait_for_frame_or_nack(PAYLOAD_VERSION, FRAME_WAIT_MS * 2);
  if (hdr) {
    if (hdr->payload_type == PAYLOAD_VERSION) {
      memcpy(gInstalledVersion, ((struct VersionInfo*)(hdr + 1))->app_version, VERSTRING_LEN);
      return gInstalledVersion;
    }
    else {
      DAS_LOG(DAS_WARN, "dfu.request_version.nack", "%d", *(Ack*)(hdr + 1));
    }
  }
  else {
    DAS_LOG(DAS_WARN, "dfu.request_version.no_response", "Assuming erased");
  }

  //Any other response, including NAK from bootloader, we assume erased and continue.
  //If it was not really ready, the next steps will fail.
  memcpy(gInstalledVersion, ERASED_INDICATOR, VERSTRING_LEN);
  return gInstalledVersion;
}

uint8_t versionString[VERSTRING_LEN];

const uint8_t* dfu_open_binary_file(const char* filename, FILE** fpOut)
{
  DAS_LOG(DAS_EVENT, "dfu.open_file", "opening \"%s\"", filename);
  *fpOut = fopen(filename, "rb");
  if (!*fpOut) {
    show_error(err_DFU_FILE_OPEN);
    return NULL;
  }
  size_t nchars = fread(versionString, sizeof(uint8_t), VERSTRING_LEN, *fpOut);
  if (nchars != VERSTRING_LEN) {
    show_error(err_DFU_FILE_READ);
    return NULL;
  }

  return versionString;
}

bool dfu_erase_image(void)
{
  const struct SpineMessageHeader* hdr;

  DAS_LOG(DAS_EVENT, "dfu.erase_image", "erasing installed image");

  hal_send_frame(PAYLOAD_ERASE, NULL, 0);

  hdr = hal_wait_for_frame(PAYLOAD_ACK, ERASE_WAIT_MS);
  if (!hdr) {
    //No ack means something wrong
    DAS_LOG(DAS_ERROR, "dfu.erase_timeout", "Erase timeout");
    return false;
  }
  if (hdr) {
    if (!IsGoodAck((struct AckMessage*)(hdr + 1))) {
      //Should not fail
      DAS_LOG(DAS_ERROR, "dfu.erase_nak", "Got NACK %d in response",
              ((struct AckMessage*)(hdr + 1))->status);
      return false;
    }
  }
  return true;
}

bool dfu_send_image(FILE* imgfile)
{
  struct WriteDFU packet = {0};
  size_t databytes;
  size_t itemcount;
  int start_addr = 0;

  while (!feof(imgfile)) {
    itemcount = fread(&(packet.data[0]), sizeof(uint32_t), 256, imgfile);
    databytes = itemcount * sizeof(uint32_t);
    DAS_LOG(DAS_DEBUG, "dfu.send_image_read", "read %zd words (%zd bytes)", itemcount, databytes);

    if (itemcount < 256 && ferror(imgfile)) {
      show_error(err_DFU_FILE_READ);
      return false;
    }

    if (itemcount  > 0) {
      packet.address = start_addr;
      packet.wordCount = itemcount;

      DAS_LOG(DAS_DEBUG, "dfu.send_image_write", "writing %d words (%zd bytes) for @%x",
              packet.wordCount, databytes, packet.address);

      if (!SendData(&packet)) {
        show_error(err_DFU_SEND);
        return false;
      }
      start_addr += databytes;
    }
    else {
      DAS_LOG(DAS_ERROR, "dfu.send_image_error", "unexpected itemcount of %zd", itemcount);
      return false;
    }
  }
  return true;
}

bool dfu_validate_image(void)
{
  DAS_LOG(DAS_EVENT, "dfu.validate", "...");

  const struct SpineMessageHeader* hdr;

  //wait for boot frame to be sure we are ready to continue
  hdr = hal_wait_for_frame(PAYLOAD_BOOT_FRAME, FRAME_WAIT_MS);
  if (!hdr) {
    //No boot frames means something wrong
    DAS_LOG(DAS_EVENT, "dfu.validate_noframes", "not seeing boot frames");
    return false;
  }
  hal_send_frame(PAYLOAD_VALIDATE, NULL, 0);

  hdr = hal_wait_for_frame(PAYLOAD_ACK, VALIDATE_WAIT_MS);
  if (!hdr) {
    DAS_LOG(DAS_WARN, "dfu.validate_no_ack", "Did not get ACK response");
  }
  else if (!IsGoodAck((struct AckMessage*)(hdr + 1))) {
    //Noone should send NACK
    DAS_LOG(DAS_ERROR, "dfu.validate_nak", "Got NACK %d in response",
            ((struct AckMessage*)(hdr + 1))->status);
    return false;
  }
  //else good ack, but let's wait for a app data frame to be sure
  hdr = hal_wait_for_frame(PAYLOAD_DATA_FRAME, FRAME_WAIT_MS);
  if (!hdr) {
    DAS_LOG(DAS_ERROR, "dfu.validate_timeout", "Did not get app DATA_FRAME");
    return false;
  }
  DAS_LOG(DAS_DEBUG, "dfu.validate_success", "Saw DATA_FRAME");
  //else we are up and running
  return true;
}

RampostErr dfu_sequence(const char* dfu_file, bool force_update)
{
  const uint8_t* installed_version = NULL;
  int tries = 0;
  // try 5 times, accounting for prototype bodyboards
  while (tries < 5 && !installed_version) {
      installed_version = dfu_get_version();
      if (!installed_version) {
          usleep(100000);
          ++tries;
      }
  }
  // ignore DevBuild* but not 2.0's devbuild
  if ((installed_version && strstr((const char*)installed_version, "DevBuild")) && 
      !(installed_version && strstr((const char*)installed_version, "DevBuild9c26f10"))) 
  {
    DAS_PRINTHEX(DAS_EVENT, "dfu.installed_version", installed_version, VERSTRING_LEN);
    DAS_LOG(DAS_EVENT, "dfu.success", "ignoring DevBuild");
    return err_OK;
  }
  if (!installed_version) {
    return err_DFU_NO_VERSION;
  }
  const uint8_t* desired_version = dfu_open_binary_file(dfu_file, &gImgFilep);

  if (!force_update && CheckVersion(desired_version, installed_version) >= version_OK) {
    return err_SYSCON_VERSION_GOOD; //no update needed
  }

  if (!dfu_erase_image()) {
    return err_DFU_ERASE_ERROR;
  }

  if (!dfu_send_image(gImgFilep)) {
    return err_DFU_INSTALL_ERROR;
  }

  if (!dfu_validate_image()) {
    return err_DFU_VALIDATE_ERROR;
  }

  DAS_LOG(DAS_EVENT, "dfu.success", "Success");

  return err_OK; //Updated
}
