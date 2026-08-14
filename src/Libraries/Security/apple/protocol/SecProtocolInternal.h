//
//  SecProtocolInternal.h
//  Security
//

#ifndef SecProtocolInternal_h
#define SecProtocolInternal_h

#include "SecProtocolPriv.h"

#define kATSInfoKey "NSAppTransportSecurity"
#define kAllowsArbitraryLoads "NSAllowsArbitraryLoads"
#define kAllowsArbitraryLoadsForMedia "NSAllowsArbitraryLoadsForMedia"
#define kAllowsArbitraryLoadsInWebContent "NSAllowsArbitraryLoadsInWebContent"
#define kAllowsLocalNetworking "NSAllowsLocalNetworking"
#define kExceptionDomains "NSExceptionDomains"
#define kIncludesSubdomains "NSIncludesSubdomains"
#define kExceptionAllowsInsecureHTTPLoads "NSExceptionAllowsInsecureHTTPLoads"
#define kExceptionMinimumTLSVersion "NSExceptionMinimumTLSVersion"
#define kExceptionRequiresForwardSecrecy "NSExceptionRequiresForwardSecrecy"

#define CiphersuitesTLS13 \
    TLS_AES_128_GCM_SHA256, \
    TLS_AES_256_GCM_SHA384, \
    TLS_CHACHA20_POLY1305_SHA256

#define CiphersuitesPFS \
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, \
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, \
    TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, \
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384, \
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, \
    TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, \
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384, \
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256, \
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, \
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, \
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384, \
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256, \
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA, \
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA

#define CiphersuitesNonPFS \
    TLS_RSA_WITH_AES_256_GCM_SHA384, \
    TLS_RSA_WITH_AES_128_GCM_SHA256, \
    TLS_RSA_WITH_AES_256_CBC_SHA256, \
    TLS_RSA_WITH_AES_128_CBC_SHA256, \
    TLS_RSA_WITH_AES_256_CBC_SHA, \
    TLS_RSA_WITH_AES_128_CBC_SHA

#define CiphersuitesTLS10 \
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, \
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, \
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA, \
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA, \
    TLS_RSA_WITH_AES_256_CBC_SHA, \
    TLS_RSA_WITH_AES_128_CBC_SHA

#define CiphersuitesTLS10_3DES \
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA, \
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA, \
    SSL_RSA_WITH_3DES_EDE_CBC_SHA

#define CiphersuitesDHE \
    TLS_DHE_RSA_WITH_AES_256_GCM_SHA384, \
    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256, \
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA256, \
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA256, \
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA, \
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA, \
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA

SEC_RETURNS_RETAINED sec_protocol_configuration_builder_t
sec_protocol_configuration_builder_copy_default(void);

CFDictionaryRef
sec_protocol_configuration_builder_get_ats_dictionary(sec_protocol_configuration_builder_t builder);

bool
sec_protocol_configuration_builder_get_is_apple_bundle(sec_protocol_configuration_builder_t builder);

SEC_RETURNS_RETAINED xpc_object_t
sec_protocol_configuration_get_map(sec_protocol_configuration_t configuration);

void
sec_protocol_options_clear_tls_ciphersuites(sec_protocol_options_t options);

void
sec_protocol_options_set_ats_non_pfs_ciphersuite_allowed(sec_protocol_options_t options, bool ats_non_pfs_ciphersuite_allowed);

void
sec_protocol_options_set_ats_minimum_tls_version_allowed(sec_protocol_options_t options, bool ats_minimum_tls_version_allowed);

void
sec_protocol_options_set_ats_required(sec_protocol_options_t options, bool required);

void
sec_protocol_options_set_minimum_rsa_key_size(sec_protocol_options_t options, size_t minimum_key_size);

void
sec_protocol_options_set_minimum_ecdsa_key_size(sec_protocol_options_t options, size_t minimum_key_size);

void
sec_protocol_options_set_minimum_signature_algorithm(sec_protocol_options_t options, SecSignatureHashAlgorithm algorithm);

void
sec_protocol_options_set_trusted_peer_certificate(sec_protocol_options_t options, bool trusted_peer_certificate);

void
sec_protocol_configuration_populate_insecure_defaults(sec_protocol_configuration_t configuration);

void
sec_protocol_configuration_populate_secure_defaults(sec_protocol_configuration_t configuration);

void
sec_protocol_configuration_register_builtin_exceptions(sec_protocol_configuration_t configuration);

const tls_key_exchange_group_t *
sec_protocol_helper_tls_key_exchange_group_set_to_key_exchange_group_list(tls_key_exchange_group_set_t set, size_t *listSize);

bool sec_protocol_helper_dispatch_data_equal(dispatch_data_t left, dispatch_data_t right);

#endif /* SecProtocolInternal_h */
