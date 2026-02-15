/**********************************************************************
 *
 *  Copyright (C) 2015 Intel Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 *  implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 **********************************************************************/

#define LOG_TAG "bt_vendor"

#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <poll.h>

#include <sys/socket.h>
#include <sys/ioctl.h>

#include "hci/include/bt_vendor_lib.h"
#include "osi/include/log.h"
#include "osi/include/properties.h"

#define BTPROTO_HCI     1
#define HCI_CHANNEL_USER        1
#define HCI_CHANNEL_CONTROL     3
#define HCI_DEV_NONE    0xffff

#define RFKILL_TYPE_BLUETOOTH   2
#define RFKILL_OP_CHANGE_ALL    3

#define MGMT_OP_INDEX_LIST      0x0003
#define MGMT_EV_INDEX_ADDED     0x0004
#define MGMT_EV_COMMAND_COMP    0x0001
#define MGMT_EV_SIZE_MAX        1024
#define MGMT_EV_POLL_TIMEOUT    3000 /* 3000ms */

#define IOCTL_HCIDEVDOWN        _IOW('H', 202, int)

struct sockaddr_hci {
  sa_family_t    hci_family;
  unsigned short hci_dev;
  unsigned short hci_channel;
};

struct rfkill_event {
  uint32_t idx;
  uint8_t  type;
  uint8_t  op;
  uint8_t  soft, hard;
} __attribute__((packed));

struct mgmt_pkt {
  uint16_t opcode;
  uint16_t index;
  uint16_t len;
  uint8_t data[MGMT_EV_SIZE_MAX];
} __attribute__((packed));

struct mgmt_event_read_index {
  uint16_t cc_opcode;
  uint8_t status;
  uint16_t num_intf;
  uint16_t index[0];
} __attribute__((packed));

static const bt_vendor_callbacks_t *bt_vendor_callbacks;
static unsigned char bt_vendor_local_bdaddr[6];
static int bt_vendor_fd = -1;
static int hci_interface;
static int rfkill_en;
static int bt_hwcfg_en;

static int bt_vendor_init(const bt_vendor_callbacks_t *p_cb,
                          unsigned char *local_bdaddr)
{
  char prop_value[PROPERTY_VALUE_MAX];

  LOG_INFO(LOG_TAG, "%s", __func__);

  if (p_cb == NULL) {
    LOG_ERROR(LOG_TAG, "init failed with no user callbacks!");
    return -1;
  }

  bt_vendor_callbacks = p_cb;

  memcpy(bt_vendor_local_bdaddr, local_bdaddr,
         sizeof(bt_vendor_local_bdaddr));

  osi_property_get("bluetooth.interface", prop_value, "0");

  errno = 0;
  if (memcmp(prop_value, "hci", 3))
    hci_interface = strtol(prop_value, NULL, 10);
  else
    hci_interface = strtol(prop_value + 3, NULL, 10);
  if (errno)
    hci_interface = 0;

  LOG_INFO(LOG_TAG, "Using interface hci%d", hci_interface);

  osi_property_get("bluetooth.rfkill", prop_value, "0");

  rfkill_en = atoi(prop_value);
  if (rfkill_en)
    LOG_INFO(LOG_TAG, "RFKILL enabled");

  bt_hwcfg_en = osi_property_get("bluetooth.hwcfg",
                             prop_value, NULL) > 0 ? 1 : 0;
  if (bt_hwcfg_en)
    LOG_INFO(LOG_TAG, "HWCFG enabled");

  return 0;
}

static int bt_vendor_hw_cfg(int stop)
{
  if (!bt_hwcfg_en)
    return 0;

  if (stop) {
    if (osi_property_set("bluetooth.hwcfg", "stop") < 0) {
      LOG_ERROR(LOG_TAG, "%s cannot stop btcfg service via prop", __func__);
      return 1;
    }
  } else {
    if (osi_property_set("bluetooth.hwcfg", "start") < 0) {
      LOG_ERROR(LOG_TAG, "%s cannot start btcfg service via prop", __func__);
      return 1;
    }
  }
  return 0;
}

