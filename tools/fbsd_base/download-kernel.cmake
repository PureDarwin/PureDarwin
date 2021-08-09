if(NOT OUTDIR)
    message(FATAL_ERROR "OUTDIR variable must be specified")
endif()

if(NOT EXISTS ${OUTDIR}/kernel.txz)
    message(STATUS "Downloading amd64 kernel.txz")
    file(DOWNLOAD "https://download.freebsd.org/ftp/releases/amd64/amd64/13.0-RELEASE/kernel.txz" ${OUTDIR}/kernel.txz)
endif()

message(STATUS "Unpacking amd64 kernel.txz")
file(MAKE_DIRECTORY ${OUTDIR}/kernel)
execute_process(
    COMMAND tar -x -f ${OUTDIR}/kernel.txz
    WORKING_DIRECTORY ${OUTDIR}/kernel
)
