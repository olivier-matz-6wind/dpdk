#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2019 Red Hat, Inc.

if [ $# != 2 ] && [ $# != 3 ]; then
	echo "Usage: $0 builddir dumpdir [warnonly]"
	exit 1
fi

builddir=$1
dumpdir=$2
warnonly=${3:-}
if [ ! -d $builddir ]; then
	echo "Error: build directory '$builddir' does not exist."
	exit 1
fi
if [ ! -d $dumpdir ]; then
	echo "Error: dump directory '$dumpdir' does not exist."
	exit 1
fi

ABIDIFF_OPTIONS="--suppr $(dirname $0)/dpdk.abignore"
error=
for dump in $(find $dumpdir -name "*.dump"); do
	libname=$(basename $dump)
	libname=${libname%.dump}
	result=
	for f in $(find $builddir -name "$libname.so.*"); do
		if test -L $f || [ "$f" != "${f%%.symbols}" ]; then
			continue
		fi
		result=found

		if ! abidiff $ABIDIFF_OPTIONS $dump $f; then
			echo "Error: ABI issue reported for $dump, $f"
			error=1
		fi
		break
	done
	if [ "$result" != "found" ]; then
		echo "Error: can't find a library for dump file $dump"
		error=1
	fi
done

[ -z "$error" ] || [ -n "$warnonly" ]
