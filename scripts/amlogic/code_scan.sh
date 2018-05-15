#!/bin/bash

export PATH=/proj/coverity/cov-analysis/bin/:$PATH
export CROSS_COMPILE=/opt/gcc-linaro-6.3.1-2017.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
export KERNEL_PATH=`pwd`
export KERNEL_OUTPUT_PATH="$KERNEL_PATH/../kernel_output/"
export COVERITY_OUTPUT_PATH="$KERNEL_PATH/../coverity_output/"
export COVERITY_HTML_PATH="$KERNEL_PATH/../coverity_html/"

if [ -n "$1" ]; then
	if [ -d "$KERNEL_PATH/$1" ]; then
		echo "scan path: $1"
	else
		echo "you should input a right module path"
		exit 1
	fi
fi

echo "kernel distclean"
make ARCH=arm64 O="$KERNEL_OUTPUT_PATH" distclean
rm -rf "$COVERITY_HTML_PATH"


make ARCH=arm64 O="$KERNEL_OUTPUT_PATH" meson64_defconfig
cov-build --dir $COVERITY_OUTPUT_PATH make ARCH=arm64 O="$KERNEL_OUTPUT_PATH" -j8 Image  UIMAGE_LOADADDR=0x1080000 || echo "Compile Image Fail !!"
if [ -n "$1" ]; then
	cov-analyze --dir "$COVERITY_OUTPUT_PATH" --strip-path "$KERNEL_PATH" --tu-pattern "file(\"$1\")" --all
else
	cov-analyze --dir "$COVERITY_OUTPUT_PATH" --strip-path "$KERNEL_PATH" --all
fi
cov-format-errors --dir "$COVERITY_OUTPUT_PATH" --strip-path "$KERNEL_PATH" --html-output "$COVERITY_HTML_PATH" --filesort -x

rm -rf "$COVERITY_HTML_PATH/emit"