static int bt_vendor_wait_hcidev(void)
{
  struct sockaddr_hci addr;
  struct pollfd fds[1];
  struct mgmt_pkt ev;
  int fd;
  int ret = 0;

  LOG_INFO(LOG_TAG, "%s", __func__);

  fd = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
  if (fd < 0) {
    LOG_ERROR(LOG_TAG, "Bluetooth socket error: %s", strerror(errno));
    return -1;
  }

  memset(&addr, 0, sizeof(addr));
  addr.hci_family = AF_BLUETOOTH;
  addr.hci_dev = HCI_DEV_NONE;
  addr.hci_channel = HCI_CHANNEL_CONTROL;

  if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    LOG_ERROR(LOG_TAG, "HCI Channel Control: %s", strerror(errno));
    close(fd);
    return -1;
  }

  fds[0].fd = fd;
  fds[0].events = POLLIN;

  /* Read Controller Index List Command */
  ev.opcode = MGMT_OP_INDEX_LIST;
  ev.index = HCI_DEV_NONE;
  ev.len = 0;

  ssize_t wrote;
  OSI_NO_INTR(wrote = write(fd, &ev, 6));
  if (wrote != 6) {
    LOG_ERROR(LOG_TAG, "Unable to write mgmt command: %s", strerror(errno));
    ret = -1;
    goto end;
  }

  while (1) {
    int n;
    OSI_NO_INTR(n = poll(fds, 1, MGMT_EV_POLL_TIMEOUT));
    if (n == -1) {
      LOG_ERROR(LOG_TAG, "Poll error: %s", strerror(errno));
      ret = -1;
      break;
    } else if (n == 0) {
      LOG_ERROR(LOG_TAG, "Timeout, no HCI device detected");
      ret = -1;
      break;
    }

    if (fds[0].revents & POLLIN) {
      OSI_NO_INTR(n = read(fd, &ev, sizeof(struct mgmt_pkt)));
      if (n < 0) {
        LOG_ERROR(LOG_TAG, "Error reading control channel: %s",
                  strerror(errno));
        ret = -1;
        break;
      }

      if (ev.opcode == MGMT_EV_INDEX_ADDED && ev.index == hci_interface) {
        goto end;
      } else if (ev.opcode == MGMT_EV_COMMAND_COMP) {
        struct mgmt_event_read_index *cc;
        int i;

        cc = (struct mgmt_event_read_index *)ev.data;

        if (cc->cc_opcode != MGMT_OP_INDEX_LIST || cc->status != 0)
          continue;

        for (i = 0; i < cc->num_intf; i++) {
          if (cc->index[i] == hci_interface)
            goto end;
        }
      }
    }
  }

end:
  close(fd);
  return ret;
}

static int bt_vendor_open(void *param)
{
  int (*fd_array)[] = (int (*)[]) param;
  int fd;

  LOG_INFO(LOG_TAG, "%s", __func__);

  fd = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
  if (fd < 0) {
    LOG_ERROR(LOG_TAG, "socket create error %s", strerror(errno));
    return -1;
  }

  (*fd_array)[CH_CMD] = fd;
  (*fd_array)[CH_EVT] = fd;
  (*fd_array)[CH_ACL_OUT] = fd;
  (*fd_array)[CH_ACL_IN] = fd;

  bt_vendor_fd = fd;

  LOG_INFO(LOG_TAG, "%s returning %d", __func__, bt_vendor_fd);

  return 1;
}

static int bt_vendor_close(void *param)
{
  (void)(param);

  LOG_INFO(LOG_TAG, "%s", __func__);

  if (bt_vendor_fd != -1) {
    close(bt_vendor_fd);
    bt_vendor_fd = -1;
  }

  return 0;
}

