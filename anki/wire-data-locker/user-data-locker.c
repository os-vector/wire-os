// exit codes:
// 2 = gen fail
// 3/7 = sign fail
// 4 = write fail
// 5 = read fail
// 6 = empty blob

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

// copy of uapi
#include <linux/ioctl.h>
#define VKM_KEYBLOB_LEN		1604
#define VKM_MAX_SIG_LEN		512
#define VKM_MAX_DATA_LEN	256
struct vkm_gen_keyblob {
	uint32_t length;
	uint32_t reserved;
	uint8_t  keyblob[VKM_KEYBLOB_LEN];
};
struct vkm_sign {
	uint32_t keyblob_len;
	uint8_t  keyblob[VKM_KEYBLOB_LEN];
	uint32_t data_len;
	uint8_t  data[VKM_MAX_DATA_LEN];
	uint32_t sig_len;
	uint8_t  sig[VKM_MAX_SIG_LEN];
};
#define VKM_IOC_MAGIC		'V'
#define VKM_IOC_GEN_KEYBLOB	_IOR(VKM_IOC_MAGIC, 1, struct vkm_gen_keyblob)
#define VKM_IOC_SIGN		_IOWR(VKM_IOC_MAGIC, 2, struct vkm_sign)

#define UDL_BLOCK_DEV "/dev/block/bootdevice/by-name/switchboard"
#define UDL_BLOCK_LEN 262144
#define UDL_MAX_KEYBLOB 8192
#define UDL_VERSION 2

struct __attribute__((packed)) udl_info {
	uint8_t  magic[8];	 // "ANKIUDLI"
	uint32_t version;
	uint8_t  has_keyblob;
	uint32_t keyblob_len;
	uint8_t  keyblob[UDL_MAX_KEYBLOB];
};

static const uint8_t kDataToSign[] = {
	'a', 'n', 'k', 'i', 'd', 'a', 't', 'a'
};

static ssize_t readn(int fd, void *buf, size_t n)
{
	size_t left = n;
	char *p = buf;
	while (left) {
		ssize_t r = read(fd, p, left);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (r == 0)
			break;
		left -= r;
		p += r;
	}
	return n - left;
}

static ssize_t writen(int fd, const void *buf, size_t n)
{
	size_t left = n;
	const char *p = buf;
	while (left) {
		ssize_t w = write(fd, p, left);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		left -= w;
		p += w;
	}
	return n;
}

static int udl_read(struct udl_info *info)
{
	int fd = open(UDL_BLOCK_DEV, O_RDONLY);
	if (fd < 0)
		return -1;
	if (lseek(fd, -UDL_BLOCK_LEN, SEEK_END) < 0) {
		close(fd);
		return -1;
	}
	ssize_t got = readn(fd, info, sizeof(*info));
	close(fd);
	if (got < (ssize_t)sizeof(*info))
		return -1;
	if (memcmp(info->magic, "ANKIUDLI", 8) != 0 ||
	    info->version != UDL_VERSION)
		return -1;
	if (info->has_keyblob && info->keyblob_len == 0)
		return -1;
	return 0;
}

static int udl_write(const uint8_t *keyblob, uint32_t len)
{
	uint8_t *block = calloc(1, UDL_BLOCK_LEN);
	if (!block)
		return -1;

	int fd = open(UDL_BLOCK_DEV, O_RDWR);
	if (fd < 0) {
		free(block);
		return -1;
	}
	if (lseek(fd, -UDL_BLOCK_LEN, SEEK_END) < 0)
		goto err;
	if (readn(fd, block, UDL_BLOCK_LEN) < 0)
		goto err;

	struct udl_info *info = (struct udl_info *)block;
	memcpy(info->magic, "ANKIUDLI", 8);
	info->version = UDL_VERSION;
	info->has_keyblob = (len > 0);
	info->keyblob_len = (len > UDL_MAX_KEYBLOB) ? UDL_MAX_KEYBLOB : len;
	memcpy(info->keyblob, keyblob, info->keyblob_len);

	if (lseek(fd, -UDL_BLOCK_LEN, SEEK_END) < 0)
		goto err;
	if (writen(fd, block, UDL_BLOCK_LEN) < 0)
		goto err;
	fsync(fd);
	close(fd);
	free(block);
	return 0;

err:
	close(fd);
	free(block);
	return -1;
}

