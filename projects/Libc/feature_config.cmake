set(FEATURE_LEGACY_64_APIS TRUE)
set(FEATURE_LEGACY_CRT1_ENVIRON TRUE)
set(FEATURE_LEGACY_RUNE_APIS TRUE)
set(FEATURE_THERM_NOTIFICATION_APIS TRUE)
set(FEATURE_ONLY_1050_VARIANTS TRUE)
set(FEATURE_ONLY_UNIX_CONFORMANCE TRUE)
set(FEATURE_ONLY_64_BIT_INO_T TRUE)
set(FEATURE_PATCH_3333969 TRUE)
set(FEATURE_PATCH_3417676 TRUE)
set(FEATURE_PLOCKSTAT TRUE)
set(FEATURE_TIMEZONE_CHANGE_NOTIFICATION TRUE)
set(FEATURE_XPRINTF_PERF TRUE)
set(FEATURE_SIGNAL_RESTRICTION FALSE)
set(FEATURE_POSIX_ILP32_ALLOW FALSE)


file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/config)
configure_file(libc-features.h.in ${CMAKE_CURRENT_BINARY_DIR}/config/libc-features.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/config) # This needs to be present in all targets in Libc.


macro(append_unifdef_arg var)
    if(${${var}})
        list(APPEND results -D${var})
    else()
        list(APPEND results -U${var})
    endif()
endmacro()

function(get_libc_feature_unifdef_args outvar)
    set(results) # create empty array

    list(APPEND results -DFEATURE_BLOCKS) # Hardcoded in generate_features.pl
    append_unifdef_arg(FEATURE_LEGACY_64_APIS)
    append_unifdef_arg(FEATURE_LEGACY_CRT1_ENVIRON)
    append_unifdef_arg(FEATURE_LEGACY_RUNE_APIS)
    append_unifdef_arg(FEATURE_THERM_NOTIFICATION_APIS)
    append_unifdef_arg(FEATURE_ONLY_1050_VARIANTS)
    append_unifdef_arg(FEATURE_ONLY_UNIX_CONFORMANCE)
    append_unifdef_arg(FEATURE_ONLY_64_BIT_INO_T)
    append_unifdef_arg(FEATURE_PATCH_3333969)
    append_unifdef_arg(FEATURE_PATCH_3417676)
    append_unifdef_arg(FEATURE_PLOCKSTAT)
    append_unifdef_arg(FEATURE_TIMEZONE_CHANGE_NOTIFICATION)
    append_unifdef_arg(FEATURE_XPRINTF_PERF)
    append_unifdef_arg(FEATURE_SIGNAL_RESTRICTION)
    append_unifdef_arg(FEATURE_POSIX_ILP32_ALLOW)

    set(${outvar} ${results} PARENT_SCOPE)
endfunction()
