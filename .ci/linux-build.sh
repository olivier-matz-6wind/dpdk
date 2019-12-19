#!/bin/sh -xe

on_error() {
    if [ $? = 0 ]; then
        exit
    fi
    FILES_TO_PRINT="build/meson-logs/testlog.txt build/.ninja_log build/meson-logs/meson-log.txt"

    for pr_file in $FILES_TO_PRINT; do
        if [ -e "$pr_file" ]; then
            cat "$pr_file"
        fi
    done
}
trap on_error EXIT

if [ "$AARCH64" = "1" ]; then
    # convert the arch specifier
    OPTS="$OPTS --cross-file config/arm/arm64_armv8_linux_gcc"
fi

if [ "$BUILD_DOCS" = "1" ]; then
    OPTS="$OPTS -Denable_docs=true"
fi

if [ "$BUILD_32BIT" = "1" ]; then
    OPTS="$OPTS -Dc_args=-m32 -Dc_link_args=-m32"
    export PKG_CONFIG_LIBDIR="/usr/lib32/pkgconfig"
fi

OPTS="$OPTS --default-library=$DEF_LIB"
meson build --werror -Dexamples=all $OPTS

if [ "$ABI_CHECKS" = "1" ]; then
    git remote add ref ${REF_GIT_REPO:-https://dpdk.org/git/dpdk}
    git fetch --tags ref ${REF_GIT_BRANCH:-master}

    head=$(git describe --all)
    tag=$(git describe --abbrev=0)

    if [ "$(cat reference/VERSION 2>/dev/null)" != "$tag" ]; then
        rm -rf reference
    fi

    if [ ! -d reference ]; then
        gen_abi_dump=$(mktemp -t gen-abi-dump-XXX.sh)
        cp -a devtools/gen-abi-dump.sh $gen_abi_dump

        git checkout -qf $tag
        ninja -C build
        $gen_abi_dump build reference
        rm $gen_abi_dump

        if [ "$AARCH64" != "1" ]; then
            mkdir -p reference/app
            cp -a build/app/dpdk-testpmd reference/app/

            export LD_LIBRARY_PATH=$(pwd)/build/lib:$(pwd)/build/drivers
            devtools/test-null.sh reference/app/dpdk-testpmd
            unset LD_LIBRARY_PATH
        fi
        echo $tag > reference/VERSION

        git checkout -qf $head
    fi
fi

ninja -C build

if [ "$ABI_CHECKS" = "1" ]; then
    devtools/gen-abi-dump.sh build dump
    devtools/check-abi-dump.sh reference dump ${ABI_CHECKS_WARN_ONLY:-}
    if [ "$AARCH64" != "1" ]; then
        export LD_LIBRARY_PATH=$(pwd)/build/lib:$(pwd)/build/drivers
        devtools/test-null.sh reference/app/dpdk-testpmd
        unset LD_LIBRARY_PATH
    fi
fi

if [ "$AARCH64" != "1" ]; then
    devtools/test-null.sh
fi

if [ "$RUN_TESTS" = "1" ]; then
    sudo meson test -C build --suite fast-tests -t 3
fi
