// GSS-API with no security mechanisms, what a Mac without Kerberos credentials answers too.
// Callers such as Qt's Negotiate authentication see it unavailable and fall back to other schemes
#include <GSS/gssapi.h>
#include <stdlib.h>
#include <string.h>

// 1.2.840.113554.1.2.1.4 and 1.2.840.113554.1.2.1.1
gss_OID_desc __gss_c_nt_hostbased_service_oid_desc = { 10, "\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04" };
gss_OID_desc __gss_c_nt_user_name_oid_desc = { 10, "\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x01" };

OM_uint32 gss_import_name(OM_uint32 *minor, const gss_buffer_t name, gss_OID type, gss_name_t *out)
{
    if (minor)
        *minor = 0;
    if (out)
        *out = GSS_C_NO_NAME;
    return GSS_S_UNAVAILABLE;
}

OM_uint32 gss_release_name(OM_uint32 *minor, gss_name_t *name)
{
    if (minor)
        *minor = 0;
    if (name)
        *name = GSS_C_NO_NAME;
    return GSS_S_COMPLETE;
}

OM_uint32 gss_acquire_cred(OM_uint32 *minor, gss_name_t name, OM_uint32 time, gss_OID_set mechs,
    gss_cred_usage_t usage, gss_cred_id_t *cred, gss_OID_set *actualMechs, OM_uint32 *actualTime)
{
    if (minor)
        *minor = 0;
    if (cred)
        *cred = GSS_C_NO_CREDENTIAL;
    if (actualMechs)
        *actualMechs = GSS_C_NO_OID_SET;
    if (actualTime)
        *actualTime = 0;
    return GSS_S_NO_CRED;
}

OM_uint32 gss_release_cred(OM_uint32 *minor, gss_cred_id_t *cred)
{
    if (minor)
        *minor = 0;
    if (cred)
        *cred = GSS_C_NO_CREDENTIAL;
    return GSS_S_COMPLETE;
}

OM_uint32 gss_init_sec_context(OM_uint32 *minor, gss_cred_id_t cred, gss_ctx_id_t *ctx, gss_name_t target,
    gss_OID mech, OM_uint32 reqFlags, OM_uint32 time, gss_channel_bindings_t bindings, const gss_buffer_t input,
    gss_OID *actualMech, gss_buffer_t output, OM_uint32 *retFlags, OM_uint32 *retTime)
{
    if (minor)
        *minor = 0;
    if (output) {
        output->length = 0;
        output->value = NULL;
    }
    if (actualMech)
        *actualMech = GSS_C_NO_OID;
    if (retFlags)
        *retFlags = 0;
    if (retTime)
        *retTime = 0;
    return GSS_S_UNAVAILABLE;
}

OM_uint32 gss_delete_sec_context(OM_uint32 *minor, gss_ctx_id_t *ctx, gss_buffer_t output)
{
    if (minor)
        *minor = 0;
    if (ctx)
        *ctx = GSS_C_NO_CONTEXT;
    if (output) {
        output->length = 0;
        output->value = NULL;
    }
    return GSS_S_COMPLETE;
}

OM_uint32 gss_release_buffer(OM_uint32 *minor, gss_buffer_t buffer)
{
    if (minor)
        *minor = 0;
    if (buffer) {
        free(buffer->value);
        buffer->length = 0;
        buffer->value = NULL;
    }
    return GSS_S_COMPLETE;
}

OM_uint32 gss_display_status(OM_uint32 *minor, OM_uint32 status, int type, gss_OID mech, OM_uint32 *context,
    gss_buffer_t string)
{
    const char *text = type == GSS_C_GSS_CODE && status == GSS_S_NO_CRED ? "No credentials were supplied" :
        type == GSS_C_GSS_CODE && status == GSS_S_COMPLETE ? "The routine completed successfully" :
        "No security mechanisms are installed";

    if (minor)
        *minor = 0;
    if (context)
        *context = 0;
    if (!string)
        return GSS_S_FAILURE;
    string->value = strdup(text);
    string->length = strlen(text);
    return GSS_S_COMPLETE;
}
