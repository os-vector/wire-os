/*
 * Copyright (c) 2011,2013 The Linux Foundation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions are
 *   met:
 *     * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/input.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <errno.h>
#include <linux/reboot.h>
#include <libgen.h>
#include <mtd/mtd-user.h>

#define KEY_INPUT_DEVICE "/dev/input/event0"
#define SHUTDOWN_COMMAND "/sbin/shutdown"
#define REBOOT_COMMAND "/sbin/reboot"
#define USEC_IN_SEC 1000000
#define POWER_NODE "/sys/power/state"
#define BUFFER_SZ 32
#define SUSPEND_STRING "mem"
#define RESUME_STRING "on"
#define POWER_OFF_TIMER 1000000

/* Finds and opens an mtdchar device with the given partition name. Returns a
   valid file desciptor or -1 on failure. */
static int mtd_open_partition_name(const char *partition_name, unsigned int *dev_id,
                                   unsigned int *size, unsigned int *erasesize)
{
   int mtd_fd = -1, procfs_fd;
   int num_matches;
   unsigned char match_found = 0;
   size_t partition_name_len, bytes_read, offset = 0;
   char name[32];
   char buf[1024];

   if (!partition_name || !dev_id || !size || !erasesize)
   {
      fprintf(stderr, "Unexpected NULL ptr\n");
   }
   else if ((procfs_fd = open("/proc/mtd", O_RDONLY)) < 0)
   {
      perror("open");
   }
   else
   {
      bytes_read = read(procfs_fd, buf, sizeof(buf) - 1);
      if (bytes_read < 0)
      {
         perror("read");
         bytes_read = 0;
      }
      (void) close(procfs_fd);

      buf[bytes_read] = '\0';
      partition_name_len = strlen(partition_name) + 1;
      memset(name, 0, sizeof(name));
      while (offset < bytes_read &&
             (num_matches = sscanf(&buf[offset], "mtd%u: %x %x \"%31[^\"]",
                                   dev_id, size, erasesize, name)) != EOF)
      {
         if (num_matches == 4 &&
             strncmp(name, partition_name, partition_name_len) == 0)
         {
            match_found = 1;
            break;
         }
         else
         {
            while (buf[offset++] != '\n' && offset < bytes_read);
         }
      }

      if (!match_found)
      {
         fprintf(stderr, "Couldn't find partition with name \"%s\"",
                 partition_name);
      }
      else
      {
         (void) snprintf(name, sizeof(name), "/dev/mtd%u", *dev_id);
         mtd_fd = open(name, O_RDWR);
         if (mtd_fd < 0)
         {
            perror("open");
         }
      }
   }

   return mtd_fd;
}

/* Sets offset to start of first good block. Returns -1 on error, 0 on success */
static int mtd_find_first_good_block(int fd, loff_t *offset, unsigned int size,
                                     unsigned int erasesize)
{
   int ret = -1;

   if (!offset || erasesize == 0)
   {
      fprintf(stderr, "Bad parameters\n");
   }
   else
   {
      *offset = 0;
      while (*offset < size)
      {
         int ret = ioctl(fd, MEMGETBADBLOCK, offset);
         if (ret == 1)
         {
            printf("skipping bad block at offset 0x%llx\n", *offset);
            *offset += erasesize;
            continue;
         }
         else
         {
            if (ret == -1 && errno != EOPNOTSUPP)
            {
               perror("ioctl (MEMGETBADBLOCK)");
               *offset = size;
            }
            break;
         }
      }

      if (*offset < size)
      {
         ret = 0;
      }
   }

   return ret;
}

/* Erases a full eraseblock at the given offset, then writes one or more pages
   of data to the block. Assumes that data_len is a multiple of the page size
   and is <= erasesize. Returns number of bytes written (0 on failure). */
static unsigned int mtd_write_data(int fd, loff_t offset, const unsigned char *data,
                                   unsigned int data_len, unsigned int erasesize)
{
   unsigned int ret = 0;
   int attempts = 0;
   struct erase_info_user ei;

   memset(&ei, 0, sizeof(ei));
   ei.start = offset;
   ei.length = erasesize;

   do
   {
      if (ioctl(fd, MEMERASE, &ei) < 0)
      {
         perror("ioctl (MEMERASE)");
      }
      else if (lseek(fd, offset, SEEK_SET) != offset)
      {
         perror("lseek");
      }
      else if (write(fd, data, data_len) != data_len)
      {
         perror("write");
      }
      else
      {
         ret = data_len;
         break;
      }
   } while (++attempts < 3);

   return ret;
}

/* Returns the page size (minimum write size) for the given device, or 0 on error. */
static unsigned int mtd_get_page_size(unsigned int dev_id)
{
   int fd;
   char buf[64];
   size_t bytes_read;
   unsigned int page_size = 0;

   (void) snprintf(buf, sizeof(buf), "/sys/class/mtd/mtd%u/subpagesize", dev_id);
   fd = open(buf, O_RDONLY);
   if (fd < 0)
   {
      perror("open");
   }
   else
   {
      bytes_read = read(fd, buf, sizeof(buf) - 1);
      if (bytes_read < 0)
      {
         perror("read");
      }
      else
      {
         buf[bytes_read] = '\0';
         if (sscanf(buf, "%u", &page_size) != 1)
         {
            fprintf(stderr, "Couldn't read page size\n");
            page_size = 0;
         }
      }
      close(fd);
   }

   return page_size;
}

