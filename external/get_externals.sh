#!/bin/bash
#
# simple script to pull in dependencies or update them
#
OPWD=$PWD
echo Catch2
if [ ! -d Catch2 ]
then
	git clone --filter=tree:0 https://github.com/catchorg/Catch2.git
else 
	cd Catch2
	git pull
	cd $OPWD
fi
echo fmt
if [ ! -d fmt ]
then
	git clone --filter=tree:0 https://github.com/fmtlib/fmt.git
else 
	cd fmt
	git pull
	cd $OPWD
fi
echo yaml-cpp
if [ ! -d yaml-cpp ]
then
	git clone --filter=tree:0 https://github.com/jbeder/yaml-cpp.git
else 
	cd yaml-cpp
	git pull
	cd $OPWD
fi
echo rapidyaml
if [ ! -d rapidyaml ]
then
	git clone --filter=tree:0 --recurse-submodules --also-filter-submodules https://github.com/biojppm/rapidyaml.git
else 
	cd rapidyaml
	git pull 
	git submodule update --recursive
	cd $OPWD
fi
echo GSL
if [ ! -d GSL ]
then
	git clone --filter=tree:0 https://github.com/microsoft/GSL.git
else 
	cd GSL
	git pull
	cd $OPWD
fi
