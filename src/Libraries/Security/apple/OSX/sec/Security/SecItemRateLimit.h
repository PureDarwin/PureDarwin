#ifndef SECITEMRATELIMIT_H_
#define SECITEMRATELIMIT_H_

#include <stdbool.h>

bool isReadOnlyAPIRateWithinLimits(void);
bool isModifyingAPIRateWithinLimits(void);

#endif // SECITEMRATELIMIT_H_
