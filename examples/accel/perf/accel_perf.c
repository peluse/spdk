/*-
 *   BSD LICENSE
 *
 *   Copyright (c) Intel Corporation.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "spdk/stdinc.h"
#include "spdk/thread.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/string.h"

struct user_config {
	int xfer_size_bytes;
	int queue_depth;
	int time_in_sec;
	bool verify;
	char *core_mask;
	int ioat_chan_num;
};

static struct user_config g_user_config;

static void
construct_user_config(struct user_config *self)
{
	self->xfer_size_bytes = 4096;
	self->ioat_chan_num = 1;
	self->queue_depth = 256;
	self->time_in_sec = 10;
	self->verify = false;
	self->core_mask = "0x1";
}

static void
dump_user_config(struct user_config *self)
{
	printf("User configuration:\n");
	printf("Number of channels:    %u\n", self->ioat_chan_num);
	printf("Transfer size:  %u bytes\n", self->xfer_size_bytes);
	printf("Queue depth:    %u\n", self->queue_depth);
	printf("Run time:       %u seconds\n", self->time_in_sec);
	printf("Core mask:      %s\n", self->core_mask);
	printf("Verify:         %s\n\n", self->verify ? "Yes" : "No");
}

static void
usage(void)
{
	printf("accel_perf options:\n");
	printf("\t[-h help message]\n");
	printf("\t[-c core mask for distributing operations\n");
	printf("\t[-q queue depth]\n");
	printf("\t[-n number of channels]\n");
	printf("\t[-o transfer size in bytes]\n");
	printf("\t[-t time in seconds]\n");
	printf("\t[-v verify copy result if this switch is on]\n");
}

static int
parse_args(int argc, char *argv)
{
	int op;

	construct_user_config(&g_user_config);
	while ((op = getopt(argc, &argv, "c:hn:o:q:t:v")) != -1) {
		switch (op) {
		case 'o':
			g_user_config.xfer_size_bytes = spdk_strtol(optarg, 10);
			break;
		case 'n':
			g_user_config.ioat_chan_num = spdk_strtol(optarg, 10);
			break;
		case 'q':
			g_user_config.queue_depth = spdk_strtol(optarg, 10);
			break;
		case 't':
			g_user_config.time_in_sec = spdk_strtol(optarg, 10);
			break;
		case 'c':
			g_user_config.core_mask = optarg;
			break;
		case 'v':
			g_user_config.verify = true;
			break;
		case 'h':
			usage();
			exit(0);
		default:
			usage();
			return 1;
		}
	}
	if (g_user_config.xfer_size_bytes <= 0 || g_user_config.queue_depth <= 0 ||
	    g_user_config.time_in_sec <= 0 || !g_user_config.core_mask ||
	    g_user_config.ioat_chan_num <= 0) {
		usage();
		return 1;
	}

	dump_user_config(&g_user_config);
	return 0;
}

static void
accel_perf_start(void *arg1)
{

	SPDK_NOTICELOG("Successfully started the application\n");

}

int
main(int argc, char **argv)
{
	struct spdk_app_opts opts = {};
	int rc = 0;

	/* Set default values in opts structure. */
	spdk_app_opts_init(&opts);


	if ((rc = spdk_app_parse_args(argc, argv, &opts, "b:", NULL, parse_args,
				      usage)) != SPDK_APP_PARSE_ARGS_SUCCESS) {
		exit(rc);
	}


	rc = spdk_app_start(&opts, accel_perf_start, NULL);
	if (rc) {
		SPDK_ERRLOG("ERROR starting application\n");
	}

	spdk_app_fini();
	return rc;
}
