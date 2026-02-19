#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "common.h"

int main(int argc, char **argv) {
    if (argc != 2) return 1;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return 1;

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        return 1;

    char buf[MAX_LINE];
    snprintf(buf, sizeof(buf), "get %s\n", argv[1]);
    write(fd, buf, strlen(buf));

    memset(buf, 0, sizeof(buf));
    read(fd, buf, sizeof(buf) - 1);
    printf("%s", buf);

    close(fd);
    return 0;
}
