#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include "common.h"

struct prop {
    char key[MAX_KEY];
    char val[MAX_VAL];
};

static struct prop *props = NULL;
static size_t prop_count = 0;

static void load_build_prop(void) {
    FILE *f = fopen("/build.prop", "r");
    if (!f) return;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = 0;
        char *key = line;
        char *val = eq + 1;

        key[strcspn(key, "\r\n")] = 0;
        val[strcspn(val, "\r\n")] = 0;

        props = realloc(props, sizeof(*props) * (prop_count + 1));
        strncpy(props[prop_count].key, key, MAX_KEY - 1);
        strncpy(props[prop_count].val, val, MAX_VAL - 1);
        prop_count++;
    }

    fclose(f);
}

static const char *get_prop(const char *key) {
    for (size_t i = 0; i < prop_count; i++) {
        if (strcmp(props[i].key, key) == 0)
            return props[i].val;
    }
    return "";
}

static void set_prop(const char *key, const char *val) {
    for (size_t i = 0; i < prop_count; i++) {
        if (strcmp(props[i].key, key) == 0) {
            strncpy(props[i].val, val, MAX_VAL - 1);
            return;
        }
    }

    props = realloc(props, sizeof(*props) * (prop_count + 1));
    strncpy(props[prop_count].key, key, MAX_KEY - 1);
    strncpy(props[prop_count].val, val, MAX_VAL - 1);
    prop_count++;
}

int main(void) {
    unlink(SOCKET_PATH);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return 1;

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        return 1;

    listen(fd, 5);

    load_build_prop();

    for (;;) {
        int c = accept(fd, NULL, NULL);
        if (c < 0) continue;

        char line[MAX_LINE] = {0};
        read(c, line, sizeof(line) - 1);

        char *cmd = strtok(line, " \n");
        if (!cmd) {
            close(c);
            continue;
        }

        if (strcmp(cmd, "get") == 0) {
            char *key = strtok(NULL, " \n");
            if (!key) {
                write(c, "\n", 1);
            } else {
                const char *val = get_prop(key);
                write(c, val, strlen(val));
                write(c, "\n", 1);
            }
        } else if (strcmp(cmd, "set") == 0) {
            char *key = strtok(NULL, " \n");
            char *val = strtok(NULL, "\n");
            if (key && val)
                set_prop(key, val);
            write(c, "\n", 1);
        }

        close(c);
    }
}
