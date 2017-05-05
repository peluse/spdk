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

#include "spdk_cunit.h"

#include "spdk/env.h"

#include "nvme/nvme.c"

#include "lib/test_env.c"
#include <signal.h>

extern int ut_fake_pthread_mutexattr_init;
extern int ut_fake_pthread_mutex_init;
extern int g_nvme_driver_timeout_us;

int
spdk_pci_nvme_enumerate(spdk_pci_enum_cb enum_cb, void *enum_ctx)
{
	return -1;
}

struct spdk_pci_id
spdk_pci_device_get_id(struct spdk_pci_device *pci_dev)
{
	struct spdk_pci_id pci_id;

	memset(&pci_id, 0xFF, sizeof(pci_id));

	return pci_id;
}

bool
spdk_nvme_transport_available(enum spdk_nvme_transport_type trtype)
{
	return true;
}

void *ut_fake_nvme_transport_ctrlr_construct = NULL;
struct spdk_nvme_ctrlr *nvme_transport_ctrlr_construct(const struct spdk_nvme_transport_id *trid,
		const struct spdk_nvme_ctrlr_opts *opts,
		void *devhandle)
{
	return ut_fake_nvme_transport_ctrlr_construct;
}

int
nvme_transport_ctrlr_scan(const struct spdk_nvme_transport_id *trid,
			  void *cb_ctx,
			  spdk_nvme_probe_cb probe_cb,
			  spdk_nvme_remove_cb remove_cb)
{
	return 0;
}

int ut_destruct_flag = 0;

void
nvme_ctrlr_destruct(struct spdk_nvme_ctrlr *ctrlr)
{
	// caller must clear to reset test
	ut_destruct_flag = 1;
}

int
nvme_ctrlr_add_process(struct spdk_nvme_ctrlr *ctrlr, void *devhandle)
{
	return 0;
}

int
nvme_ctrlr_process_init(struct spdk_nvme_ctrlr *ctrlr)
{
	return 0;
}

int
nvme_ctrlr_start(struct spdk_nvme_ctrlr *ctrlr)
{
	return 0;
}

void
nvme_ctrlr_fail(struct spdk_nvme_ctrlr *ctrlr, bool hot_remove)
{
}

void
spdk_nvme_ctrlr_opts_set_defaults(struct spdk_nvme_ctrlr_opts *opts)
{
	memset(opts, 0, sizeof(*opts));
}

struct spdk_pci_addr
spdk_pci_device_get_addr(struct spdk_pci_device *pci_dev)
{
	struct spdk_pci_addr pci_addr;

	memset(&pci_addr, 0, sizeof(pci_addr));
	return pci_addr;
}

int
spdk_pci_addr_compare(const struct spdk_pci_addr *a1, const struct spdk_pci_addr *a2)
{
	return true;
}

void
nvme_ctrlr_proc_get_ref(struct spdk_nvme_ctrlr *ctrlr)
{
	return;
}

void
nvme_ctrlr_proc_put_ref(struct spdk_nvme_ctrlr *ctrlr)
{
	return;
}

int ut_ctrlr_ref_count = 0;

int
nvme_ctrlr_get_ref_count(struct spdk_nvme_ctrlr *ctrlr)
{
	// caller must set to desired value
	return ut_ctrlr_ref_count;
}

static void
test_spdk_nvme_detach(void)
{
	int rc = 1;
	struct spdk_nvme_ctrlr ctrlr;
	struct nvme_driver dummy;
	g_spdk_nvme_driver = &dummy;

	// setup enough global stuff to get coverage
	TAILQ_INIT(&dummy.attached_ctrlrs);
	TAILQ_INSERT_TAIL(&dummy.attached_ctrlrs, &ctrlr, tailq);
	CU_ASSERT_FATAL(pthread_mutex_init(&dummy.lock, NULL) == 0);
	ut_ctrlr_ref_count = 0;

	rc = spdk_nvme_detach(&ctrlr);
	CU_ASSERT(ut_destruct_flag == 1);
	CU_ASSERT(rc == 0);

	// additional branch coverage
	ut_ctrlr_ref_count = 1;
	ut_destruct_flag = 0;
	rc = 1;

	rc = spdk_nvme_detach(&ctrlr);
	CU_ASSERT(ut_destruct_flag == 0);
	CU_ASSERT(rc == 0);
}

