#ifndef MBED_TLS_MIN_H
#define MBED_TLS_MIN_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Stable C ABI. No CRT allocations, Mbed TLS types or C++ objects cross the DLL. */
#define MTM_ABI_VERSION 1u
#define MTM_TLS12 0x0303u
#define MTM_TLS13 0x0304u
#define MTM_IO_ERROR (-1)
#define MTM_IO_AGAIN (-2)
#define MTM_WANT_READ (-0x6900)
#define MTM_WANT_WRITE (-0x6880)
#define MTM_BAD_ARGUMENT (-0x10001)
#define MTM_NO_MEMORY (-0x10002)
#define MTM_TRUNCATED (-0x10003)
typedef struct mtm_session mtm_session;
typedef int (__cdecl *mtm_send_fn)(void *, const unsigned char *, size_t);
typedef int (__cdecl *mtm_recv_fn)(void *, unsigned char *, size_t);
typedef struct mtm_config {
    uint32_t size;
    const char *hostname; /* ASCII DNS name for SNI and certificate verification. */
    mtm_send_fn send;
    mtm_recv_fn recv;
    void *io_context;
    /* NULL selects the embedded Mozilla CA bundle. A supplied PEM replaces it;
       intended for explicitly configured private trust and isolated tests. */
    const unsigned char *ca_pem;
    size_t ca_pem_size; /* Includes its terminating NUL. */
    uint32_t min_version; /* 0 -> TLS 1.2. */
    uint32_t max_version; /* 0 -> TLS 1.3. */
} mtm_config;
typedef struct mtm_info {
    uint32_t size;
    uint32_t protocol;
    uint32_t verify_flags;
    char ciphersuite[96];
} mtm_info;
typedef struct mtm_api {
    uint32_t size;
    uint32_t abi_version;
    const char *library_version;
    const char *ca_bundle_version;
    int (__cdecl *create)(const mtm_config *, mtm_session **);
    int (__cdecl *handshake)(mtm_session *);
    int (__cdecl *read)(mtm_session *, unsigned char *, size_t);
    int (__cdecl *write)(mtm_session *, const unsigned char *, size_t);
    int (__cdecl *info)(mtm_session *, mtm_info *);
    void (__cdecl *destroy)(mtm_session *);
    void (__cdecl *describe_error)(int, char *, size_t);
} mtm_api;
typedef const mtm_api *(__cdecl *mtm_get_api_fn)(uint32_t);
int mtm_initialize(void); /* Internal thread-safe PSA/CA initialization. */
const mtm_api *__cdecl mtm_get_api(uint32_t abi_version);
/* A session belongs to one caller at a time. Different sessions may run on
   different threads. Keep the DLL loaded until all sessions are destroyed.
   Callbacks return a byte count, 0 for EOF, MTM_IO_ERROR, or MTM_IO_AGAIN.
   handshake/read/write can return MTM_WANT_READ / MTM_WANT_WRITE. */
#ifdef __cplusplus
}
#endif
#endif
