#!/bin/bash
if [ "$1" != "emacs" ];then
	echo "make tags ARCH=arm"
	make tags ARCH=arm
else
	echo "make TAGS ARCH=arm"
	make TAGS ARCH=arm
fi

echo "make cscope ARCH=arm"
make cscope ARCH=arm