static void
test_nvme_completion_poll_cb(void)
{
	struct nvme_completion_poll_status status;
	struct spdk_nvme_cpl cpl;

	memset(&status, 0x0, sizeof(struct nvme_completion_poll_status));
	memset(&cpl, 0xff, sizeof(struct spdk_nvme_cpl));

	nvme_completion_poll_cb(&status, &cpl);
	CU_ASSERT(status.done == true);
	CU_ASSERT(memcmp(&cpl, &status.cpl,
			 sizeof(struct spdk_nvme_cpl)) == 0);
}

static void
test_nvme_allocate_request(void)
{
	struct spdk_nvme_qpair qpair = {0};
	struct nvme_payload payload = {0};
	uint32_t payload_size = 0;
	spdk_nvme_cmd_cb cb_fn = NULL;
	void *cb_arg = NULL;
	struct nvme_request *req;
	struct nvme_request dummy_req, match_req;

	STAILQ_INIT(&qpair.free_req);
	STAILQ_INIT(&qpair.queued_req);

	// empty queue
	req = nvme_allocate_request(&qpair, &payload, payload_size,
				    cb_fn, cb_arg);
	CU_ASSERT(req == NULL);

	// put a dummy on the queue, make sure it matches correctly upon return
	memset(&dummy_req, 0xff, sizeof(struct nvme_request));
	memset(&match_req, 0xff, sizeof(struct nvme_request));
	memset(&match_req, 0, offsetof(struct nvme_request, children));
	match_req.cb_fn = cb_fn;
	match_req.cb_arg = cb_arg;
	match_req.payload = payload;
	match_req.payload_size = payload_size;
	match_req.qpair = &qpair;
	match_req.pid = getpid();
	STAILQ_INSERT_HEAD(&qpair.free_req, &dummy_req, stailq);

	req = nvme_allocate_request(&qpair, &payload, payload_size,
				    cb_fn, cb_arg);
	CU_ASSERT(req != NULL);
	CU_ASSERT(memcmp(req, &match_req,
			 sizeof(struct nvme_request)) == 0);
}

static void
test_nvme_allocate_request_contig(void)
{
	struct spdk_nvme_qpair qpair = {0};
	void *buffer = NULL;
	uint32_t payload_size = 0;
	spdk_nvme_cmd_cb cb_fn = NULL;
	void *cb_arg = NULL;
	struct nvme_request *req = NULL;
	struct nvme_request dummy_req, match_req;

	// put a dummy on the queue, make sure it matches correctly upon return
	STAILQ_INIT(&qpair.free_req);
	STAILQ_INIT(&qpair.queued_req);
	memset(&dummy_req, 0xff, sizeof(struct nvme_request));
	memset(&match_req, 0xff, sizeof(struct nvme_request));
	memset(&match_req, 0, offsetof(struct nvme_request, children));
	match_req.payload.type = NVME_PAYLOAD_TYPE_CONTIG;
	match_req.payload.u.sgl.next_sge_fn = 0;
	match_req.qpair = &qpair;
	match_req.pid = getpid();
	STAILQ_INSERT_HEAD(&qpair.free_req, &dummy_req, stailq);

	req = nvme_allocate_request_contig(&qpair, buffer, payload_size,
					   cb_fn, cb_arg);
	CU_ASSERT(req != NULL);
	req->payload.u.sgl.next_sge_fn = 0;
	CU_ASSERT(memcmp(req, &match_req,
			 sizeof(struct nvme_request)) == 0);
}

