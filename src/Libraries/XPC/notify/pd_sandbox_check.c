#include <stdarg.h>
#include "sandbox.h"

int
sandbox_check_by_audit_token(audit_token_t audit, const char *operation, sandbox_filter_type_t type, ...)
{
    (void)audit;
    (void)operation;
    (void)type;
    return 0;
}
