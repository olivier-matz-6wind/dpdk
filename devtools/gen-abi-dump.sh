#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2019 Red Hat, Inc.

if [ $# != 2 ]; then
	echo "Usage: $0 builddir dumpdir"
	exit 1
fi

builddir=$1
dumpdir=$2
if [ ! -d $builddir ]; then
	echo "Error: build directory '$builddir' does not exist."
	exit 1
fi
if [ -d $dumpdir ]; then
	echo "Error: dump directory '$dumpdir' already exists."
	exit 1
fi

mkdir -p $dumpdir
for f in $(find $builddir -name "*.so.*"); do
	if test -L $f || [ "$f" != "${f%%.symbols}" ]; then
		continue
	fi

	libname=$(basename $f)
	abidw --out-file $dumpdir/${libname%.so.*}.dump $f
done