static void
test_nvme_allocate_request_null(void)
{
	struct spdk_nvme_qpair qpair = {0};
	spdk_nvme_cmd_cb cb_fn = NULL;
	void *cb_arg = NULL;
	struct nvme_request *req = NULL;
	struct nvme_request dummy_req, match_req;

	// put a dummy on the queue, make sure it matches correctly upon return
	STAILQ_INIT(&qpair.free_req);
	STAILQ_INIT(&qpair.queued_req);
	memset(&dummy_req, 0xff, sizeof(struct nvme_request));
	memset(&match_req, 0xff, sizeof(struct nvme_request));
	memset(&match_req, 0, offsetof(struct nvme_request, children));
	match_req.payload.type = NVME_PAYLOAD_TYPE_CONTIG;
	match_req.payload.u.sgl.next_sge_fn = 0;
	match_req.qpair = &qpair;
	match_req.pid = getpid();
	STAILQ_INSERT_HEAD(&qpair.free_req, &dummy_req, stailq);

	req = nvme_allocate_request_null(&qpair, cb_fn, cb_arg);
	CU_ASSERT(req != NULL);
	req->payload.u.sgl.next_sge_fn = 0;
	CU_ASSERT(memcmp(req, &match_req,
			 sizeof(struct nvme_request)) == 0);

}

int ut_dummy_cb = 0;
static void
dummy_cb(void *user_cb_arg, struct spdk_nvme_cpl *cpl)
{
	ut_dummy_cb  = 1;
}

static void
test_nvme_user_copy_cmd_complete(void)
{
	struct nvme_request req = {0};
	struct spdk_nvme_cpl cpl = {0};
	int test_data = 0xdeadbeef;
	int buff_size = sizeof(test_data);

	// without user buffer
	req.user_cb_fn = (void *)dummy_cb;
	ut_dummy_cb = 0;

	nvme_user_copy_cmd_complete(&req, &cpl);
	CU_ASSERT(ut_dummy_cb == 1);

	// with user buffer
	req.user_buffer = malloc(buff_size);
	CU_ASSERT(req.user_buffer != NULL);
	memset(req.user_buffer, 0, buff_size);
	req.payload_size = buff_size;
	req.payload.type = NVME_PAYLOAD_TYPE_CONTIG;
	req.payload.u.contig = malloc(buff_size); // freed in code under test
	memcpy(req.payload.u.contig, &test_data, buff_size);
	CU_ASSERT(req.payload.u.contig != NULL);
	req.cmd.opc = SPDK_NVME_OPC_GET_LOG_PAGE;
	req.pid = getpid();

	ut_dummy_cb = 0;
	nvme_user_copy_cmd_complete(&req, &cpl);
	CU_ASSERT(memcmp(req.user_buffer, &test_data, buff_size) == 0);
	CU_ASSERT(ut_dummy_cb == 1);
	free(req.user_buffer);
}

