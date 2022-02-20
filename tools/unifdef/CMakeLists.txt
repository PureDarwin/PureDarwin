# unifdef tool. This is very simple, we create an executable from one source file,
#   and install the unifdefall shell script just in case.

# BEGIN EXECUTABLE UNIFDEF

add_executable(unifdef)

target_sources(unifdef PRIVATE unifdef.c)

install(TARGETS unifdef DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT HostTools)

# END EXECUTABLE UNIFDEF

# BEGIN SCRIPT UNIFDEFALL

install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/unifdefall.sh
    DESTINATION ${CMAKE_INSTALL_PREFIX}
    RENAME unifdefall
    COMPONENT HostTools
)

# END SCRIPT UNIFDEFALL