static int bt_vendor_rfkill(int block)
{
  struct rfkill_event event;
  int fd;

  LOG_INFO(LOG_TAG, "%s", __func__);

  fd = open("/dev/rfkill", O_WRONLY);
  if (fd < 0) {
    LOG_ERROR(LOG_TAG, "Unable to open /dev/rfkill");
    return -1;
  }

  memset(&event, 0, sizeof(struct rfkill_event));
  event.op = RFKILL_OP_CHANGE_ALL;
  event.type = RFKILL_TYPE_BLUETOOTH;
  event.hard = block;
  event.soft = block;

  ssize_t len;
  OSI_NO_INTR(len = write(fd, &event, sizeof(event)));
  if (len < 0) {
    LOG_ERROR(LOG_TAG, "Failed to change rfkill state");
    close(fd);
    return 1;
  }

  close(fd);
  return 0;
}

/* TODO: fw config should thread the device waiting and return immedialty */
static void bt_vendor_fw_cfg(void)
{
  struct sockaddr_hci addr;
  int fd = bt_vendor_fd;

  LOG_INFO(LOG_TAG, "%s", __func__);

  if (fd == -1) {
    LOG_ERROR(LOG_TAG, "bt_vendor_fd: %s", strerror(EBADF));
    goto failure;
  }

  memset(&addr, 0, sizeof(addr));
  addr.hci_family = AF_BLUETOOTH;
  addr.hci_dev = hci_interface;
  addr.hci_channel = HCI_CHANNEL_USER;

  if (bt_vendor_wait_hcidev()) {
    LOG_ERROR(LOG_TAG, "HCI interface (%d) not found", hci_interface);
    goto failure;
  }

  if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    LOG_ERROR(LOG_TAG, "socket bind error %s", strerror(errno));
    goto failure;
  }

  LOG_INFO(LOG_TAG, "HCI device ready");

  bt_vendor_callbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);

  return;

failure:
  LOG_ERROR(LOG_TAG, "Hardware Config Error");
  bt_vendor_callbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
}

static int bt_vendor_op(bt_vendor_opcode_t opcode, void *param)
{
  int retval = 0;

  LOG_INFO(LOG_TAG, "%s op %d", __func__, opcode);

  switch (opcode) {
  case BT_VND_OP_POWER_CTRL:
    if (!rfkill_en || !param)
      break;

    if (*((int *)param) == BT_VND_PWR_ON) {
      retval = bt_vendor_rfkill(0);
      if (!retval)
        retval = bt_vendor_hw_cfg(0);
    } else {
      retval = bt_vendor_hw_cfg(1);
      if (!retval)
        retval = bt_vendor_rfkill(1);
    }

    break;

  case BT_VND_OP_FW_CFG:
    bt_vendor_fw_cfg();
    break;

  case BT_VND_OP_SCO_CFG:
    bt_vendor_callbacks->scocfg_cb(BT_VND_OP_RESULT_SUCCESS);
    break;

  case BT_VND_OP_USERIAL_OPEN:
    retval = bt_vendor_open(param);
    break;

  case BT_VND_OP_USERIAL_CLOSE:
    retval = bt_vendor_close(param);
    break;

  case BT_VND_OP_GET_LPM_IDLE_TIMEOUT:
    *((uint32_t *)param) = 3000;
    retval = 0;
    break;

  case BT_VND_OP_LPM_SET_MODE:
    bt_vendor_callbacks->lpm_cb(BT_VND_OP_RESULT_SUCCESS);
    break;

  case BT_VND_OP_LPM_WAKE_SET_STATE:
    break;

  case BT_VND_OP_SET_AUDIO_STATE:
    bt_vendor_callbacks->audio_state_cb(BT_VND_OP_RESULT_SUCCESS);
    break;

  case BT_VND_OP_EPILOG:
    bt_vendor_callbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
    break;
  }

  LOG_INFO(LOG_TAG, "%s op %d retval %d", __func__, opcode, retval);

  return retval;
}

static void bt_vendor_cleanup(void)
{
  LOG_INFO(LOG_TAG, "%s", __func__);

  bt_vendor_callbacks = NULL;
}

EXPORT_SYMBOL const bt_vendor_interface_t BLUETOOTH_VENDOR_LIB_INTERFACE = {
  sizeof(bt_vendor_interface_t),
  bt_vendor_init,
  bt_vendor_op,
  bt_vendor_cleanup,
};
