#include <windows.h>
#include <wincrypt.h>
#include <stdlib.h>
#include <string.h>
#include "mbed_tls_min.h"
#include "mbedtls/ssl.h"
#include "mbedtls/threading.h"
#include "mbedtls/error.h"
#include "psa/crypto.h"
#include "ca_bundle.h"
#if MBEDTLS_VERSION_NUMBER != 0x04020000 || TF_PSA_CRYPTO_VERSION_NUMBER != 0x01020000
#error "This wrapper requires Mbed TLS 4.2.0 and TF-PSA-Crypto 1.2.0"
#endif

struct mtm_session {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_x509_crt ca;
    mtm_send_fn send;
    mtm_recv_fn recv;
    void *io_context;
    int ready;
};
static volatile LONG initialized;
static int initialize_result;
static mbedtls_x509_crt builtin_ca;

static int mutex_init(mbedtls_platform_mutex_t *m) {
    return InitializeCriticalSectionAndSpinCount(m, 0) ? 0 : PSA_ERROR_INSUFFICIENT_MEMORY;
}
static void mutex_free(mbedtls_platform_mutex_t *m) {
    DeleteCriticalSection(m);
}
static int mutex_lock(mbedtls_platform_mutex_t *m) {
    EnterCriticalSection(m); return 0;
}
static int mutex_unlock(mbedtls_platform_mutex_t *m) {
    LeaveCriticalSection(m); return 0;
}
static int cond_init(mbedtls_platform_condition_variable_t *c) {
    InitializeConditionVariable(c); return 0;
}
static void cond_free(mbedtls_platform_condition_variable_t *c) { (void)c; }
static int cond_signal(mbedtls_platform_condition_variable_t *c) {
    WakeConditionVariable(c); return 0;
}
static int cond_broadcast(mbedtls_platform_condition_variable_t *c) {
    WakeAllConditionVariable(c); return 0;
}
static int cond_wait(mbedtls_platform_condition_variable_t *c, mbedtls_platform_mutex_t *m) {
    return SleepConditionVariableCS(c, m, INFINITE) ? 0 : PSA_ERROR_BAD_STATE;
}
/* CryptoAPI's cryptographic RNG is available on XP. Never use rand(),
   timestamps or a fixed seed. Fail closed if the OS cannot produce randomness. */
