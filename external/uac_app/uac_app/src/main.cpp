/*
 * Copyright 2020 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

#include "uevent.h"
#include "uac_control.h"
#include "uac_log.h"

int enable_minilog    = 0;
char *rockit_interface_type = NULL;
int uac_app_log_level = LOG_LEVEL_DEBUG;
static const char short_options[] = "t:";
static const struct option long_options[] = {
    {"type", required_argument, NULL, 't'},
    {"help", no_argument, NULL, 'h'},
    {0, 0}
};

static void usage_tip(FILE *fp, int argc, char **argv) {
    fprintf(fp, "Usage: %s [options]\n"
                "Version %s\n"
                "Options:\n"
                "-t | --type        select rockit mpi type[mpi/mpi_vqe/graph], default is mpi\n"
                "-h | --help        for help \n\n"
                "\n",
            argv[0], "V1.0");
}

void debug_level_init() {
    char *log_level = getenv("uac_app_log_level");
    if (log_level) {
        uac_app_log_level = atoi(log_level);
    }
}

void rkuac_get_opt(int argc, char *argv[]) {
    for (;;) {
        int idx;
        int c;
        c = getopt_long(argc, argv, short_options, long_options, &idx);
        if (-1 == c)
            break;
        switch (c) {
          case 0: /* getopt_long() flag */
            break;
          case 't':
            rockit_interface_type = optarg;
            break;
          case 'h':
            usage_tip(stdout, argc, argv);
            exit(EXIT_SUCCESS);
          default:
            usage_tip(stderr, argc, argv);
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[])
{
    debug_level_init();
    ALOGI("uac uevent version = 1.0\n");
    // create uac control
    char *ch;
    int type = UAC_API_MPI;
    rkuac_get_opt(argc, argv);
    if (rockit_interface_type) {
        if (strcmp(rockit_interface_type, "graph") == 0) {
            type = UAC_API_GRAPH;
        } else if(strcmp(rockit_interface_type, "mpi") == 0) {
            type = UAC_API_MPI;
        }
    }

    int result = uac_control_create(type);
    if (result < 0) {
        ALOGE("uac_control_create fail\n");
        return 0;
    }

    // register uevent monitor
    uevent_monitor_run();

    while(1) {
        usleep(100000);
    }

    uac_control_destory();
    return 0;
}

