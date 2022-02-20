#!/bin/sh
#
# copy_if copies a file into a directory if a condition is true.
#
# Currently copy_if assumes 'src-file' is a plain file that can be copied with
# the cp command. The file cannot be renamed in-flight. the 'dst-dir' directory
# will be created if necessary, and any pre-existing file will be removed from
# the destination before the copy is attempted.
#
# The condition is considered true iff the condition parameter is present and
# it has a non-NULL value. So:
#
#   condition=NULL  == FALSE
#   condition=""    == FALSE
#   condition="YES" == TRUE
#   condition="NO"  == TRUE
#
# Why such a weird usage? This is working around Xcode's inability to drive
# whole targets and whole build phases conditionally, something that's trivial
# in a Makefile.

usage() {
  echo `basename $0`: error: $* 1>&2
  echo usage: `basename $0` 'src-file' 'dst-dir' 'condition' 1>&2
  exit 1
}

[ $# -gt 3  -o $# -lt 2 ] && usage "wrong number of arguments"

src_file=$1; shift
dst_dir=$1; shift
condition=$1; shift
src_base=`basename ${src_file}`

if [ "${INSTALL_MODE_FLAG}" != "" ]; then
  mode=${INSTALL_MODE_FLAG}
else
  mode="u+w,go-w,a+rX"
fi

if [ "${condition}" != "" ]; then
  echo "copying ${src_file} to ${dst_dir}"
  
  mkdir -p ${dst_dir};
  rm -f ${dst_dir}/${src_base}
  cp ${src_file} ${dst_dir}/
  chmod ${mode} ${dst_dir}/${src_base}
fi
