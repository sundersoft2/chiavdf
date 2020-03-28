#!/bin/sh

# Stop in case of error
set -e

python -m pip install cibuildwheel==1.3.0
python -m pip install pep517

# build just python 3.7
CIBW_BUILD="cp37-win_amd64"
# don't build i686 targets, can't seem to find cmake for these
#CIBW_SKIP="'*-manylinux_i686'"
# we need boost
#CIBW_MANYLINUX_X86_64_IMAGE: manylinux2014
#CIBW_BEFORE_BUILD_LINUX: curl -L https://github.com/Kitware/CMake/releases/download/v3.17.0/cmake-3.17.0-Linux-`uname -m`.sh > cmake.sh && yes | sh cmake.sh | cat && rm -f /usr/bin/cmake && yum -y install boost-devel gmp-devel && python -m pip install --upgrade pip && which cmake && cmake --version
#CIBW_BEFORE_BUILD_MACOS: brew install boost && python -m pip install --upgrade pip
CIBW_BEFORE_BUILD_WINDOWS="python -m pip install --upgrade pip"
CIBW_TEST_REQUIRES="pytest"
CIBW_TEST_COMMAND="py.test -v {project}/tests"
#CIBW_ENVIRONMENT_LINUX: "PATH=/project/cmake-3.17.0-Linux-`uname -m`/bin:$PATH BUILD_VDF_CLIENT=N"

python -m cibuildwheel --output-dir dist
