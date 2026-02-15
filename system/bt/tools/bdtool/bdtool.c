/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "btcore/include/bdaddr.h"
#include "btcore/include/property.h"
#include "osi/include/osi.h"
#include "test/suite/support/callbacks.h"
#include "test/suite/support/hal.h"

static const bt_uuid_t HFP_UUID = {{ 0x00, 0x00, 0x11, 0x1E, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB }};
static const bt_uuid_t HFP_AG_UUID = {{ 0x00, 0x00, 0x11, 0x1F, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB }};

const bt_interface_t *bt_interface;

bt_bdaddr_t bt_remote_bdaddr;

static int f_verbose;
static bool discover = false;
static bool discoverable = false;
static bool bond = false;
static bool up = false;
static bool get_name = false;
static bool set_name = false;
static bool sco_listen = false;
static bool sco_connect = false;

static int timeout_in_sec = 30;
static char *bd_name;

static struct option long_options[] = {
  {"bdaddr", required_argument, 0, 0 },
  {"discover", no_argument, 0, 0 },
  {"discoverable", no_argument, 0, 0 },
  {"time", required_argument, 0, 0 },
  {"bond", no_argument, 0, 0 },
  {"up", no_argument, 0, 0 },
  {"verbose", no_argument, 0, 0 },
  {"get_name", no_argument, 0, 0 },
  {"set_name", required_argument, 0, 0 },
  {"sco_listen", no_argument, 0, 0 },
  {"sco_connect", no_argument, 0, 0 },
  {0, 0, 0, 0 }
};

static void usage(const char *name);
static bool parse_args(int argc, char **argv);
static void sig_handler(int signo);

bt_property_t *adapter_get_property(bt_property_type_t type);

