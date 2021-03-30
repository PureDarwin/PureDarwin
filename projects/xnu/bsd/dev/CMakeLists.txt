install_xnu_headers(TARGET_NAME xnu.headers.bsd_dev DESTINATION kernel SUBDIRECTORY dev/random FILES random/randomdev.h)

add_subdirectory(dtrace/scripts)
