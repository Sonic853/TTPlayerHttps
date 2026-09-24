# Third-party notices

This binary links the following independently licensed components:

| Component | Version | License / source |
|---|---|---|
| Mbed TLS | 4.2.0 | Apache-2.0 option of Apache-2.0 OR GPL-2.0-or-later; [source](https://github.com/Mbed-TLS/mbedtls/releases/tag/mbedtls-4.2.0) |
| TF-PSA-Crypto | 1.2.0 | Apache-2.0 option of Apache-2.0 OR GPL-2.0-or-later; bundled in the above archive |
| YY-Thunks | 1.2.2 | MIT, Copyright (c) 2018 Chuyu-Team; [source](https://github.com/Chuyu-Team/YY-Thunks/tree/v1.2.2) |
| VC-LTL | 5.3.1 | EPL-2.0; [source](https://github.com/Chuyu-Team/VC-LTL5/tree/v5.3.1) |
| Mozilla CA certificate data | curl extraction 2026-08-13 | MPL-2.0; [exact PEM source](https://curl.se/ca/cacert-2026-08-13.pem) |

Copyright The Mbed TLS Contributors. The upstream TLS and crypto sources are
unmodified. This project supplies a reduced compile-time configuration, Windows
thread/RNG adapters, a C ABI wrapper and a DER representation of the CA data.
The unused post-quantum drivers in the source archive are not linked.

Full license texts and the original, unmodified CA PEM from which the DER array
is generated are retained in `licenses/` in this repository. Runtime archives
contain only AddIn/ttp_https.dll and SHA256SUMS.txt; release notes link to the
source and license files at the exact build commit.
VC-LTL's third-party CRT notices and source references remain available in its
[upstream documentation](https://github.com/Chuyu-Team/VC-LTL5#excursus---third-party-licenses).