psa_status_t mbedtls_psa_external_get_random(mbedtls_psa_external_random_context_t *data,
        unsigned char *out, size_t len, size_t *olen) {
    HCRYPTPROV provider = 0;
    BOOL ok;
    (void)data;
    *olen = 0;
    if (len > MAXDWORD || !CryptAcquireContextW(&provider, NULL, NULL,
            PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
        return PSA_ERROR_INSUFFICIENT_ENTROPY;
    ok = CryptGenRandom(provider, (DWORD)len, out);
    CryptReleaseContext(provider, 0);
    if (!ok) return PSA_ERROR_INSUFFICIENT_ENTROPY;
    *olen = len;
    return 0;
}
int mtm_initialize(void) {
    LONG previous = InterlockedCompareExchange(&initialized, 1, 0);
    if (previous == 0) {
        size_t i, offset = 0;
        mbedtls_threading_set_alt(mutex_init, mutex_free, mutex_lock, mutex_unlock,
            cond_init, cond_free, cond_signal, cond_broadcast, cond_wait);
        initialize_result = (int)psa_crypto_init();
        mbedtls_x509_crt_init(&builtin_ca);
        for (i = 0; !initialize_result && i < sizeof(mtm_ca_lengths)/sizeof(mtm_ca_lengths[0]); ++i) {
            initialize_result = mbedtls_x509_crt_parse_der_nocopy(&builtin_ca,
                mtm_ca_bundle + offset, mtm_ca_lengths[i]);
            offset += mtm_ca_lengths[i];
        }
        InterlockedExchange(&initialized, 2);
    } else {
        while (InterlockedCompareExchange(&initialized, 2, 2) != 2) Sleep(1);
    }
    return initialize_result;
}
static int send_data(void *ctx, const unsigned char *data, size_t size) {
    mtm_session *s = (mtm_session *)ctx;
    int r = s->send(s->io_context, data, size);
    if (r == MTM_IO_AGAIN) return MBEDTLS_ERR_SSL_WANT_WRITE;
    return r < 0 || (size_t)r > size ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : r;
}
static int recv_data(void *ctx, unsigned char *data, size_t size) {
    mtm_session *s = (mtm_session *)ctx;
    int r = s->recv(s->io_context, data, size);
    if (r == MTM_IO_AGAIN) return MBEDTLS_ERR_SSL_WANT_READ;
    return r < 0 || (size_t)r > size ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : r;
}
static void __cdecl destroy(mtm_session *s) {
    if (!s) return;
    mbedtls_ssl_free(&s->ssl);
    mbedtls_ssl_config_free(&s->config);
    mbedtls_x509_crt_free(&s->ca);
    free(s);
}
static int __cdecl create(const mtm_config *c, mtm_session **result) {
    mtm_session *s;
    uint32_t lo, hi;
    size_t i, length;
    int r;
    if (!result) return MTM_BAD_ARGUMENT;
    *result = NULL;
    if (!c || c->size != sizeof(*c) || !c->hostname || !c->send || !c->recv)
        return MTM_BAD_ARGUMENT;
    length = strlen(c->hostname);
    if (!length || length > 253) return MTM_BAD_ARGUMENT;
    for (i = 0; i < length; ++i)
        if ((unsigned char)c->hostname[i] <= 32 || (unsigned char)c->hostname[i] >= 127)
            return MTM_BAD_ARGUMENT;
    lo = c->min_version ? c->min_version : MTM_TLS12;
    hi = c->max_version ? c->max_version : MTM_TLS13;
    if (lo < MTM_TLS12 || hi > MTM_TLS13 || lo > hi) return MTM_BAD_ARGUMENT;
    if (c->ca_pem && (!c->ca_pem_size || c->ca_pem_size > 2u*1024u*1024u ||
            c->ca_pem[c->ca_pem_size-1] != 0)) return MTM_BAD_ARGUMENT;
    if (!c->ca_pem && c->ca_pem_size) return MTM_BAD_ARGUMENT;
    r = mtm_initialize();
    if (r) return r;
    s = (mtm_session *)calloc(1, sizeof(*s));
    if (!s) return MTM_NO_MEMORY;
    mbedtls_ssl_init(&s->ssl);
    mbedtls_ssl_config_init(&s->config);
    mbedtls_x509_crt_init(&s->ca);
    s->send = c->send; s->recv = c->recv; s->io_context = c->io_context;
    r = mbedtls_ssl_config_defaults(&s->config, MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (!r && c->ca_pem)
        r = mbedtls_x509_crt_parse(&s->ca, c->ca_pem, c->ca_pem_size);
    if (!r) {
        mbedtls_ssl_conf_authmode(&s->config, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&s->config, c->ca_pem ? &s->ca : &builtin_ca, NULL);
        mbedtls_ssl_conf_min_tls_version(&s->config, (mbedtls_ssl_protocol_version)lo);
        mbedtls_ssl_conf_max_tls_version(&s->config, (mbedtls_ssl_protocol_version)hi);
        r = mbedtls_ssl_setup(&s->ssl, &s->config);
    }
    if (!r) r = mbedtls_ssl_set_hostname(&s->ssl, c->hostname);
    if (r) { destroy(s); return r; }
    mbedtls_ssl_set_bio(&s->ssl, s, send_data, recv_data, NULL);
    *result = s;
    return 0;
}
static int __cdecl handshake(mtm_session *s) {
    int r;
    if (!s) return MTM_BAD_ARGUMENT;
    r = mbedtls_ssl_handshake(&s->ssl);
    if (!r && mbedtls_ssl_get_verify_result(&s->ssl))
        r = MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
    s->ready = r == 0;
    return r;
}
static int __cdecl read_data(mtm_session *s, unsigned char *data, size_t size) {
    int r;
    if (!s || !s->ready || !data || !size || size > 0x7fffffff) return MTM_BAD_ARGUMENT;
    r = mbedtls_ssl_read(&s->ssl, data, size);
    if (r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return 0;
    /* mbedtls_ssl_read maps an unannounced TCP EOF to zero. Keep this distinct
       from close_notify so close-delimited HTTP cannot accept truncated data. */
    return r == 0 ? MTM_TRUNCATED : r;
}
static int __cdecl write_data(mtm_session *s, const unsigned char *data, size_t size) {
    if (!s || !s->ready || !data || !size || size > 0x7fffffff) return MTM_BAD_ARGUMENT;
    return mbedtls_ssl_write(&s->ssl, data, size);
}
static int __cdecl info(mtm_session *s, mtm_info *i) {
    const char *cipher;
    if (!s || !i || i->size != sizeof(*i)) return MTM_BAD_ARGUMENT;
    i->protocol = s->ready ? (uint32_t)mbedtls_ssl_get_version_number(&s->ssl) : 0;
    i->verify_flags = mbedtls_ssl_get_verify_result(&s->ssl);
    cipher = mbedtls_ssl_get_ciphersuite(&s->ssl);
    if (!cipher) cipher = "";
    strncpy(i->ciphersuite, cipher, sizeof(i->ciphersuite)-1);
    i->ciphersuite[sizeof(i->ciphersuite)-1] = 0;
    return 0;
}
static void __cdecl describe(int code, char *buffer, size_t size) {
    const char *message = NULL;
    if (!buffer || !size) return;
    if (code == MTM_BAD_ARGUMENT) message = "Invalid TLS API argument";
    else if (code == MTM_NO_MEMORY) message = "TLS allocation failed";
    else if (code == MTM_TRUNCATED) message = "TLS connection closed without close_notify";
    else if (code == PSA_ERROR_INSUFFICIENT_MEMORY) message = "PSA allocation/key storage exhausted";
    if (message) {
        strncpy(buffer, message, size - 1); buffer[size - 1] = 0; return;
    }
    mbedtls_strerror(code, buffer, size);
}
static const mtm_api api = { sizeof(mtm_api), MTM_ABI_VERSION,
    MBEDTLS_VERSION_STRING_FULL " / " TF_PSA_CRYPTO_VERSION_STRING_FULL " / ABI 1", "Mozilla/curl 2026-08-13",
    create, handshake, read_data, write_data, info, destroy, describe };
const mtm_api *__cdecl mtm_get_api(uint32_t version) {
    return version == MTM_ABI_VERSION ? &api : NULL;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved) {
    (void)module;
    if (reason == DLL_PROCESS_DETACH && !reserved && initialized == 2) {
        mbedtls_x509_crt_free(&builtin_ca);
        mbedtls_psa_crypto_free();
        mbedtls_threading_free_alt();
    }
    return TRUE;
}