static void
test_nvme_allocate_request_user_copy(void)
{
	struct spdk_nvme_qpair qpair = {0};
	void *buffer = NULL;
	uint32_t payload_size = 0;
	spdk_nvme_cmd_cb cb_fn = {0};
	void *cb_arg = NULL;
	bool host_to_controller = true;
	struct nvme_request *req;
	struct nvme_request dummy_req;
	int test_data = 0xdeadbeef;
	int buff_size = sizeof(test_data);

	// no buffer or valid payload size, early NULL return
	req = nvme_allocate_request_user_copy(&qpair, buffer, payload_size, cb_fn,
					      cb_arg, host_to_controller);
	CU_ASSERT(req == NULL);

	// good buffer and valid payload size
	buffer = malloc(buff_size);
	CU_ASSERT(buffer != NULL);
	memcpy(buffer, &test_data, buff_size);
	payload_size = buff_size;
	// put a dummy on the queue
	STAILQ_INIT(&qpair.free_req);
	STAILQ_INIT(&qpair.queued_req);
	memset(&dummy_req, 0, sizeof(struct nvme_request));
	STAILQ_INSERT_HEAD(&qpair.free_req, &dummy_req, stailq);

	req = nvme_allocate_request_user_copy(&qpair, buffer, payload_size, cb_fn,
					      cb_arg, host_to_controller);
	CU_ASSERT(req->user_cb_fn == cb_fn);
	CU_ASSERT(req->user_cb_arg == cb_arg);
	CU_ASSERT(req->user_buffer == buffer);
	CU_ASSERT(req->cb_arg == req);
	CU_ASSERT(memcmp(req->payload.u.contig, buffer, sizeof(buff_size)) == 0);

	// same thing but additinoal path coverage (no copy)
	host_to_controller = false;
	memset(&dummy_req, 0, sizeof(struct nvme_request));
	STAILQ_INSERT_HEAD(&qpair.free_req, &dummy_req, stailq);

	req = nvme_allocate_request_user_copy(&qpair, buffer, payload_size, cb_fn,
					      cb_arg, host_to_controller);
	CU_ASSERT(req->user_cb_fn == cb_fn);
	CU_ASSERT(req->user_cb_arg == cb_arg);
	CU_ASSERT(req->user_buffer == buffer);
	CU_ASSERT(req->cb_arg == req);
	CU_ASSERT(memcmp(req->payload.u.contig, buffer, sizeof(buff_size)) != 0);

	// good buffer and valid payload size but lets make spdk_zmalloc fail
	payload_size = 0xffffffff;
	req = nvme_allocate_request_user_copy(&qpair, buffer, payload_size, cb_fn,
					      cb_arg, host_to_controller);
	CU_ASSERT(req == NULL);
}

static void
test_nvme_free_request(void)
{
	struct nvme_request dummy_req = {0};
	struct nvme_request *return_req;

	// init req qpair and put a dummy on it, make sure it gets
	// moved to freeq
	memset(&dummy_req, 0x5a, sizeof(struct spdk_nvme_cmd));
	dummy_req.qpair = malloc(sizeof(*dummy_req.qpair));
	STAILQ_INIT(&dummy_req.qpair->free_req);

	nvme_free_request(&dummy_req);
	return_req = STAILQ_FIRST(&dummy_req.qpair->free_req);
	CU_ASSERT(memcmp(return_req, &dummy_req,
			 sizeof(struct spdk_nvme_cmd)) == 0);

}

static void
test_nvme_robust_mutex_init_shared(void)
{
	pthread_mutex_t mtx;
	int rc = 0;

	rc = nvme_robust_mutex_init_shared(&mtx);
	CU_ASSERT(rc == 0);

	ut_fake_pthread_mutexattr_init = -1;
	rc = nvme_robust_mutex_init_shared(&mtx);
	CU_ASSERT(rc == ut_fake_pthread_mutexattr_init);
	ut_fake_pthread_mutexattr_init = 0;

	ut_fake_pthread_mutex_init = -1;
	rc = nvme_robust_mutex_init_shared(&mtx);
	CU_ASSERT(rc == ut_fake_pthread_mutex_init);
	ut_fake_pthread_mutex_init = 0;
}


static void
test_nvme_driver_init(void)
{
	int rc;
	struct nvme_driver *orig_nvme_driver;

	g_nvme_driver_timeout_us = 100;

	orig_nvme_driver = g_spdk_nvme_driver;
	ut_process_is_primary = true;
	g_spdk_nvme_driver->initialized = true;
	rc = nvme_driver_init();
	CU_ASSERT(rc == 0);

	// additional branch coverage
	g_spdk_nvme_driver = NULL;
	ut_fail_memzone_reserve = true;
	ut_process_is_primary = true;
	rc = nvme_driver_init();
	CU_ASSERT(rc == -1);

	// additional branch coverage
	ut_process_is_primary = false;
	g_spdk_nvme_driver = NULL;
	rc = nvme_driver_init();
	CU_ASSERT(rc == -1);

	// additional branch coverage
	ut_process_is_primary = false;
	g_spdk_nvme_driver = orig_nvme_driver;
	ut_fake_spdk_memzone_lookup = g_spdk_nvme_driver;
	rc = nvme_driver_init();
	CU_ASSERT(rc == 0);

	// additional branch coverage
	ut_process_is_primary = false;
	g_spdk_nvme_driver = orig_nvme_driver;
	ut_fake_spdk_memzone_lookup = g_spdk_nvme_driver;
	g_spdk_nvme_driver->initialized = false;
	rc = nvme_driver_init();
	CU_ASSERT(rc == -1);

	// additional branch coverage
	ut_process_is_primary = true;
	g_spdk_nvme_driver->initialized = false;
	ut_fail_memzone_reserve = false;
	g_spdk_nvme_driver = NULL;
	ut_fake_pthread_mutexattr_init = -1;
	rc = nvme_driver_init();
	CU_ASSERT(rc != 0);

	// additional branch coverage
	ut_process_is_primary = true;
	g_spdk_nvme_driver->initialized = false;
	ut_fail_memzone_reserve = false;
	g_spdk_nvme_driver = NULL;
	ut_fake_pthread_mutexattr_init = 0;
	CU_ASSERT(rc != 0)

	// additional branch coverage
	rc = nvme_driver_init();
	CU_ASSERT(g_spdk_nvme_driver->initialized == false);
	CU_ASSERT(rc == 0);
}

