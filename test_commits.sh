#!/bin/bash
cur_branch=$(git branch --show-current)

for arch in arm64 arm ; do
	if [ "${arch}" != "arm64" ]; then
		export ARCH=arm
		export CROSS_COMPILE=arm-linux-gnueabihf-
		image="zImage";
		base_file=arch/arm/configs/boundary_defconfig;
		temp=arm;
	else
		export ARCH=arm64
		export CROSS_COMPILE=aarch64-linux-gnu-
		image="Image";
		base_file=arch/arm64/configs/boundary_defconfig;
		temp=arm64;
	fi

	start=$(git log --oneline --reverse ${base_file} | head -n1 | cut -f1 -d' ')
	git log --oneline --reverse ${start}..HEAD >${temp}.tmp
	echo Start >${temp}_2.tmp
	while read -r line; do
		commit=$(echo $line | cut -f1 -d' ');
		echo ${commit}
		git checkout ${commit};
		status="fail";
		make boundary_defconfig
		if [ $? -eq 0 ]; then
			make -j4 ${image} dtbs 2>>${temp}_2.tmp
			if [ $? -eq 0 ]; then
				make -j4 modules 2>>${temp}_2.tmp
				if [ $? -eq 0 ]; then
					status="success";
				fi
			fi
		fi
		echo ${commit} ${status} >>${temp}_2.tmp
	done < ${temp}.tmp
	git checkout ${cur_branch}
done


