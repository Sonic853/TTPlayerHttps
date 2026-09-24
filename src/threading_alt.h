#ifndef MTM_THREADING_ALT_H
#define MTM_THREADING_ALT_H
#include <windows.h>
/* The linker resolves Vista condition variables through YY-Thunks on XP. */
typedef CRITICAL_SECTION mbedtls_platform_mutex_t;
typedef CONDITION_VARIABLE mbedtls_platform_condition_variable_t;
#endif