int main(int argc, char **argv) {
  if (!parse_args(argc, argv)) {
    usage(argv[0]);
  }

  if (bond && discoverable) {
    fprintf(stderr, "Can only select either bond or discoverable, not both\n");
    usage(argv[0]);
  }

  if (sco_listen && sco_connect) {
    fprintf(stderr, "Can only select either sco_listen or sco_connect, not both\n");
    usage(argv[0]);
  }

  if (!bond && !discover && !discoverable && !up && !get_name && !set_name && !sco_listen && !sco_connect) {
    fprintf(stderr, "Must specify one command\n");
    usage(argv[0]);
  }

  if (signal(SIGINT, sig_handler) == SIG_ERR) {
    fprintf(stderr, "Will be unable to catch signals\n");
  }

  fprintf(stdout, "Bringing up bluetooth adapter\n");
  if (!hal_open(callbacks_get_adapter_struct())) {
    fprintf(stderr, "Unable to open Bluetooth HAL.\n");
    return 1;
  }

  if (discover) {
    CALL_AND_WAIT(bt_interface->enable(false), adapter_state_changed);
    fprintf(stdout, "BT adapter is up\n");

    fprintf(stdout, "Starting to start discovery\n");
    CALL_AND_WAIT(bt_interface->start_discovery(), discovery_state_changed);
    fprintf(stdout, "Started discovery for %d seconds\n", timeout_in_sec);

    sleep(timeout_in_sec);

    fprintf(stdout, "Starting to cancel discovery\n");
    CALL_AND_WAIT(bt_interface->cancel_discovery(), discovery_state_changed);
    fprintf(stdout, "Cancelled discovery after %d seconds\n", timeout_in_sec);
  }

  if (discoverable) {
    CALL_AND_WAIT(bt_interface->enable(false), adapter_state_changed);
    fprintf(stdout, "BT adapter is up\n");

    bt_property_t *property = property_new_scan_mode(BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);

    int rc = bt_interface->set_adapter_property(property);
    fprintf(stdout, "Set rc:%d device as discoverable for %d seconds\n", rc, timeout_in_sec);

    sleep(timeout_in_sec);

    property_free(property);
  }

   if (bond) {
    if (bdaddr_is_empty(&bt_remote_bdaddr)) {
      fprintf(stderr, "Must specify a remote device address [ --bdaddr=xx:yy:zz:aa:bb:cc ]\n");
      exit(1);
    }

    CALL_AND_WAIT(bt_interface->enable(false), adapter_state_changed);
    fprintf(stdout, "BT adapter is up\n");

    int rc = bt_interface->create_bond(&bt_remote_bdaddr, 0 /* UNKNOWN; Currently not documented :( */);
    fprintf(stdout, "Started bonding:%d for %d seconds\n", rc, timeout_in_sec);

    sleep(timeout_in_sec);
  }

  if (up) {
    CALL_AND_WAIT(bt_interface->enable(false), adapter_state_changed);
    fprintf(stdout, "BT adapter is up\n");

    fprintf(stdout, "Waiting for %d seconds\n", timeout_in_sec);
    sleep(timeout_in_sec);
  }

  if (get_name) {
    CALL_AND_WAIT(bt_interface->enable(false), adapter_state_changed);
    fprintf(stdout, "BT adapter is up\n");
    int error;
    CALL_AND_WAIT(error = bt_interface->get_adapter_property(BT_PROPERTY_BDNAME), adapter_properties);
    if (error != BT_STATUS_SUCCESS) {
      fprintf(stderr, "Unable to get adapter property\n");
      exit(1);
    }
    bt_property_t *property = adapter_get_property(BT_PROPERTY_BDNAME);
    const bt_bdname_t *name = property_as_name(property);
    if (name)
      printf("Queried bluetooth device name:%s\n", name->name);
    else
      printf("No name\n");
  }

  if (set_name) {
    CALL_AND_WAIT(bt_interface->enable(false), adapter_state_changed);
    fprintf(stdout, "BT adapter is up\n");

    bt_property_t *property = property_new_name(bd_name);
    printf("Setting bluetooth device name to:%s\n", bd_name);
    int error;
    CALL_AND_WAIT(error = bt_interface->set_adapter_property(property), adapter_properties);
    if (error != BT_STATUS_SUCCESS) {
      fprintf(stderr, "Unable to set adapter property\n");
      exit(1);
    }
    CALL_AND_WAIT(error = bt_interface->get_adapter_property(BT_PROPERTY_BDNAME), adapter_properties);
    if (error != BT_STATUS_SUCCESS) {
      fprintf(stderr, "Unable to get adapter property\n");
      exit(1);
    }
    property_free(property);
    sleep(timeout_in_sec);
  }

  const int app_uid = 0;

  if (sco_listen) {
    CALL_AND_WAIT(bt_interface->enable(false), adapter_state_changed);
    fprintf(stdout, "BT adapter is up\n");

    bt_property_t *property = property_new_scan_mode(BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
    CALL_AND_WAIT(bt_interface->set_adapter_property(property), adapter_properties);
    property_free(property);

    const btsock_interface_t *sock = bt_interface->get_profile_interface(BT_PROFILE_SOCKETS_ID);

    int rfcomm_fd = INVALID_FD;
    int error = sock->listen(BTSOCK_RFCOMM, "meow", (const uint8_t *)&HFP_AG_UUID, 0, &rfcomm_fd, 0, app_uid);
    if (error != BT_STATUS_SUCCESS) {
      fprintf(stderr, "Unable to listen for incoming RFCOMM socket: %d\n", error);
      exit(1);
    }

    int sock_fd = INVALID_FD;
    error = sock->listen(BTSOCK_SCO, NULL, NULL, 5, &sock_fd, 0, app_uid);
    if (error != BT_STATUS_SUCCESS) {
      fprintf(stderr, "Unable to listen for incoming SCO sockets: %d\n", error);
      exit(1);
    }
    fprintf(stdout, "Waiting for incoming SCO connections...\n");
    sleep(timeout_in_sec);
  }

  if (sco_connect) {
    if (bdaddr_is_empty(&bt_remote_bdaddr)) {
      fprintf(stderr, "Must specify a remote device address [ --bdaddr=xx:yy:zz:aa:bb:cc ]\n");
      exit(1);
    }

    CALL_AND_WAIT(bt_interface->enable(false), adapter_state_changed);
    fprintf(stdout, "BT adapter is up\n");

    const btsock_interface_t *sock = bt_interface->get_profile_interface(BT_PROFILE_SOCKETS_ID);

    int rfcomm_fd = INVALID_FD;
    int error = sock->connect(&bt_remote_bdaddr, BTSOCK_RFCOMM, (const uint8_t *)&HFP_AG_UUID, 0, &rfcomm_fd, 0, app_uid);
    if (error != BT_STATUS_SUCCESS) {
      fprintf(stderr, "Unable to connect to RFCOMM socket: %d.\n", error);
      exit(1);
    }

    WAIT(acl_state_changed);

    fprintf(stdout, "Establishing SCO connection...\n");

    int sock_fd = INVALID_FD;
    error = sock->connect(&bt_remote_bdaddr, BTSOCK_SCO, NULL, 5, &sock_fd, 0, app_uid);
    if (error != BT_STATUS_SUCCESS) {
      fprintf(stderr, "Unable to connect to SCO socket: %d.\n", error);
      exit(1);
    }
    sleep(timeout_in_sec);
  }

  CALL_AND_WAIT(bt_interface->disable(), adapter_state_changed);
  fprintf(stdout, "BT adapter is down\n");
}

static void sig_handler(int signo) {
  if (signo == SIGINT) {
    fprintf(stderr, "Received SIGINT\n");
    CALL_AND_WAIT(bt_interface->disable(), adapter_state_changed);
    fprintf(stderr, "BT adapter is down\n");
    exit(1);
  }
}

static void usage(const char *name) {
  fprintf(stderr, "Usage: %s [--bond|--discover|--discoverable|--up|--sco_listen|--sco_connect] [--bdaddr=<bdaddr>] [--time=<time_in_sec>] --verbose\n", name);
  fprintf(stderr, "     bond: Discover actively advertising devices\n");
  fprintf(stderr, "     discover: Discover actively advertising devices\n");
  fprintf(stderr, "     discoverable: Set into a connectable and discoverable mode\n");
  fprintf(stderr, "     up: Only bring up stack\n");
  fprintf(stderr, "     sco_listen: Listen for incoming SCO connections\n");
  fprintf(stderr, "     sco_connect: Establish a SCO connection with another device\n");
  fprintf(stderr, "     time: Time to hold in the specified mode\n");
  exit(1);
}

static bool parse_args(int argc, char **argv) {
  while (1) {
    int option_index = 0;
    int c = getopt_long_only(argc, argv, "", long_options, &option_index);
    if (c != 0)
      break;

    switch (c) {
      case 0:
        if (option_index == 0) {
          if (!string_to_bdaddr(optarg, &bt_remote_bdaddr)) {
            return false;
          }
        }
        if (option_index == 1) {
          discover = true;
        }
        if (option_index == 2) {
          discoverable = true;
        }
        if (option_index == 3) {
          timeout_in_sec = atoi(optarg);
        }
        if (option_index == 4) {
          bond  = true;
        }
        if (option_index == 5) {
          up = true;
        }
        if (option_index == 6) {
          f_verbose++;
        }
        if (option_index == 7) {
          get_name = true;
        }
        if (option_index == 8) {
          bd_name = (char *)optarg;
          set_name = true;
        }
        if (option_index == 9) {
          sco_listen = true;
        }
        if (option_index == 10) {
          sco_connect = true;
        }
        break;

      default:
        fprintf(stderr, "?? getopt returned character code 0%o ??\n", c);
    }
  }

  if (optind < argc) {
    fprintf(stderr, "non-option ARGV-elements: ");
    while (optind < argc)
      fprintf(stderr, "%s ", argv[optind++]);
    fprintf(stderr, "\n");
    return false;
  }
  return true;
}
