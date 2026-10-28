// PureDarwin GSS: the GSS-API with no security mechanisms installed
#ifndef GSS_GSSAPI_H
#define GSS_GSSAPI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t OM_uint32;

typedef struct gss_OID_desc_struct {
    OM_uint32 length;
    void *elements;
} gss_OID_desc, *gss_OID;

typedef struct gss_buffer_desc_struct {
    size_t length;
    void *value;
} gss_buffer_desc, *gss_buffer_t;

typedef struct gss_OID_set_desc_struct {
    size_t count;
    gss_OID elements;
} gss_OID_set_desc, *gss_OID_set;

typedef struct gss_name_t_desc_struct *gss_name_t;
typedef struct gss_cred_id_t_desc_struct *gss_cred_id_t;
typedef struct gss_ctx_id_t_desc_struct *gss_ctx_id_t;
typedef struct gss_channel_bindings_struct *gss_channel_bindings_t;
typedef int gss_cred_usage_t;

#define GSS_C_NO_NAME ((gss_name_t)0)
#define GSS_C_NO_CREDENTIAL ((gss_cred_id_t)0)
#define GSS_C_NO_CONTEXT ((gss_ctx_id_t)0)
#define GSS_C_NO_OID ((gss_OID)0)
#define GSS_C_NO_OID_SET ((gss_OID_set)0)
#define GSS_C_NO_BUFFER ((gss_buffer_t)0)
#define GSS_C_NO_CHANNEL_BINDINGS ((gss_channel_bindings_t)0)
#define GSS_C_EMPTY_BUFFER { 0, NULL }

#define GSS_C_INDEFINITE 0xffffffffU
#define GSS_C_BOTH 0
#define GSS_C_INITIATE 1
#define GSS_C_ACCEPT 2

#define GSS_C_GSS_CODE 1
#define GSS_C_MECH_CODE 2

#define GSS_C_DELEG_FLAG 1
#define GSS_C_MUTUAL_FLAG 2
#define GSS_C_REPLAY_FLAG 4
#define GSS_C_SEQUENCE_FLAG 8
#define GSS_C_CONF_FLAG 16
#define GSS_C_INTEG_FLAG 32

#define GSS_S_COMPLETE 0
#define GSS_S_CONTINUE_NEEDED 1
#define GSS_C_ROUTINE_ERROR_OFFSET 16
#define GSS_S_BAD_MECH (1U << GSS_C_ROUTINE_ERROR_OFFSET)
#define GSS_S_BAD_NAME (2U << GSS_C_ROUTINE_ERROR_OFFSET)
#define GSS_S_NO_CRED (7U << GSS_C_ROUTINE_ERROR_OFFSET)
#define GSS_S_NO_CONTEXT (8U << GSS_C_ROUTINE_ERROR_OFFSET)
#define GSS_S_FAILURE (13U << GSS_C_ROUTINE_ERROR_OFFSET)
#define GSS_S_UNAVAILABLE (16U << GSS_C_ROUTINE_ERROR_OFFSET)
#define GSS_ERROR(x) ((x) & 0xffff0000U)

extern gss_OID_desc __gss_c_nt_hostbased_service_oid_desc;
extern gss_OID_desc __gss_c_nt_user_name_oid_desc;
#define GSS_C_NT_HOSTBASED_SERVICE (&__gss_c_nt_hostbased_service_oid_desc)
#define GSS_C_NT_USER_NAME (&__gss_c_nt_user_name_oid_desc)

OM_uint32 gss_import_name(OM_uint32 *minor, const gss_buffer_t name, gss_OID type, gss_name_t *out);
OM_uint32 gss_release_name(OM_uint32 *minor, gss_name_t *name);
OM_uint32 gss_acquire_cred(OM_uint32 *minor, gss_name_t name, OM_uint32 time, gss_OID_set mechs,
    gss_cred_usage_t usage, gss_cred_id_t *cred, gss_OID_set *actualMechs, OM_uint32 *actualTime);
OM_uint32 gss_release_cred(OM_uint32 *minor, gss_cred_id_t *cred);
OM_uint32 gss_init_sec_context(OM_uint32 *minor, gss_cred_id_t cred, gss_ctx_id_t *ctx, gss_name_t target,
    gss_OID mech, OM_uint32 reqFlags, OM_uint32 time, gss_channel_bindings_t bindings, const gss_buffer_t input,
    gss_OID *actualMech, gss_buffer_t output, OM_uint32 *retFlags, OM_uint32 *retTime);
OM_uint32 gss_delete_sec_context(OM_uint32 *minor, gss_ctx_id_t *ctx, gss_buffer_t output);
OM_uint32 gss_release_buffer(OM_uint32 *minor, gss_buffer_t buffer);
OM_uint32 gss_display_status(OM_uint32 *minor, OM_uint32 status, int type, gss_OID mech, OM_uint32 *context,
    gss_buffer_t string);

#ifdef __cplusplus
}
#endif

#endif
