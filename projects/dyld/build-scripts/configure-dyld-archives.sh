# link with all .a files in /usr/local/lib/dyld
ls -1 ${SDKROOT}/usr/local/lib/dyld/*.a | grep -v libcompiler_rt > ${DERIVED_SOURCES_DIR}/archives.txt

# link with crash report archive if it exists
if [ -f ${SDKROOT}/usr/local/lib/libCrashReporterClient.a ]
then
  echo \"${SDKROOT}/usr/local/lib/libCrashReporterClient.a\" >> ${DERIVED_SOURCES_DIR}/archives.txt
fi

# link with crypto archive if it exists
if [ -f ${SDKROOT}/usr/local/lib/libcorecrypto_static.a ]
then
  echo \"${SDKROOT}/usr/local/lib/libcorecrypto_static.a\" >> ${DERIVED_SOURCES_DIR}/archives.txt
fi