bool ut_fail_dummy_probe_cb = false;
static bool
dummy_probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
	       struct spdk_nvme_ctrlr_opts *opts)
{
	return !ut_fail_dummy_probe_cb;
}

static void
test_nvme_ctrlr_probe(void)
{
	int rc = 0;
	const struct spdk_nvme_transport_id *trid = NULL;
	void *devhandle = NULL;
	spdk_nvme_probe_cb probe_cb = dummy_probe_cb;
	void *cb_ctx = NULL;

	ut_fail_dummy_probe_cb = true;
	rc = nvme_ctrlr_probe(trid, devhandle, probe_cb, cb_ctx);
	CU_ASSERT(rc == 1);

	// additional path coverage
	ut_fail_dummy_probe_cb = false;
	rc = nvme_ctrlr_probe(trid, devhandle, probe_cb, cb_ctx);
	CU_ASSERT(rc == -1);

	// additional path coverage
	ut_fail_dummy_probe_cb = false;
	// this can be any valid address for this test
	ut_fake_nvme_transport_ctrlr_construct = &ut_fake_nvme_transport_ctrlr_construct;
	rc = nvme_ctrlr_probe(trid, devhandle, probe_cb, cb_ctx);
	CU_ASSERT(rc == 0);
}

static void
test_opc_data_transfer(void)
{
	enum spdk_nvme_data_transfer xfer;

	xfer = spdk_nvme_opc_get_data_transfer(SPDK_NVME_OPC_FLUSH);
	CU_ASSERT(xfer == SPDK_NVME_DATA_NONE);

	xfer = spdk_nvme_opc_get_data_transfer(SPDK_NVME_OPC_WRITE);
	CU_ASSERT(xfer == SPDK_NVME_DATA_HOST_TO_CONTROLLER);

	xfer = spdk_nvme_opc_get_data_transfer(SPDK_NVME_OPC_READ);
	CU_ASSERT(xfer == SPDK_NVME_DATA_CONTROLLER_TO_HOST);

	xfer = spdk_nvme_opc_get_data_transfer(SPDK_NVME_OPC_GET_LOG_PAGE);
	CU_ASSERT(xfer == SPDK_NVME_DATA_CONTROLLER_TO_HOST);
}

