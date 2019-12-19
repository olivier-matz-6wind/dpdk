#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2019 Red Hat, Inc.

devtools_dir=$(dirname $(readlink -f $0))
. $devtools_dir/load-devel-config

abi_ref_build_dir=${DPDK_ABI_REF_BUILD_DIR:-reference}
builds_dir=${DPDK_BUILD_TEST_DIR:-.}

for dir in $abi_ref_build_dir/*; do
	if [ "$dir" = "$abi_ref_build_dir" ]; then
		exit 1
	fi
	if [ ! -d $dir/dump ]; then
		echo "Skipping $dir"
		continue
	fi
	target=$(basename $dir)
	if [ -d $builds_dir/$target/install ]; then
		libdir=$builds_dir/$target/install
	else
		libdir=$builds_dir/$target
	fi
	echo "Checking ABI between $libdir and $dir/dump"
	$devtools_dir/check-abi-dump.sh $libdir $dir/dump
done
