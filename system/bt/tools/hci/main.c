#include <hardware/bluetooth.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "osi/include/osi.h"

typedef int (*handler_t)(int argc, char **argv);

typedef enum {
  HCI_PACKET_COMMAND  = 1,
  HCI_PACKET_ACL_DATA = 2,
  HCI_PACKET_SCO_DATA = 3,
  HCI_PACKET_EVENT    = 4,
} hci_packet_t;

typedef struct {
  const char *name;
  const char *help;
  handler_t handler;
} command_t;

static int help(int argc, char **argv);
static int set_discoverable(int argc, char **argv);
static int set_name(int argc, char **argv);
static int set_pcm_loopback(int argc, char **argv);
static int set_sco_route(int argc, char **argv);

static bool write_hci_command(hci_packet_t type, const void *packet, size_t length);
static const command_t *find_command(const char *name);
static void usage(const char *name);

static const command_t commands[] = {
  { "help", "<command> - shows help text for <command>.", help },
  { "setDiscoverable", "(true|false) - whether the controller should be discoverable.", set_discoverable },
  { "setName", "<name> - sets the device's Bluetooth name to <name>.", set_name },
  { "setPcmLoopback", "(true|false) - enables or disables PCM loopback on the controller.", set_pcm_loopback },
  { "setScoRoute", "(pcm|i2s|uart) - sets the SCO packet route to one of the specified buses.", set_sco_route },
};

static int help(int argc, char **argv) {
  if (!argc) {
    printf("No help command specified.\n");
    return 1;
  }

  const command_t *command = find_command(argv[0]);
  if (!command) {
    printf("No command named '%s'.\n", argv[0]);
    return 2;
  }

  printf("%s %s\n", argv[0], command->help);
  return 0;
}

static int set_discoverable(int argc, char **argv) {
  if (argc != 1) {
    printf("Discoverable mode not specified.\n");
    return 1;
  }

  if (strcmp(argv[0], "true") && strcmp(argv[0], "false")) {
    printf("Invalid discoverable mode '%s'.\n", argv[0]);
    return 2;
  }

  uint8_t packet[] = { 0x1A, 0x0C, 0x01, 0x00 };
  if (argv[0][0] == 't')
    packet[ARRAY_SIZE(packet) - 1] = 0x03;

  return !write_hci_command(HCI_PACKET_COMMAND, packet, ARRAY_SIZE(packet));
}

static int set_name(int argc, char **argv) {
  if (argc != 1) {
    printf("Device name not specified.\n");
    return 1;
  }

  size_t len = strlen(argv[0]);
  if (len > 247) {
    printf("Device name cannot exceed 247 bytes.\n");
    return 2;
  }

  uint8_t packet[251] = { 0x13, 0x0C, 248 };
  memcpy(&packet[3], argv[0], len + 1);

  if (!write_hci_command(HCI_PACKET_COMMAND, packet, sizeof(packet)))
    return 1;

  memset(&packet[0], 0, sizeof(packet));
  packet[0] = 0x52;
  packet[1] = 0x0C;
  packet[2] = 0xF1;  // HCI command packet length.
  packet[3] = 0x01;  // FEC required.
  packet[4] = len + 1;
  packet[5] = 0x09;  // Device name field tag.
  memcpy(&packet[6], argv[0], len);
  return !write_hci_command(HCI_PACKET_COMMAND, packet, 0xF4);
}

static int set_pcm_loopback(int argc, char **argv) {
  if (argc != 1) {
    printf("PCM loopback mode not specified.\n");
    return 1;
  }

  if (strcmp(argv[0], "true") && strcmp(argv[0], "false")) {
    printf("Invalid PCM mode '%s'.\n", argv[0]);
    return 2;
  }

  uint8_t packet[] = { 0x24, 0xFC, 0x01, 0x00 };
  if (argv[0][0] == 't')
    packet[ARRAY_SIZE(packet) - 1] = 0x01;

  return !write_hci_command(HCI_PACKET_COMMAND, packet, ARRAY_SIZE(packet));
}

static int set_sco_route(int argc, char **argv) {
  if (argc != 1) {
    printf("SCO route parameter must be specified.\n");
    return 1;
  }

  uint8_t route = 0xFF;
  if (!strcmp(argv[0], "pcm"))
    route = 0;
  else if (!strcmp(argv[0], "i2s"))
    route = 3;
  else if (!strcmp(argv[0], "uart"))
    route = 1;

  if (route == 0xFF) {
    printf("Invalid SCO route specified: %s\n", argv[0]);
    return 2;
  }

  uint8_t packet[] = { 0x1C, 0xFC, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00 };
  packet[3] = route;

  return !write_hci_command(HCI_PACKET_COMMAND, packet, ARRAY_SIZE(packet));
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(argv[0]);
    return -1;
  }

  const command_t *command = find_command(argv[1]);
  if (!command) {
    printf("Unrecognized command '%s'.\n", argv[1]);
    return -2;
  }

  if (!command->handler) {
    printf("Unhandled command '%s'.\n", argv[1]);
    return -3;
  }

  return command->handler(argc - 2, &argv[2]);
}

static bool write_hci_command(hci_packet_t type, const void *packet, size_t length) {
  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock == INVALID_FD)
    goto error;

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(0x7F000001);
  addr.sin_port = htons(8873);
  int ret;
  OSI_NO_INTR(ret = connect(sock, (const struct sockaddr *)&addr,
                            sizeof(addr)));
  if (ret == -1)
    goto error;

  if (send(sock, &type, 1, 0) != 1)
    goto error;

  if (send(sock, &length, 2, 0) != 2)
    goto error;

  if (send(sock, packet, length, 0) != (ssize_t)length)
    goto error;

  close(sock);
  return true;

error:;
  close(sock);
  return false;
}

static const command_t *find_command(const char *name) {
  for (size_t i = 0; i < ARRAY_SIZE(commands); ++i)
    if (!strcmp(commands[i].name, name))
      return &commands[i];
  return NULL;
}

static void usage(const char *name) {
  printf("Usage: %s <command> [options]\n", name);
  printf("Commands:\n");
  for (size_t i = 0; i < ARRAY_SIZE(commands); ++i)
    printf("  %s\n", commands[i].name);
  printf("For detailed help on a command, run '%s help <command>'.\n", name);
}