static void
test_trid_parse(void)
{
	struct spdk_nvme_transport_id trid1, trid2;

	memset(&trid1, 0, sizeof(trid1));
	CU_ASSERT(spdk_nvme_transport_id_parse(&trid1,
					       "trtype:rdma\n"
					       "adrfam:ipv4\n"
					       "traddr:192.168.100.8\n"
					       "trsvcid:4420\n"
					       "subnqn:nqn.2014-08.org.nvmexpress.discovery") == 0);
	CU_ASSERT(trid1.trtype == SPDK_NVME_TRANSPORT_RDMA);
	CU_ASSERT(trid1.adrfam == SPDK_NVMF_ADRFAM_IPV4);
	CU_ASSERT(strcmp(trid1.traddr, "192.168.100.8") == 0);
	CU_ASSERT(strcmp(trid1.trsvcid, "4420") == 0);
	CU_ASSERT(strcmp(trid1.subnqn, "nqn.2014-08.org.nvmexpress.discovery") == 0);

	memset(&trid2, 0, sizeof(trid2));
	CU_ASSERT(spdk_nvme_transport_id_parse(&trid2, "trtype:PCIe traddr:0000:04:00.0") == 0);
	CU_ASSERT(trid2.trtype == SPDK_NVME_TRANSPORT_PCIE);
	CU_ASSERT(strcmp(trid2.traddr, "0000:04:00.0") == 0);

	CU_ASSERT(spdk_nvme_transport_id_compare(&trid1, &trid2) != 0);
}

static void
test_spdk_nvme_transport_id_parse_trtype(void)
{

	enum spdk_nvme_transport_type *trtype;
	enum spdk_nvme_transport_type sct;
	char *str;

	trtype = NULL;
	str = "unit_test";

	/* test function returned value when trtype is NULL but str not NULL */
	CU_ASSERT(spdk_nvme_transport_id_parse_trtype(trtype, str) == (-EINVAL));

	/* test function returned value when str is NULL but trtype not NULL */
	trtype = &sct;
	str = NULL;
	CU_ASSERT(spdk_nvme_transport_id_parse_trtype(trtype, str) == (-EINVAL));

	/* test function returned value when str and strtype not NULL, but str value
	 * not "PCIe" or "RDMA" */
	str = "unit_test";
	CU_ASSERT(spdk_nvme_transport_id_parse_trtype(trtype, str) == (-ENOENT));

	/* test trtype value when use function "strcasecmp" to compare str and "PCIe"，not case-sensitive */
	str = "PCIe";
	spdk_nvme_transport_id_parse_trtype(trtype, str);
	CU_ASSERT((*trtype) == SPDK_NVME_TRANSPORT_PCIE);

	str = "pciE";
	spdk_nvme_transport_id_parse_trtype(trtype, str);
	CU_ASSERT((*trtype) == SPDK_NVME_TRANSPORT_PCIE);

	/* test trtype value when use function "strcasecmp" to compare str and "RDMA"，not case-sensitive */
	str = "RDMA";
	spdk_nvme_transport_id_parse_trtype(trtype, str);
	CU_ASSERT((*trtype) == SPDK_NVME_TRANSPORT_RDMA);

	str = "rdma";
	spdk_nvme_transport_id_parse_trtype(trtype, str);
	CU_ASSERT((*trtype) == SPDK_NVME_TRANSPORT_RDMA);

}

