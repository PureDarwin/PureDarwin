if(NOT OUTDIR)
    message(FATAL_ERROR "OUTDIR variable must be specified")
endif()

if(NOT EXISTS ${OUTDIR}/base.txz)
    message(STATUS "Downloading amd64 base.txz")
    file(DOWNLOAD "https://download.freebsd.org/ftp/releases/amd64/amd64/13.0-RELEASE/base.txz" ${OUTDIR}/base.txz)
endif()

message(STATUS "Unpacking amd64 kernel.txz")
file(MAKE_DIRECTORY ${OUTDIR}/base)
execute_process(
    COMMAND tar --include=./lib --include=./usr/include --include=./usr/lib -x -f ${OUTDIR}/base.txz
    WORKING_DIRECTORY ${OUTDIR}/base
)
