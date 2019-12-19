#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2019 Red Hat, Inc.

devtools_dir=$(dirname $(readlink -f $0))
. $devtools_dir/load-devel-config

abi_ref_build_dir=${DPDK_ABI_REF_BUILD_DIR:-reference}
for dir in $abi_ref_build_dir/*; do
	if [ "$dir" = "$abi_ref_build_dir" ]; then
		exit 1
	fi
	if [ -d $dir/dump ]; then
		echo "Skipping $dir"
		continue
	fi
	if [ -d $dir/install ]; then
		libdir=$dir/install
	else
		libdir=$dir
	fi
	echo "Dumping libraries from $libdir in $dir/dump"
	$devtools_dir/gen-abi-dump.sh $libdir $dir/dump
done