static void
test_spdk_nvme_transport_id_parse_adrfam(void)
{

	enum spdk_nvmf_adrfam *adrfam;
	enum spdk_nvmf_adrfam sct;
	char *str;

	adrfam = NULL;
	str = "unit_test";

	/* test function returned value when adrfam is NULL but str not NULL */
	CU_ASSERT(spdk_nvme_transport_id_parse_adrfam(adrfam, str) == (-EINVAL));

	/* test function returned value when str is NULL but adrfam not NULL */
	adrfam = &sct;
	str = NULL;
	CU_ASSERT(spdk_nvme_transport_id_parse_adrfam(adrfam, str) == (-EINVAL));

	/* test function returned value when str and adrfam not NULL, but str value
	 * not "IPv4" or "IPv6" or "IB" or "FC" */
	str = "unit_test";
	CU_ASSERT(spdk_nvme_transport_id_parse_adrfam(adrfam, str) == (-ENOENT));

	/* test adrfam value when use function "strcasecmp" to compare str and "IPv4"，not case-sensitive */
	str = "IPv4";
	spdk_nvme_transport_id_parse_adrfam(adrfam, str);
	CU_ASSERT((*adrfam) == SPDK_NVMF_ADRFAM_IPV4);

	str = "ipV4";
	spdk_nvme_transport_id_parse_adrfam(adrfam, str);
	CU_ASSERT((*adrfam) == SPDK_NVMF_ADRFAM_IPV4);

	/* test adrfam value when use function "strcasecmp" to compare str and "IPv6"，not case-sensitive */
	str = "IPv6";
	spdk_nvme_transport_id_parse_adrfam(adrfam, str);
	CU_ASSERT((*adrfam) == SPDK_NVMF_ADRFAM_IPV6);

	str = "ipV6";
	spdk_nvme_transport_id_parse_adrfam(adrfam, str);
	CU_ASSERT((*adrfam) == SPDK_NVMF_ADRFAM_IPV6);

	/* test adrfam value when use function "strcasecmp" to compare str and "IB"，not case-sensitive */
	str = "IB";
	spdk_nvme_transport_id_parse_adrfam(adrfam, str);
	CU_ASSERT((*adrfam) == SPDK_NVMF_ADRFAM_IB);

	str = "ib";
	spdk_nvme_transport_id_parse_adrfam(adrfam, str);
	CU_ASSERT((*adrfam) == SPDK_NVMF_ADRFAM_IB);

	/* test adrfam value when use function "strcasecmp" to compare str and "FC"，not case-sensitive */
	str = "FC";
	spdk_nvme_transport_id_parse_adrfam(adrfam, str);
	CU_ASSERT((*adrfam) == SPDK_NVMF_ADRFAM_FC);

	str = "fc";
	spdk_nvme_transport_id_parse_adrfam(adrfam, str);
	CU_ASSERT((*adrfam) == SPDK_NVMF_ADRFAM_FC);

}

int main(int argc, char **argv)
{
	CU_pSuite   suite = NULL;
	unsigned int    num_failures;

	if (CU_initialize_registry() != CUE_SUCCESS) {
		return CU_get_error();
	}

	suite = CU_add_suite("nvme", NULL, NULL);
	if (suite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	if (
		CU_add_test(suite, "test_opc_data_transfer",
			    test_opc_data_transfer) == NULL ||
		CU_add_test(suite, "test_spdk_nvme_transport_id_parse_trtype",
			    test_spdk_nvme_transport_id_parse_trtype) == NULL ||
		CU_add_test(suite, "test_spdk_nvme_transport_id_parse_adrfam",
			    test_spdk_nvme_transport_id_parse_adrfam) == NULL ||
		CU_add_test(suite, "test_trid_parse",
			    test_trid_parse) == NULL ||
		CU_add_test(suite, "test_spdk_nvme_detach",
			    test_spdk_nvme_detach) == NULL ||
		CU_add_test(suite, "test_nvme_completion_poll_cb",
			    test_nvme_completion_poll_cb) == NULL ||
		CU_add_test(suite, "test_nvme_allocate_request",
			    test_nvme_allocate_request) == NULL ||
		CU_add_test(suite, "test_nvme_allocate_request_contig",
			    test_nvme_allocate_request_contig) == NULL ||
		CU_add_test(suite, "test_nvme_allocate_request_null",
			    test_nvme_allocate_request_null) == NULL ||
		CU_add_test(suite, "test_nvme_user_copy_cmd_complete",
			    test_nvme_user_copy_cmd_complete) == NULL ||
		CU_add_test(suite, "test_nvme_allocate_request_user_copy",
			    test_nvme_allocate_request_user_copy) == NULL ||
		CU_add_test(suite, "test_nvme_free_request",
			    test_nvme_free_request) == NULL ||
		CU_add_test(suite, "test_nvme_robust_mutex_init_shared",
			    test_nvme_robust_mutex_init_shared) == NULL ||
		CU_add_test(suite, "test_nvme_driver_init",
			    test_nvme_driver_init) == NULL ||
		CU_add_test(suite, "test_nvme_ctrlr_probe",
			    test_nvme_ctrlr_probe) == NULL
	) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	num_failures = CU_get_number_of_failures();
	CU_cleanup_registry();
	return num_failures;
}
