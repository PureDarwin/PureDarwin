# The INSTALL_NAME_DIR is not used, but it fixes a warning.
add_darwin_shared_library(booter INSTALL_NAME_DIR "/System/Library/CoreServices")
target_link_libraries(booter PRIVATE libefi)
target_link_options(booter PRIVATE -static -e _EfiMain -seg1addr 0x1000)

target_sources(booter PRIVATE
    main.c
    util.c
)


add_custom_command(TARGET booter POST_BUILD
    COMMAND host_mtoc -subsystem application -e _EfiMain $<TARGET_FILE:booter> ${CMAKE_CURRENT_BINARY_DIR}/boot.efi
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/boot.efi
    COMMENT "mtoc boot.efi" VERBATIM
)
add_dependencies(booter host_mtoc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/boot.efi DESTINATION System/Library/CoreServices COMPONENT BaseSystem)
