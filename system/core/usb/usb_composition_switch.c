/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <signal.h>

#define COMMAND_SIZE sizeof("usb_composition 9060 n n y y")

int main(int argc, char **argv) {

        /* Our process ID and Session ID */
        pid_t pid, sid;
        char command[COMMAND_SIZE];

        if (argc < 5) {
            exit(EXIT_FAILURE);
        }

        /* E.g. "usb_composition 9060 n n y n" to switch to composition 9060*/
        snprintf(command, COMMAND_SIZE, "usb_composition %s %s %s y y",
			argv[1], argv[2], argv[3]);

        pid = fork();
        if (pid < 0) {
            exit(EXIT_FAILURE);
        }

        /* Let parent process exit after disabling USB */
        if (pid > 0) {
            system(command);
            exit(EXIT_SUCCESS);
        }

        if (pid == 0) {

            umask(0);

            sid = setsid();
            if (sid < 0) {
                exit(EXIT_FAILURE);
            }

            if ((chdir("/")) < 0) {
                exit(EXIT_FAILURE);
            }

            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            /* Let child process execute remaining usb_composition switch */
            system(command);
            exit(EXIT_SUCCESS);
    }

}
