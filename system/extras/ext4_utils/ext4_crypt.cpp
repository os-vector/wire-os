/*
 * Copyright (c) 2015 Google, Inc.
 */

#define TAG "ext4_utils"

#include "ext4_crypt_init_extensions.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <asm/ioctl.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <cutils/klog.h>

#include "unencrypted_properties.h"

#define XATTR_NAME_ENCRYPTION_POLICY "encryption.policy"
#define EXT4_KEYREF_DELIMITER ((char)'.')

// ext4enc:TODO Include structure from somewhere sensible
// MUST be in sync with ext4_crypto.c in kernel
#define EXT4_KEY_DESCRIPTOR_SIZE 8
struct ext4_encryption_policy {
    char version;
    char contents_encryption_mode;
    char filenames_encryption_mode;
    char flags;
    char master_key_descriptor[EXT4_KEY_DESCRIPTOR_SIZE];
} __attribute__((__packed__));

#define EXT4_ENCRYPTION_MODE_AES_256_XTS    1
#define EXT4_ENCRYPTION_MODE_AES_256_CTS    4

// ext4enc:TODO Get value from somewhere sensible
#define EXT4_IOC_SET_ENCRYPTION_POLICY \
    _IOR('f', 19, struct ext4_encryption_policy)

/* Validate that all path items are available and accessible. */
static int is_path_valid(const char *path)
{
    if (access(path, W_OK)) {
        KLOG_ERROR(TAG, "Can't access %s: %s\n",strerror(errno), path);
        return 0;
    }

    return 1;
}

static int is_dir_empty(const char *dirname)
{
    int n = 0;
    struct dirent *d;
    DIR *dir;

    dir = opendir(dirname);
    while ((d = readdir(dir)) != NULL) {
        if (strcmp(d->d_name, "lost+found") == 0) {
            // Skip lost+found directory
        } else if (++n > 2) {
            break;
        }
    }
    closedir(dir);
    return n <= 2;
}

int do_policy_set(const char *directory, const char *policy, int policy_length)
{
    struct stat st;
    ssize_t ret;

    if (policy_length != EXT4_KEY_DESCRIPTOR_SIZE) {
        KLOG_ERROR("Policy wrong length\n");
        return -EINVAL;
    }

    if (!is_path_valid(directory)) {
        return -EINVAL;
    }

    stat(directory, &st);
    if (!S_ISDIR(st.st_mode)) {
        KLOG_ERROR(TAG, "Can only set policy on a directory (%s)\n", directory);
        return -EINVAL;
    }

    if (!is_dir_empty(directory)) {
        KLOG_ERROR(TAG, "Can only set policy on an empty directory (%s)\n",
                   directory);
        return -EINVAL;
    }

    int fd = open(directory, O_DIRECTORY);
    if (fd == -1) {
        KLOG_ERROR(TAG, "Failed to open directory (%s)\n", directory);
        return -EINVAL;
    }

    ext4_encryption_policy eep;
    eep.version = 0;
    eep.contents_encryption_mode = EXT4_ENCRYPTION_MODE_AES_256_XTS;
    eep.filenames_encryption_mode = EXT4_ENCRYPTION_MODE_AES_256_CTS;
    eep.flags = 0;
    memcpy(eep.master_key_descriptor, policy, EXT4_KEY_DESCRIPTOR_SIZE);
    ret = ioctl(fd, EXT4_IOC_SET_ENCRYPTION_POLICY, &eep);
    auto preserve_errno = errno;
    close(fd);

    if (ret) {
        KLOG_ERROR(TAG, "Failed to set encryption policy for %s: %s\n",
                   directory, strerror(preserve_errno));
        return -EINVAL;
    }

    KLOG_INFO(TAG, "Encryption policy for %s is set to %02x%02x%02x%02x\n",
              directory, policy[0], policy[1], policy[2], policy[3]);
    return 0;
}

bool e4crypt_non_default_key(const char* dir)
{
    UnencryptedProperties props(dir);
    return props.Get<int>(properties::is_default, 1) != 1;
}
