#!/usr/bin/env bash
#
# Environment variables:
#  $valgrind    Valgrind executable name, if desired

set -xe

# setup local unit test coverage if cov is available
src=$(readlink -f $(dirname $0))
out=$PWD
cd $src
if hash lcov; then
	if [ $EUID -ne 0 ]; then
		echo "$0 must be run as root for coverage"
		exit 1
	fi
	export LCOV_OPTS="
		--rc lcov_branch_coverage=1
		--rc lcov_function_coverage=1
		--rc genhtml_branch_coverage=1
		--rc genhtml_function_coverage=1
		--rc genhtml_legend=1
		--rc geninfo_all_blocks=1
		"
	export LCOV="lcov $LCOV_OPTS --no-external"
	# zero out coverage data
	$LCOV -q -c -i -t "Baseline" -d $src -o cov_base.info
fi


$valgrind test/lib/blob/blob_ut/blob_ut

$valgrind test/lib/blobfs/blobfs_ut/blobfs_ut
# cache_ut hangs when run under valgrind, so don't use $valgrind
test/lib/blobfs/cache_ut/cache_ut

$valgrind test/lib/nvme/unit/nvme_c/nvme_ut
$valgrind test/lib/nvme/unit/nvme_ctrlr_c/nvme_ctrlr_ut
$valgrind test/lib/nvme/unit/nvme_ctrlr_cmd_c/nvme_ctrlr_cmd_ut
$valgrind test/lib/nvme/unit/nvme_ns_c/nvme_ns_ut
$valgrind test/lib/nvme/unit/nvme_ns_cmd_c/nvme_ns_cmd_ut
$valgrind test/lib/nvme/unit/nvme_qpair_c/nvme_qpair_ut
$valgrind test/lib/nvme/unit/nvme_pcie_c/nvme_pcie_ut
$valgrind test/lib/nvme/unit/nvme_quirks_c/nvme_quirks_ut

$valgrind test/lib/ioat/unit/ioat_ut

$valgrind test/lib/json/parse/json_parse_ut
$valgrind test/lib/json/util/json_util_ut
$valgrind test/lib/json/write/json_write_ut

$valgrind test/lib/jsonrpc/server/jsonrpc_server_ut

$valgrind test/lib/log/log_ut

$valgrind test/lib/nvmf/discovery/discovery_ut
$valgrind test/lib/nvmf/request/request_ut
$valgrind test/lib/nvmf/session/session_ut
$valgrind test/lib/nvmf/subsystem/subsystem_ut
$valgrind test/lib/nvmf/direct/direct_ut
$valgrind test/lib/nvmf/virtual/virtual_ut

$valgrind test/lib/scsi/dev/dev_ut
$valgrind test/lib/scsi/init/init_ut
$valgrind test/lib/scsi/lun/lun_ut
$valgrind test/lib/scsi/scsi_bdev/scsi_bdev_ut
$valgrind test/lib/scsi/scsi_nvme/scsi_nvme_ut

# TODO: fix valgrind warnings and add $valgrind to iSCSI tests
test/lib/iscsi/param/param_ut
$valgrind test/lib/iscsi/target_node/target_node_ut test/lib/iscsi/target_node/target_node.conf
test/lib/iscsi/pdu/pdu

$valgrind test/lib/util/bit_array/bit_array_ut
$valgrind test/lib/util/io_channel/io_channel_ut
$valgrind test/lib/util/string/string_ut

# local unit test coverage
if hash lcov; then
	#generate coverage data and combine with baseline
	$LCOV -q -c -d $src -t "$(hostname)" -o cov_test.info
	$LCOV -q -a cov_base.info -a cov_test.info -o cov_total.info
	$LCOV -q -a cov_total.info -o cov_unit.info
	genhtml cov_unit.info --output-directory .
fi

set +x

echo
echo
echo "====================="
echo "All unit tests passed"
echo "====================="
echo
echo