static int vkm_open(void)
{
	int fd = open("/dev/vector-km", O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "udl-test: open /dev/vector-km: %s\n",
			strerror(errno));
	}
	return fd;
}

static int vkm_gen(int fd, uint8_t *blob_out, uint32_t *blob_len_out)
{
	struct vkm_gen_keyblob gk;
	memset(&gk, 0, sizeof(gk));
	if (ioctl(fd, VKM_IOC_GEN_KEYBLOB, &gk) < 0) {
		fprintf(stderr, "udl-test: VKM_IOC_GEN_KEYBLOB: %s\n",
			strerror(errno));
		return -1;
	}
	if (gk.length != VKM_KEYBLOB_LEN) {
		fprintf(stderr, "udl-test: unexpected keyblob len %u\n",
			gk.length);
		return -1;
	}
	memcpy(blob_out, gk.keyblob, gk.length);
	*blob_len_out = gk.length;
	return 0;
}

static int vkm_sign(int fd, const uint8_t *blob, uint32_t blob_len,
		    const uint8_t *data, uint32_t data_len,
		    uint8_t *sig_out, uint32_t *sig_len_out)
{
	struct vkm_sign s;
	memset(&s, 0, sizeof(s));
	s.keyblob_len = blob_len;
	memcpy(s.keyblob, blob, blob_len);
	s.data_len = data_len;
	memcpy(s.data, data, data_len);
	if (ioctl(fd, VKM_IOC_SIGN, &s) < 0) {
		fprintf(stderr, "udl-test: VKM_IOC_SIGN: %s\n",
			strerror(errno));
		return -1;
	}
	if (s.sig_len == 0 || s.sig_len > sizeof(s.sig)) {
		fprintf(stderr, "udl-test: bad sig_len=%u\n", s.sig_len);
		return -1;
	}
	memcpy(sig_out, s.sig, s.sig_len);
	*sig_len_out = s.sig_len;
	return 0;
}

static void emit_passphrase(const uint8_t *sig, uint32_t len)
{
	for (uint32_t i = 0; i < len; i++)
		printf("%02x", sig[i]);
}

static int cmd_reset(void)
{
	uint8_t blob[VKM_KEYBLOB_LEN];
	uint8_t sig[VKM_MAX_SIG_LEN];
	uint32_t blob_len = 0, sig_len = 0;

	int fd = vkm_open();
	if (fd < 0)
		return 2;
	if (vkm_gen(fd, blob, &blob_len) < 0) {
		close(fd);
		return 2;
	}
	if (vkm_sign(fd, blob, blob_len, kDataToSign, sizeof(kDataToSign),
		     sig, &sig_len) < 0) {
		close(fd);
		return 3;
	}
	close(fd);

	if (udl_write(blob, blob_len) < 0) {
		fprintf(stderr, "failed to persist keyblob\n");
		return 4;
	}
	emit_passphrase(sig, sig_len);
	return 0;
}

static int cmd_read(void)
{
	struct udl_info *info = calloc(1, sizeof(*info));
	uint8_t sig[VKM_MAX_SIG_LEN];
	uint32_t sig_len = 0;
	if (!info)
		return 5;
	if (udl_read(info) < 0) {
		free(info);
		return 5;
	}
	if (!info->has_keyblob || info->keyblob_len != VKM_KEYBLOB_LEN) {
		fprintf(stderr, "no valid keyblob on switchboard\n");
		free(info);
		return 6;
	}
	int fd = vkm_open();
	if (fd < 0) {
		free(info);
		return 7;
	}
	if (vkm_sign(fd, info->keyblob, info->keyblob_len,
		     kDataToSign, sizeof(kDataToSign),
		     sig, &sig_len) < 0) {
		close(fd);
		free(info);
		return 7;
	}
	close(fd);
	free(info);
	emit_passphrase(sig, sig_len);
	return 0;
}

int cmd_help() {
	printf("just 'user-data-locker' will give you the key blob\n");
	printf("'user-data-locker reset' makes new key blob and puts it into switchboard\n");
	printf(":3\n");
	return 0;
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "reset") == 0)
		return cmd_reset();
	if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
		return cmd_help();
	return cmd_read();
}
