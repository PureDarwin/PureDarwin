#!/bin/sh

# Dump adapter script to deal with `unifdef` returning 1 on success
# After rdar://113347229 (Update unifdef) we should be able to get
# rid of this and do `unifdef -x2` in the CMake command.
unifdef "$@"
returnValue=$?
if [ \( $returnValue -ne 0 \) -a \( $returnValue -ne 1 \) ]
then
    exit $returnValue
fi
