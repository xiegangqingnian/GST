#!/bin/bash
##########################################################################
# This is the GSTIO automated install script for Linux and Mac OS.
# This file was downloaded from https://github.com/gsthot/gst.git
#
# Copyright (c) 2017, Respective Authors all rights reserved.
#
# After June 1, 2018 this software is available under the following terms:
#
# The MIT License
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
##########################################################################

   SOURCE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

   function usage()
   {
      printf "\\tUsage: %s \\n\\t[Build Option -o <Debug|Release|RelWithDebInfo|MinSizeRel>] \\n\\t[CodeCoverage -c] \\n\\t[Doxygen -d] \\n\\t[CoreSymbolName -s <1-7 characters>] \\n\\t[Avoid Compiling -a]\\n\\n" "$0" 1>&2
      exit 1
   }

   ARCH=$( uname )
   if [ "${SOURCE_DIR}" == "${PWD}" ]; then
      BUILD_DIR="${PWD}/build"
   else
      BUILD_DIR="${PWD}"
   fi
   CMAKE_BUILD_TYPE=Release
   DISK_MIN=20
   DOXYGEN=false
   ENABLE_COVERAGE_TESTING=false
   CORE_SYMBOL_NAME="GST"
   # Use current directory's tmp directory if noexec is enabled for /tmp
   if (mount | grep "/tmp " | grep --quiet noexec); then
        mkdir -p $SOURCE_DIR/tmp
        TEMP_DIR="${SOURCE_DIR}/tmp"
        rm -rf $SOURCE_DIR/tmp/*
   else # noexec wasn't found
        TEMP_DIR="/tmp"
   fi
   START_MAKE=true
   TIME_BEGIN=$( date -u +%s )
   VERSION=1.2

   txtbld=$(tput bold)
   bldred=${txtbld}$(tput setaf 1)
   txtrst=$(tput sgr0)

   if [ $# -ne 0 ]; then
      while getopts ":cdo:s:ah" opt; do
         case "${opt}" in
            o )
               options=( "Debug" "Release" "RelWithDebInfo" "MinSizeRel" )
               if [[ "${options[*]}" =~ "${OPTARG}" ]]; then
                  CMAKE_BUILD_TYPE="${OPTARG}"
               else
                  printf "\\n\\tInvalid argument: %s\\n" "${OPTARG}" 1>&2
                  usage
                  exit 1
               fi
            ;;
            c )
               ENABLE_COVERAGE_TESTING=true
            ;;
            d )
               DOXYGEN=true
            ;;
            s)
               if [ "${#OPTARG}" -gt 7 ] || [ -z "${#OPTARG}" ]; then
                  printf "\\n\\tInvalid argument: %s\\n" "${OPTARG}" 1>&2
                  usage
                  exit 1
               else
                  CORE_SYMBOL_NAME="${OPTARG}"
               fi
            ;;
            a)
               START_MAKE=false
            ;;
            h)
               usage
               exit 1
            ;;
            \? )
               printf "\\n\\tInvalid Option: %s\\n" "-${OPTARG}" 1>&2
               usage
               exit 1
            ;;
            : )
               printf "\\n\\tInvalid Option: %s requires an argument.\\n" "-${OPTARG}" 1>&2
               usage
               exit 1
            ;;
            * )
               usage
               exit 1
            ;;
         esac
      done
   fi
 

   pushd "${SOURCE_DIR}" &> /dev/null

   STALE_SUBMODS=$(( $(git submodule status --recursive | grep -c "^[+\-]") ))
   

   printf "\\n\\tBeginning build version: %s\\n" "${VERSION}"
   printf "\\t%s\\n" "$( date -u )"
   printf "\\tUser: %s\\n" "$( whoami )"
   printf "\\tgit head id: %s\\n" "$( cat .git/refs/heads/master )"
   printf "\\tCurrent branch: %s\\n" "$( git rev-parse --abbrev-ref HEAD )"
   printf "\\n\\tARCHITECTURE: %s\\n" "${ARCH}"

   popd &> /dev/null

   if [ "$ARCH" == "Linux" ]; then

      if [ ! -e /etc/os-release ]; then
         printf "\\n\\tGSTIO currently supports Amazon, Centos, Fedora, Mint & Ubuntu Linux only.\\n"
         printf "\\tPlease install on the latest version of one of these Linux distributions.\\n"
         printf "\\thttps://aws.amazon.com/amazon-linux-ami/\\n"
         printf "\\thttps://www.centos.org/\\n"
         printf "\\thttps://start.fedoraproject.org/\\n"
         printf "\\thttps://linuxmint.com/\\n"
         printf "\\thttps://www.ubuntu.com/\\n"
         printf "\\tExiting now.\\n"
         exit 1
      fi

      OS_NAME=$( cat /etc/os-release | grep ^NAME | cut -d'=' -f2 | sed 's/\"//gI' )

      case "$OS_NAME" in
         "Amazon Linux AMI"|"Amazon Linux")
            FILE="${SOURCE_DIR}/scripts/gstio_build_amazon.sh"
            CXX_COMPILER=g++
            C_COMPILER=gcc
            MONGOD_CONF=${HOME}/opt/mongodb/mongod.conf
            export LLVM_DIR=${HOME}/opt/wasm/lib/cmake/llvm
            export CMAKE=${HOME}/opt/cmake/bin/cmake
            export PATH=${HOME}/opt/mongodb/bin:$PATH
         ;;
         "CentOS Linux")
            FILE="${SOURCE_DIR}/scripts/gstio_build_centos_fast.sh"
            CXX_COMPILER=/opt/rh/devtoolset-7/root/usr/bin/g++
            C_COMPILER=/opt/rh/devtoolset-7/root/usr/bin/gcc
            MONGOD_CONF=${HOME}/opt/mongodb/mongod.conf
            export LLVM_DIR=${HOME}/opt/wasm/lib/cmake/llvm
            export CMAKE=${HOME}/opt/cmake/bin/cmake
            export PATH=${HOME}/opt/mongodb/bin:$PATH
         ;;
         "elementary OS")
            FILE="${SOURCE_DIR}/scripts/gstio_build_ubuntu.sh"
            CXX_COMPILER=clang++-4.0
            C_COMPILER=clang-4.0
            MONGOD_CONF=${HOME}/opt/mongodb/mongod.conf
            export PATH=${HOME}/opt/mongodb/bin:$PATH
         ;;
         "Fedora")
            FILE="${SOURCE_DIR}/scripts/gstio_build_fedora.sh"
            CXX_COMPILER=g++
            C_COMPILER=gcc
            MONGOD_CONF=/etc/mongod.conf
            export LLVM_DIR=${HOME}/opt/wasm/lib/cmake/llvm
         ;;
         "Linux Mint")
            FILE="${SOURCE_DIR}/scripts/gstio_build_ubuntu.sh"
            CXX_COMPILER=clang++-4.0
            C_COMPILER=clang-4.0
            MONGOD_CONF=${HOME}/opt/mongodb/mongod.conf
            export PATH=${HOME}/opt/mongodb/bin:$PATH
         ;;
         "Ubuntu")
            FILE="${SOURCE_DIR}/scripts/gstio_build_ubuntu.sh"
            CXX_COMPILER=clang++-4.0
            C_COMPILER=clang-4.0
            MONGOD_CONF=${HOME}/opt/mongodb/mongod.conf
            export PATH=${HOME}/opt/mongodb/bin:$PATH
         ;;
         "Debian GNU/Linux")
            FILE=${SOURCE_DIR}/scripts/gstio_build_ubuntu.sh
            CXX_COMPILER=clang++-4.0
            C_COMPILER=clang-4.0
            MONGOD_CONF=${HOME}/opt/mongodb/mongod.conf
            export PATH=${HOME}/opt/mongodb/bin:$PATH
         ;;
         *)
            printf "\\n\\tUnsupported Linux Distribution. Exiting now.\\n\\n"
            exit 1
      esac

      export BOOST_ROOT="${HOME}/opt/boost"
      OPENSSL_ROOT_DIR=/usr/include/openssl
   fi

   if [ "$ARCH" == "Darwin" ]; then
      FILE="${SOURCE_DIR}/scripts/gstio_build_darwin.sh"
      CXX_COMPILER=clang++
      C_COMPILER=clang
      MONGOD_CONF=/usr/local/etc/mongod.conf
      OPENSSL_ROOT_DIR=/usr/local/opt/openssl
   fi

   ${SOURCE_DIR}/scripts/clean_old_install.sh
   if [ $? -ne 0 ]; then
      printf "\\n\\tError occurred while trying to remove old installation!\\n\\n"
      exit -1
   fi

   . "$FILE"

   printf "\\n\\n>>>>>>>> ALL dependencies sucessfully found or installed . Installing GSTIO\\n\\n"
   printf ">>>>>>>> CMAKE_BUILD_TYPE=%s\\n" "${CMAKE_BUILD_TYPE}"
   printf ">>>>>>>> ENABLE_COVERAGE_TESTING=%s\\n" "${ENABLE_COVERAGE_TESTING}"
   printf ">>>>>>>> DOXYGEN=%s\\n\\n" "${DOXYGEN}"

   if [ ! -d "${BUILD_DIR}" ]; then
      if ! mkdir -p "${BUILD_DIR}"
      then
         printf "Unable to create build directory %s.\\n Exiting now.\\n" "${BUILD_DIR}"
         exit 1;
      fi
   fi

   if ! cd "${BUILD_DIR}"
   then
      printf "Unable to enter build directory %s.\\n Exiting now.\\n" "${BUILD_DIR}"
      exit 1;
   fi

   if [ -z "$CMAKE" ]; then
      CMAKE=$( command -v cmake )
   fi

   if ! "${CMAKE}" -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" -DCMAKE_CXX_COMPILER="${CXX_COMPILER}" \
      -DCMAKE_C_COMPILER="${C_COMPILER}" -DWASM_ROOT="${WASM_ROOT}" -DCORE_SYMBOL_NAME="${CORE_SYMBOL_NAME}" \
      -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR}" -DBUILD_MONGO_DB_PLUGIN=true \
      -DENABLE_COVERAGE_TESTING="${ENABLE_COVERAGE_TESTING}" -DBUILD_DOXYGEN="${DOXYGEN}" \
      -DCMAKE_INSTALL_PREFIX="/usr/local/gstio" ${LOCAL_CMAKE_FLAGS} "${SOURCE_DIR}"
   then
      printf "\\n\\t>>>>>>>>>>>>>>>>>>>> CMAKE building GSTIO has exited with the above error.\\n\\n"
      exit -1
   fi

   if [ "${START_MAKE}" == "false" ]; then
      printf "\\n\\t>>>>>>>>>>>>>>>>>>>> GSTIO has been successfully configured but not yet built.\\n\\n"
      exit 0
   fi

   if [ -z ${JOBS} ]; then JOBS=$CPU_CORE; fi # Future proofing: Ensure $JOBS is set (usually set in scripts/gstio_build_*.sh scripts)
   if ! make -j"${JOBS}"
   then
      printf "\\n\\t>>>>>>>>>>>>>>>>>>>> MAKE building GSTIO has exited with the above error.\\n\\n"
      exit -1
   fi

   TIME_END=$(( $(date -u +%s) - ${TIME_BEGIN} ))

   printf "\n\n${bldred}\t ____ ____ _____ ___ ___\n"
   printf '\t / ___/ ___|_   _|_ _/ _ \ \n'
   printf "\t| |  _\___ \ | |  | | | | |\n"
   printf "\t| |_| |___) || |  | | |_| |\n"
   printf "\t \____|____/ |_| |___\___/ \n${txtrst}"

   printf "\\n\\tGSTIO has been successfully built. %02d:%02d:%02d\\n\\n" $(($TIME_END/3600)) $(($TIME_END%3600/60)) $(($TIME_END%60))
   printf "\\tTo verify your installation run the following commands:\\n"

   print_instructions

   printf "\\tFor more information:\\n"
   printf "\\tGSTIO website: https://github.com/gsthot/gst\\n"