/* Writes a special value to the FOTA NAND partition to let boot know we are
   going to recovery */
static void set_fota_cookie(void)
{
   int fd;
   unsigned int dev_id, size, erasesize, writesize;
   unsigned char *data;
   loff_t offset;

   if ((fd = mtd_open_partition_name("fota", &dev_id, &size, &erasesize)) < 0)
   {
      fprintf(stderr, "Couldn't find fota partition\n");
   }
   else
   {
      if ((writesize = mtd_get_page_size(dev_id)) < 4)
      {
         fprintf(stderr, "Couldn't determine page size! (%d)\n", writesize);
      }
      else if (mtd_find_first_good_block(fd, &offset, size, erasesize) != 0)
      {
         fprintf(stderr, "Couldn't find good block in partition\n");
      }
      else
      {
         data = malloc(writesize);
         if (data == NULL)
         {
            fprintf(stderr, "Couldn't allocate %d bytes\n", erasesize);
         }
         else
         {
            memset(data, 0, writesize);
            data[0] = 0x43;
            data[1] = 0x53;
            data[2] = 0x64;
            data[3] = 0x64;
            if (mtd_write_data(fd, offset, data, writesize, erasesize) == writesize)
            {
               printf("Successfully wrote FOTA cookie\n");
            }
            free(data);
         }
      }
      (void) close(fd);
   }
}

int diff_timestamps(struct timeval* then, struct timeval* now)
{
   if (now->tv_usec > then->tv_usec)
   {
      now->tv_usec += USEC_IN_SEC;
      now->tv_sec--;
   }
   return (int) (now->tv_sec - then->tv_sec)*USEC_IN_SEC + now->tv_usec - then->tv_usec;
}

void powerapp_shutdown(void)
{
   pid_t pid;
   
   printf("SHUTDOWN\n");
   return;
   pid = fork();
   if (pid == 0)
   {
      execl(SHUTDOWN_COMMAND, SHUTDOWN_COMMAND, NULL);
   }
   // should never reach here
   for(;;);
}

void sys_shutdown_or_reboot(int reboot, char *arg1)
{
   int cmd = LINUX_REBOOT_CMD_POWER_OFF;
   int n = 0;

   if (reboot)
   {
      if (arg1)
          cmd = LINUX_REBOOT_CMD_RESTART2;
      else
          cmd = LINUX_REBOOT_CMD_RESTART;
   }

   if (cmd == LINUX_REBOOT_CMD_RESTART2 && strncmp(arg1, "recovery", 9) == 0)
   {
      set_fota_cookie();
   }

   n = syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, cmd, arg1);
   if (n < 0)
   {
      fprintf(stderr, "reboot system call failed %d (%s)\n", errno, strerror(errno));
   }
}

void suspend_or_resume(void)
{
   int fd = -1;
   char buf[BUFFER_SZ];
   static int suspend = 1;

   printf("Power Key Initiated System Suspend or Resume\n");

   fd = open(POWER_NODE, O_WRONLY);

   if (fd > 0)
   {
     if (suspend == 1)
     {
       strcpy (buf, SUSPEND_STRING);
       errno = 0;
       if (write(fd, buf, strlen(buf)) == -1)
       {
           printf("Suspend failed %d (%s)\n", errno, strerror(errno));
       }
       suspend = 0;
      }
      else
      {
        suspend = 1;
      }
    }

    close(fd);
}


int
main(int argc, char *argv[])
{
   int fd = 0;
   struct input_event ev;
   struct timeval then;
   struct timeval now;
   int n = 0;
   int duration = 0;
   char *arg1 = NULL;
   char *cmd_name = basename(argv[0]);
   if(argc > 1)
	   arg1 = argv[1];

   if (!strcmp(cmd_name, "sys_reboot"))
   {
      sys_shutdown_or_reboot(1, arg1);
      return 1;
   }
   else if (!strcmp(cmd_name, "sys_shutdown"))
   {
      sys_shutdown_or_reboot(0, arg1);
      return 2;
   }
   fd = open(KEY_INPUT_DEVICE, O_RDONLY);
   if (fd == -1)
   {
      fprintf(stderr, "%s: cannot open input device %s\n", argv[0], KEY_INPUT_DEVICE);
      exit(1);
   }

   memset(&then, 0, sizeof(struct timeval));
   memset(&now, 0, sizeof(struct timeval));

   while ((n = read(fd, &ev, sizeof(struct input_event))) > 0) {
      if (n < sizeof(struct input_event))
      {
	 fprintf(stderr, "%s: cannot read whole input event\n", argv[0]);
	 exit(2);
      }

      if (ev.type == EV_KEY && ev.code == KEY_POWER && ev.value == 1)
      {
	 memcpy(&then, &ev.time, sizeof(struct timeval));
      }
      else if (ev.type == EV_KEY && ev.code == KEY_POWER && ev.value == 0)
      {
	 memcpy(&now, &ev.time, sizeof(struct timeval));
	 duration = diff_timestamps(&then, &now);
	 if (duration > POWER_OFF_TIMER)
	 {
	    powerapp_shutdown();
	 }
	 else
	 {
	    suspend_or_resume();
	 }
      }

/*      printf("%d.%06d: type=%d code=%d value=%d\n",
	     (int) ev.time.tv_sec, (int) ev.time.tv_usec,
	     (int) ev.type, ev.code, (int) ev.value); */
   }

   return 0;
}
