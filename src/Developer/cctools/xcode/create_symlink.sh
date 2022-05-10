#!/bin/sh
#
# create_symlink creates a symlink in 'dir' pointing to 'src-file'
#
# Currently create_symlink will create 'dir' if necessary. It does not require
# 'src-file' to exist (see ln -s usage). Any pre-existing 'dst-file' will be
# removed from 'dir' before the link is made.

usage() {
  echo `basename $0`: error: $* 1>&2
  echo usage: `basename $0` 'dir' 'src-file' 'dst-file' 1>&2
  exit 1
}

[ $# -gt 3  -o $# -lt 3 ] && usage "wrong number of arguments"

dir=$1; shift
src_file=$1; shift
dst_file=$1; shift

mkdir -p $dir
( cd $dir; rm -f $dst_file; ln -s $src_file $dst_file )
