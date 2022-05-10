#!/bin/sh
#
# install as OpenSource license files into /usr/local

install_files() {
  src_dir=$1; shift
  dst_dir=$1; shift
  files=$*

  (
    cd ${src_dir}
    mkdir -p ${dst_dir}
    install -c -m 444 ${files} ${dst_dir}
  )
}

src=${SRCROOT}/as
dst=${DSTROOT}/${CCTOOLS_USRLOCAL}

install_files ${src} ${dst}/OpenSourceVersions cctools.plist
install_files ${src} ${dst}/OpenSourceLicenses COPYING
mv ${dst}/OpenSourceLicenses/COPYING ${dst}/OpenSourceLicenses/cctools.txt
