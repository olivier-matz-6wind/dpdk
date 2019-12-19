#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2019 Red Hat, Inc.

devtools_dir=$(dirname $(readlink -f $0))
. $devtools_dir/load-devel-config

abi_ref_build_dir=${DPDK_ABI_REF_BUILD_DIR:-reference}
builds_dir=${DPDK_BUILD_TEST_DIR:-.}

error=
for dir in $abi_ref_build_dir/*; do
	if [ "$dir" = "$abi_ref_build_dir" ]; then
		exit 1
	fi
	if [ ! -d $dir/dump ]; then
		if [ -d $dir/install ]; then
			libdir=$dir/install
		else
			libdir=$dir
		fi
		echo "Dumping libraries from $libdir in $dir/dump"
		$devtools_dir/gen-abi-dump.sh $libdir $dir/dump
	fi
	newdir=$builds_dir/$(basename $dir)
	if [ ! -d $newdir/dump ]; then
		if [ -d $newdir/install ]; then
			libdir=$newdir/install
		else
			libdir=$newdir
		fi
		echo "Dumping libraries from $libdir in $newdir/dump"
		$devtools_dir/gen-abi-dump.sh $libdir $newdir/dump
	fi
	echo "Checking ABI between $dir/dump $newdir/dump"
	$devtools_dir/check-abi-dump.sh $dir/dump $newdir/dump || error=1
done

[ -z "$error" ]
