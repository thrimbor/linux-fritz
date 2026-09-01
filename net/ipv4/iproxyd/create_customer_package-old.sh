#!/bin/sh

if [ ! -n "$1" ] ; then
	echo "Usage : create_customer_package.sh SOURCE_PATH"
	exit 0
fi

rm -rf *
cp $1/Makefile Makefile

if [ -e $1/iproxyd.o.T1 ] ; then
	cp $1/iproxyd.o.T1 iproxyd.o.T1
fi

if [ -e $1/iproxyd.o.T1 ] ; then
	cp $1/iproxyd.o.T2 iproxyd.o.T2
fi

sed -i -e 's/^CONFIG_FULL_PACKAGE/#CONFIG_FULL_PACKAGE/g' Makefile
