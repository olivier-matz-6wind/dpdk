#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2019 Red Hat, Inc.

if [ $# != 2 ] && [ $# != 3 ]; then
	echo "Usage: $0 dumpdir dumpdir2 [warnonly]"
	exit 1
fi

dumpdir=$1
dumpdir2=$2
warnonly=${3:-}
if [ ! -d $dumpdir ]; then
	echo "Error: dump directory '$dumpdir' does not exist."
	exit 1
fi
if [ ! -d $dumpdir2 ]; then
	echo "Error: dump directory '$dumpdir2' does not exist."
	exit 1
fi

ABIDIFF_OPTIONS="--suppr $(dirname $0)/dpdk.abignore"
error=
for dump in $(find $dumpdir -name "*.dump"); do
	dump2=$dumpdir2/$(basename $dump)
	if [ ! -e $dump2 ]; then
		echo "Error: can't find $dump2"
		error=1
		continue
	elif ! abidiff $ABIDIFF_OPTIONS $dump $dump2; then
		echo "Error: ABI issue reported for $dump, $dump2"
		error=1
	fi
done

[ -z "$error" ] || [ -n "$warnonly" ]
