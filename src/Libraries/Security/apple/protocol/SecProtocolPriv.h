//
//  SecProtocolPriv.h
//  Security
//

#ifndef SecProtocolPriv_h
#define SecProtocolPriv_h

#include <Security/SecProtocolOptions.h>
#include <Security/SecProtocolMetadata.h>
#include <Security/SecProtocolConfiguration.h>
#include <Security/SecureTransportPriv.h>
#include <Security/SecCertificatePriv.h>

#include <xpc/xpc.h>

__BEGIN_DECLS

/* See: https://tools.ietf.org/html/rfc8446#section-4.2.7 */
typedef CF_ENUM(uint16_t, tls_key_exchange_group_t) {
    tls_key_exchange_group_Secp256r1 = 0x0017,
    tls_key_exchange_group_Secp384r1 = 0x0018,
    tls_key_exchange_group_Secp521r1 = 0x0019,
    tls_key_exchange_group_X25519 = 0x001D,
    tls_key_exchange_group_X448 = 0x001E,
    tls_key_exchange_group_FFDHE2048 = 0x0100,
    tls_key_exchange_group_FFDHE3072 = 0x0101,
    tls_key_exchange_group_FFDHE4096 = 0x0102,
    tls_key_exchange_group_FFDHE6144 = 0x0103,
    tls_key_exchange_group_FFDHE8192 = 0x0104,
};

/*
 * Convenience key exchange groups that collate group identifiers of
 * comparable security into a single alias.
 */
typedef CF_ENUM(uint16_t, tls_key_exchange_group_set_t) {
    tls_key_exchange_group_set_default,
    tls_key_exchange_group_set_compatibility,
    tls_key_exchange_group_set_legacy,
};

SEC_ASSUME_NONNULL_BEGIN

#ifndef SEC_OBJECT_IMPL
SEC_OBJECT_DECL(sec_array);
#endif // !SEC_OBJECT_IMPL

struct sec_protocol_options_content;
typedef struct sec_protocol_options_content *sec_protocol_options_content_t;

struct sec_protocol_metadata_content;
typedef struct sec_protocol_metadata_content *sec_protocol_metadata_content_t;

typedef void (^sec_protocol_tls_handshake_message_handler_t)(uint8_t type, dispatch_data_t message);

typedef dispatch_data_t _Nullable (*sec_protocol_metadata_exporter)(void * handle, size_t label_len, const char *label,
                                                          size_t context_len, const uint8_t * __nullable context, size_t exporter_len);

typedef dispatch_data_t _Nullable (*sec_protocol_metadata_session_exporter)(void *handle);

typedef bool (^sec_access_block_t)(void *handle);

API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
SEC_RETURNS_RETAINED sec_array_t
sec_array_create(void);

API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
void
sec_array_append(sec_array_t array, sec_object_t object);

API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
size_t
sec_array_get_count(sec_array_t array);

typedef bool (^sec_array_applier_t) (size_t index, sec_object_t object);

API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
bool
sec_array_apply(sec_array_t array, sec_array_applier_t applier);

/*!
 * @function sec_protocol_options_access_handle
 *
 * @abstract
 *      Access the internal handle of a `sec_protocol_options` object.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param access_block
 *      A block to invoke with access to the internal handle.
 *
 * @return True if the access was successful
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
bool
sec_protocol_options_access_handle(sec_protocol_options_t options, sec_access_block_t access_block);

/*!
 * @function sec_protocol_options_contents_are_equal
 *
 * @abstract
 *      Compare two `sec_protocol_options_content_t` structs for equality.
 *
 * @param contentA
 *      A `sec_protocol_options_t` instance.
 *
 * @param contentB
 *      A `sec_protocol_options_t` instance.
 *
 * @return True if equal, and false otherwise.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
bool
sec_protocol_options_contents_are_equal(sec_protocol_options_content_t contentA, sec_protocol_options_content_t contentB);

/*!
 * @function sec_protocol_options_set_tls_early_data_enabled
 *
 * @abstract
 *      Enable or disable early (0-RTT) data for TLS.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param early_data_enabled
 *      Flag to enable or disable early (0-RTT) data.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
void
sec_protocol_options_set_tls_early_data_enabled(sec_protocol_options_t options, bool early_data_enabled);

/*!
 * @function sec_protocol_options_set_tls_sni_disabled
 *
 * @abstract
 *      Enable or disable the TLS SNI extension. This defaults to `false`.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param sni_disabled
 *      Flag to enable or disable use of the TLS SNI extension.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
void
sec_protocol_options_set_tls_sni_disabled(sec_protocol_options_t options, bool sni_disabled);

/*!
 * @function sec_protocol_options_set_enforce_ev
 *
 * @abstract
 *      Enable or disable EV enforcement.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param enforce_ev
 *      Flag to determine if EV is enforced.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
void
sec_protocol_options_set_enforce_ev(sec_protocol_options_t options, bool enforce_ev);

/*!
 * @block sec_protocol_session_update_t
 *
 * @abstract
 *      Block to be invoked when a new session is established and ready.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 */
typedef void (^sec_protocol_session_update_t)(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_options_set_session_update_block
 *
 * @abstract
 *      Set the session update block. This is fired whenever a new session is
 *      created an inserted into the cache.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param update_block
 *      A `sec_protocol_session_update_t` instance.
 *
 * @params update_queue
 *      A `dispatch_queue_t` on which the update block should be called.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
void
sec_protocol_options_set_session_update_block(sec_protocol_options_t options,
                                              sec_protocol_session_update_t update_block,
                                              dispatch_queue_t update_queue);

/*!
 * @function sec_protocol_options_set_session_state
 *
 * @abstract
 *      Set the session state using a serialized session blob.
 *
 *      If the session state is invalid or otherwise corrupt, the state is ignored and
 *      the connection will proceed as if no state was provided.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param session_state
 *      A `dispatch_data_t` carrying serialized session state from a previous.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
void
sec_protocol_options_set_session_state(sec_protocol_options_t options, dispatch_data_t session_state);

/*!
 * @function sec_protocol_options_set_quic_transport_parameters
 *
 * @abstract
 *      Set the opaque QUIC transport parameters to be used for this connection.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param transport_parameters
 *      A `dispatch_data_t` carrying opqaue QUIC transport parameters.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_set_quic_transport_parameters(sec_protocol_options_t options, dispatch_data_t transport_parameters);

/*!
 * @enum sec_protocol_transport_t
 *
 * @abstract An enumeration of the different transport protocols that can have specific security options.
 */
typedef enum {
    sec_protocol_transport_any = 0,
    sec_protocol_transport_tcp,
    sec_protocol_transport_quic,
} sec_protocol_transport_t;

#define SEC_PROTOCOL_HAS_TRANSPORT_SPECIFIC_ALPN 1

/*!
 * @function sec_protocol_options_add_transport_specific_application_protocol
 *
 * @abstract
 *      Add an application protocol supported by clients of this protocol instance, specific
 *      to a transport protocol.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param application_protocol
 *      A NULL-terminated string defining the application protocol.
 *
 * @param specific_transport
 *      A specific transport to which to bind the application protocol.
 */
API_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0))
void
sec_protocol_options_add_transport_specific_application_protocol(sec_protocol_options_t options, const char *application_protocol, sec_protocol_transport_t specific_transport);

/*!
 * @function sec_protocol_options_copy_transport_specific_application_protocol
 *
 * @abstract
 *      Return the application protocols configured by clients of this protocol instance, specific
 *      to a transport protocol if applicable.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param specific_transport
 *      A specific transport to which to bind the application protocol.
 *
 * @return An `xpc_object_t` instance carrying an array of application protocol strings, or nil.
 */
#define SEC_PROTOCOL_HAS_TRANSPORT_SPECIFIC_ALPN_GETTER 1 /* rdar://problem/63987477 */
SPI_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0))
SEC_RETURNS_RETAINED __nullable xpc_object_t
sec_protocol_options_copy_transport_specific_application_protocol(sec_protocol_options_t options, sec_protocol_transport_t specific_transport);

/*!
 * @enum sec_protocol_tls_encryption_level_t
 *
 * @abstract An enumeration of the different TLS encryption levels.
 */
typedef enum {
    sec_protocol_tls_encryption_level_initial = 0,
    sec_protocol_tls_encryption_level_early_data,
    sec_protocol_tls_encryption_level_handshake,
    sec_protocol_tls_encryption_level_application,
} sec_protocol_tls_encryption_level_t;

/*!
 * @block sec_protocol_tls_encryption_secret_update_t
 *
 * @abstract
 *      Block to be invoked when a new session is established and ready.
 *
 * @param level
 *      The `sec_protocol_tls_encryption_level_t` for this secret.
 *
 * @param is_write
 *      True if this secret is for writing, and false if it's for reading.
 *
 * @param secret
 *      Secret wrapped in a `dispatch_data_t`
 */
typedef void (^sec_protocol_tls_encryption_secret_update_t)(sec_protocol_tls_encryption_level_t level, bool is_write, dispatch_data_t secret);

/*!
 * @function sec_protocol_options_set_tls_encryption_secret_update_block
 *
 * @abstract
 *      Set the TLS secret update block. This is fired whenever a new TLS secret is
 *      available.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param update_block
 *      A `sec_protocol_tls_encryption_secret_update_t` instance.
 *
 * @params update_queue
 *      A `dispatch_queue_t` on which the update block should be called.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_set_tls_encryption_secret_update_block(sec_protocol_options_t options,
                                                            sec_protocol_tls_encryption_secret_update_t update_block,
                                                            dispatch_queue_t update_queue);

/*!
 * @block sec_protocol_tls_encryption_level_update_t
 *
 * @abstract
 *      Block to be invoked when the encryption level is updated.
 *
 * @param level
 *      The new `sec_protocol_tls_encryption_level_t`.
 *
 * @param is_write
 *      True if this is a write level and false if it's a read.
 *
 */
typedef void (^sec_protocol_tls_encryption_level_update_t)(sec_protocol_tls_encryption_level_t level, bool is_write);

/*!
 * @function sec_protocol_options_set_tls_encryption_level_update_block
 *
 * @abstract
 *      Set the TLS encryption level update block. It is invoked whenever the encryption level is updated.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param update_block
 *      A `sec_protocol_tls_encryption_level_update_t` instance.
 *
 * @params update_queue
 *      A `dispatch_queue_t` on which the update block should be called.
 */
#define SEC_PROTOCOL_HAS_TLS_ENCRYPTION_LEVEL_UPDATE_BLOCK 1 /* rdar://problem/63986462 */
SPI_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0))
void
sec_protocol_options_set_tls_encryption_level_update_block(sec_protocol_options_t options,
                                                            sec_protocol_tls_encryption_level_update_t update_block,
                                                            dispatch_queue_t update_queue);

/*!
 * @block sec_protocol_private_key_complete_t
 *
 * @abstract
 *      Block to be invoked when a private key operation is complete.
 *
 * @param result
 *      A `dispatch_data_t` object containing the private key result.
 */
typedef void (^sec_protocol_private_key_complete_t)(dispatch_data_t result);

/*!
 * @block sec_protocol_private_key_sign_t
 *
 * @abstract
 *      Block to be invoked when a private key signature operation is required.
 *
 * @param algorithm
 *      The signature algorithm to use for the signature.
 *
 * @param input
 *      The input to be signed.
 *
 * @param complete
 *      The `sec_protocol_private_key_complete_t` block to invoke when the operation is complete.
 */
typedef void (^sec_protocol_private_key_sign_t)(uint16_t algorithm, dispatch_data_t input, sec_protocol_private_key_complete_t complete);

/*!
 * @block sec_protocol_private_key_decrypt_t
 *
 * @abstract
 *      Block to be invoked when a private key decryption operation is required.
 *
 * @param input
 *      The input to be decrypted.
 *
 * @param complete
 *      The `sec_protocol_private_key_complete_t` block to invoke when the operation is complete.
 */
typedef void (^sec_protocol_private_key_decrypt_t)(dispatch_data_t input, sec_protocol_private_key_complete_t complete);

/*!
 * @block sec_protocol_options_set_private_key_blocks
 *
 * @abstract
 *      Set the private key operation blocks for this connection.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param sign_block
 *      A `sec_protocol_private_key_sign_t` block.
 *
 * @param decrypt_block
 *      A `sec_protocol_private_key_decrypt_t` block.
 *
 * @param operation_queue
 *      The `dispatch_queue_t` queue on which each private key operation is invoked.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_set_private_key_blocks(sec_protocol_options_t options,
                                            sec_protocol_private_key_sign_t sign_block,
                                            sec_protocol_private_key_decrypt_t decrypt_block,
                                            dispatch_queue_t operation_queue);

/*!
 * @block sec_protocol_options_set_local_certificates
 *
 * @abstract
 *      Set the local certificates to be used for this protocol instance.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param certificates
 *      A `sec_array_t` instance of `sec_certifiate_t` instances.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_set_local_certificates(sec_protocol_options_t options, sec_array_t certificates);

/*!
 * @block sec_protocol_options_set_tls_certificate_compression_enabled
 *
 * @abstract
 *      Enable or disable TLS 1.3 certificate compression.
 *
 *      See: https://tools.ietf.org/html/draft-ietf-tls-certificate-compression-04
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param certificate_compression_enabled
 *      Flag to determine if certificate compression is enabled.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_set_tls_certificate_compression_enabled(sec_protocol_options_t options, bool certificate_compression_enabled);

/*!
 * @block sec_protocol_options_tls_handshake_message_callback
 *
 * @abstract
 *      Set a callback to process each TLS handshake message. This function may be invoked at any point during
 *      the TLS handshake, if at all. Clients MUST NOT rely on any behavior aspect of this function as they
 *      risk breaking.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param handler
 *      A `sec_protocol_tls_handshake_message_handler_t`.
 *
 * @param queue
 *      The queue upon which to invoke the callback.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_tls_handshake_message_callback(sec_protocol_options_t options, sec_protocol_tls_handshake_message_handler_t handler, dispatch_queue_t queue);

/*!
 * @block sec_protocol_options_append_tls_key_exchange_group
 *
 * @abstract
 *      Append a TLS key exchange group to the set of enabled groups.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param group
 *      A `tls_key_exchange_group_t` value.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_append_tls_key_exchange_group(sec_protocol_options_t options, tls_key_exchange_group_t group);

/*!
 * @block sec_protocol_options_add_tls_key_exchange_group
 *
 * @abstract
 *      Add a TLS key exchange group to the set of enabled groups.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param group
 *      A SSLKeyExchangeGroup value.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_add_tls_key_exchange_group(sec_protocol_options_t options, SSLKeyExchangeGroup group);

/*!
 * @block sec_protocol_options_append_tls_key_exchange_group_set
 *
 * @abstract
 *      Append a TLS key exchange group set to the set of enabled groups.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param set
 *      A `tls_key_exchange_group_set_t` value.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_append_tls_key_exchange_group_set(sec_protocol_options_t options, tls_key_exchange_group_set_t set);

/*!
 * @block sec_protocol_options_tls_key_exchange_group_set
 *
 * @abstract
 *      Add a TLS key exchange group set to the set of enabled groups.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param set
 *      A SSLKeyExchangeGroupSet value.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_add_tls_key_exchange_group_set(sec_protocol_options_t options, SSLKeyExchangeGroupSet set);

/*!
 * @function sec_protocol_options_set_eddsa_enabled
 *
 * @abstract
 *      Enable EDDSA support (for TLS 1.3).
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param eddsa_enabled
 *      Flag to enable EDDSA.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_set_eddsa_enabled(sec_protocol_options_t options, bool eddsa_enabled);

/*!
 * @function sec_protocol_options_set_tls_delegated_credentials_enabled
 *
 * @abstract
 *      Enable TLS delegated credentials support. See https://tools.ietf.org/html/draft-ietf-tls-subcerts-02.
 *
 *      DO NOT DEPEND ON THIS SPI. IT IS FOR EXPERIMENTAL PURPOSES AND SUBJECT TO REMOVAL WITHOUT ADVANCE NOTICE.
 *      BUILD BREAKAGE ISSUES WILL BE SENT TO THE CALLING PROJECT.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param tls_delegated_credentials_enabled
 *      Flag to enable TLS delegated credentials.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_set_tls_delegated_credentials_enabled(sec_protocol_options_t options, bool tls_delegated_credentials_enabled);

/*!
 * @function sec_protocol_options_set_tls_ticket_request_count
 *
 * @abstract
 *      Enable TLS ticket request support, and specify the count of tickets. Ticket support
 *      must also be explicitly enabled by `sec_protocol_options_set_tls_tickets_enabled`.
 *
 *      DO NOT DEPEND ON THIS SPI. IT IS FOR EXPERIMENTAL PURPOSES AND SUBJECT TO REMOVAL WITHOUT ADVANCE NOTICE.
 *      BUILD BREAKAGE ISSUES WILL BE SENT TO THE CALLING PROJECT.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param tls_ticket_request_count
 *      Set the amount of tickets to request from the server.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_set_tls_ticket_request_count(sec_protocol_options_t options, uint8_t tls_ticket_request_count);

/*!
 * @function sec_protocol_options_set_tls_grease_enabled
 *
 * @abstract
 *      Enable TLS GREASE support. See https://tools.ietf.org/html/draft-ietf-tls-grease-02.
 *
 *      DO NOT DEPEND ON THIS SPI. IT IS FOR EXPERIMENTAL PURPOSES AND SUBJECT TO REMOVAL WITHOUT ADVANCE NOTICE.
 *      BUILD BREAKAGE ISSUES WILL BE SENT TO THE CALLING PROJECT.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param tls_grease_enabled
 *      Flag to enable TLS GREASE.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_set_tls_grease_enabled(sec_protocol_options_t options, bool tls_grease_enabled);

/*!
 * @function sec_protocol_options_set_allow_unknown_alpn_protos
 *
 * @abstract
 *      Configure clients to accept server ALPN values they did not advertise.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param allow_unknown_alpn_protos
 *      Flag to enable or disable the use of unknown ALPN values.
 */
#define SEC_PROTOCOL_HAS_ALLOW_UNKNOWN_ALPN_PROTOS_SETTER 1 /* rdar://problem/64449512 */
SPI_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0))
void
sec_protocol_options_set_allow_unknown_alpn_protos(sec_protocol_options_t options, bool allow_unknown_alpn_protos);

/*!
 * @function sec_protocol_options_set_experiment_identifier
 *
 * @abstract
 *      Set the SecExperiment identifier for a given connection.
 *
 *      Note: this SPI is meant to be called by libnetcore. It should not be called in any other circumstances.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param experiment_identifier
 *      The identifier for a secure connection experiment.
 */
#define SEC_PROTOCOL_HAS_EXPERIMENT_IDENTIFIER 1
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_set_experiment_identifier(sec_protocol_options_t options, const char *experiment_identifier);

/*!
 * @function sec_protocol_options_set_connection_id
 *
 * @abstract
 *      Set the explciit connection identifier. If not set, one will be populated internally.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param connection_id
 *      The `uuid_t`` connection identifier.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_set_connection_id(sec_protocol_options_t options, uuid_t _Nonnull connection_id);

/*!
 * @function sec_protocol_options_create_config
 *
 * @abstract
 *      Create a `xpc_object_t` instance carrying a configuration for the given `sec_protocol_options_t` instance.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @return A `xpc_object_t` instance carrying a configuration, or nil on failure.
 */
#define SEC_PROTOCOL_HAS_EXPERIMENT_HOOKS 1
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED __nullable xpc_object_t
sec_protocol_options_create_config(sec_protocol_options_t options);

/*!
 * @function sec_protocol_options_matches_config
 *
 * @abstract
 *      Determine if a `sec_protocol_options_t` instance matches a given configuration.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param config
 *      A `xpc_object_t` instance carrying a SecExperiment config.
 *
 * @return True if the parameters in `config` match that of `options`, and false otherwise.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
bool
sec_protocol_options_matches_config(sec_protocol_options_t options, xpc_object_t config);

/*!
 * @function sec_protocol_options_apply_config
 *
 * @abstract
 *      Transform the given `sec_protocol_options_t` instance using the provided config.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param config
 *      A `xpc_object_t` instance carrying a SecExperiment config.
 *
 * @return True if the options were applied successfully, and false otherwise.
 */
bool
sec_protocol_options_apply_config(sec_protocol_options_t options, xpc_object_t config);

/*!
 * @function sec_protocol_metadata_get_tls_negotiated_group
 *
 * @abstract
 *      Get a human readable representation of the negotiated key exchange group.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return A string representation of the negotiated group, or NULL if it does not exist.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
const char * __nullable
sec_protocol_metadata_get_tls_negotiated_group(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_experiment_identifier
 *
 * @abstract
 *      Get the SecExperiment identifier for a given connection.
 *
 *      Note: this SPI is meant to be called by libnetcore. It should not be called in any other circumstances.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return The identifier for a secure connection experiment, or NULL if none was specified.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
const char * __nullable
sec_protocol_metadata_get_experiment_identifier(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_copy_connection_id
 *
 * @abstract
 *      Copy the secure connection identifier.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @param output_uuid
 *      A `uuid_t` into which the connection identifier is written.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_metadata_copy_connection_id(sec_protocol_metadata_t metadata, uuid_t _Nonnull output_uuid);

/*!
 * @function sec_protocol_metadata_get_tls_false_start_used
 *
 * @abstract
 *      Determine if False Start was used.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return True if False Start was used, and false otherwise.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
bool
sec_protocol_metadata_get_tls_false_start_used(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_ticket_offered
 *
 * @abstract
 *      Determine if a ticket was offered for session resumption.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return True if a ticket was offered for resumption, and false otherwise.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
bool
sec_protocol_metadata_get_ticket_offered(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_ticket_received
 *
 * @abstract
 *      Determine if a ticket was received upon completing the new connection.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return True if a ticket was received from the peer (server), and false otherwise.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
bool
sec_protocol_metadata_get_ticket_received(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_session_resumed
 *
 * @abstract
 *      Determine if this new connection was a session resumption.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return True if this new connection was resumed, and false otherwise.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
bool
sec_protocol_metadata_get_session_resumed(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_session_renewed
 *
 * @abstract
 *      Determine if this resumed connection was renewed with a new ticket.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return True if this resumed connection was renewed with a new ticket, and false otherwise.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
bool
sec_protocol_metadata_get_session_renewed(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_connection_strength
 *
 * @abstract
 *      Determine the TLS connection strength.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return An `SSLConnectionStrength` enum.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
SSLConnectionStrength
sec_protocol_metadata_get_connection_strength(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_copy_serialized_session
 *
 * @abstract
 *      Copy a serialized representation of a session.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return A `dispatch_data_t` object containing a serialized session.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
SEC_RETURNS_RETAINED __nullable dispatch_data_t
sec_protocol_metadata_copy_serialized_session(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_access_handle
 *
 * @abstract
 *      Access the internal handle of a `sec_protocol_metadata` object.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @param access_block
 *      A block to invoke with access to the internal handle.
 *
 * @return True if the access was successful
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
bool
sec_protocol_metadata_access_handle(sec_protocol_metadata_t metadata, sec_access_block_t access_block);

/*!
 * @function sec_protocol_metadata_serialize_with_options
 *
 * @abstract
 *      Serialize a `sec_protocol_metadata_t` to an `xpc_object_t` dictionary using information
 *      contained in the `metadata` and `options` objects.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return A xpc_object_t carrying the serialized metadata.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
SEC_RETURNS_RETAINED __nullable xpc_object_t
sec_protocol_metadata_serialize_with_options(sec_protocol_metadata_t metadata, sec_protocol_options_t options);

/*!
 * @function sec_protocol_metadata_get_tls_certificate_compression_used
 *
 * @abstract
 *      Determine if certificate compression was used for a given connection.
 *
 *      See: https://tools.ietf.org/html/draft-ietf-tls-certificate-compression-04
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return True if certificate compression was negotiated and used.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
bool
sec_protocol_metadata_get_tls_certificate_compression_used(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_tls_certificate_compression_algorithm
 *
 * @abstract
 *      Return the certificate compression algorithm used. This will return 0
 *      if `sec_protocol_metadata_get_tls_certificate_compression_used` is false.
 *
 *      See: https://tools.ietf.org/html/draft-ietf-tls-certificate-compression-04
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return IANA codepoint for the certificate compression algorithm.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
uint16_t
sec_protocol_metadata_get_tls_certificate_compression_algorithm(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_copy_quic_transport_parameters
 *
 * @abstract
 *      Copy the peer's QUIC transport parameters.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return A dispatch_data_t carrying the connection peer's opaque QUIC tranport parameters.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED __nullable dispatch_data_t
sec_protocol_metadata_copy_quic_transport_parameters(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_handshake_time_ms
 *
 * @abstract
 *      Get the TLS handshake time in miliseconds. The result is undefined
 *      for connections not yet connected.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return A millisecond measurement of the TLS handshake time from start to finish.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
#define SEC_PROTOCOL_HAS_METRIC_SPI_V1
uint64_t
sec_protocol_metadata_get_handshake_time_ms(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_handshake_rtt
 *
 * @abstract
 *      Get the observed TLS handshake RTT. This function must only be 
 *      called after the connection is established. Calling this before
 *      the connection completes will yields an undefined result.
 *
 *      This is computed as the average RTT across all 1-RTT exchanges. 
 *      For TLS 1.3, this will be the time for the normal exchange. For prior
 *      versions, or TLS 1.3 with HRR, this will be the average RTT across
 *      multiple message flights.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return A millisecond measurement of the TLS handshake RTT.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
uint64_t
sec_protocol_metadata_get_handshake_rtt(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_handshake_byte_count
 *
 * @abstract
 *      Get the total number of bytes sent and received for the handshake.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return Number of bytes sent and received for the handshake.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
uint64_t
sec_protocol_metadata_get_handshake_byte_count(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_handshake_sent_byte_count
 *
 * @abstract
 *      Get the total number of bytes sent for the handshake.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return Number of bytes sent for the handshake.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
uint64_t
sec_protocol_metadata_get_handshake_sent_byte_count(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_handshake_received_byte_count
 *
 * @abstract
 *      Get the total number of bytes received for the handshake.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return Number of bytes received for the handshake.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
uint64_t
sec_protocol_metadata_get_handshake_received_byte_count(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_handshake_read_stall_count
 *
 * @abstract
 *      Get the total number of read stalls during the handshake.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return Number of read stalls.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
size_t
sec_protocol_metadata_get_handshake_read_stall_count(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_handshake_write_stall_count
 *
 * @abstract
 *      Get the total number of write stalls during the handshake.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return Number of write stalls.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
size_t
sec_protocol_metadata_get_handshake_write_stall_count(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_handshake_async_call_count
 *
 * @abstract
 *      Get the total number of asynchronous callbacks invoked during the handshake.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return Number of asynchronous callbacks.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
size_t
sec_protocol_metadata_get_handshake_async_call_count(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_copy_sec_trust
 *
 * @abstract
 *      Copy the `sec_trust_t` associated with a connection.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return A `sec_trust_t` instance.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED __nullable sec_trust_t
sec_protocol_metadata_copy_sec_trust(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_copy_sec_identity
 *
 * @abstract
 *      Copy the `sec_identity_t` associated with a connection.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return A `sec_identity_t` instance.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED __nullable sec_identity_t
sec_protocol_metadata_copy_sec_identity(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_access_sent_certificates
 *
 * @abstract
 *      Access the certificates which were sent to the peer on this connection.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @param handler
 *      A block to invoke one or more times with `sec_certificate_t` instances.
 *
 * @return Returns true if the peer certificates were accessible, false otherwise.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
bool
sec_protocol_metadata_access_sent_certificates(sec_protocol_metadata_t metadata,
                                               void (^handler)(sec_certificate_t certificate));

/*!
 * @function sec_protocol_metadata_get_tls_negotiated_group
 *
 * @abstract
 *      Get a human readable representation of the negotiated key exchange group.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return A string representation of the negotiated group, or NULL if it does not exist.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
const char * __nullable
sec_protocol_metadata_get_tls_negotiated_group(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_configuration_copy_singleton
 *
 * @abstract
 *      Copy the per-process `sec_protocol_configuration_t` object.
 *
 * @return A non-nil `sec_protocol_configuration_t` instance.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED sec_protocol_configuration_t
sec_protocol_configuration_copy_singleton(void);

#ifndef SEC_OBJECT_IMPL
SEC_OBJECT_DECL(sec_protocol_configuration_builder);
#endif // !SEC_OBJECT_IMPL

/*!
 * @function sec_protocol_configuration_builder_create
 *
 * @abstract
 *      This function is exposed for testing purposes only. It MUST NOT be called by clients.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED sec_protocol_configuration_builder_t
sec_protocol_configuration_builder_create(CFDictionaryRef dictionary, bool is_apple);

/*!
 * @function sec_protocol_configuration_create_with_builder
 *
 * @abstract
 *      This function is exposed for testing purposes only. It MUST NOT be called by clients.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED __nullable sec_protocol_configuration_t
sec_protocol_configuration_create_with_builder(sec_protocol_configuration_builder_t builder);

/*!
 * @block sec_protocol_output_handler_access_block_t
 *
 * @abstract
 *      Block to be invoked to obtain the output handler for a given encryption level.
 */
typedef void *_Nullable(^sec_protocol_output_handler_access_block_t)(sec_protocol_tls_encryption_level_t level);

/*!
 * @function sec_protocol_options_set_output_handler_access_block
 *
 * @abstract
 *      Set a block used to access output handler instances identified by encryption level.
 */
#define SEC_PROTOCOL_HAS_QUIC_OUTPUT_HANDLER_ACCESS_BLOCK 1
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_protocol_options_set_output_handler_access_block(sec_protocol_options_t options,
                                                     sec_protocol_output_handler_access_block_t access_block);

/*!
 * @function sec_protocol_helper_ciphersuite_group_to_ciphersuite_list
 *
 * @abstract
 *      Return a pointer to a statically allocated list of ciphersuites corresponding to `group`.
 *
 * @param group
 *      A `tls_ciphersuite_group_t` instance.
 *
 * @param list_count
 *      Pointer to storage for the ciphersuite list length.
 *
 * @return Pointer to a statically allocated list, or NULL if an error occurred.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
const tls_ciphersuite_t * __nullable
sec_protocol_helper_ciphersuite_group_to_ciphersuite_list(tls_ciphersuite_group_t group, size_t *list_count);

typedef CF_ENUM(uint16_t, sec_protocol_block_length_padding_t) {
    SEC_PROTOCOL_BLOCK_LENGTH_PADDING_NONE = 0,
    SEC_PROTOCOL_BLOCK_LENGTH_PADDING_DEFAULT = 16,
};

/*!
 * @function sec_protocol_options_set_tls_block_length_padding
 *
 * @abstract
 *      Pad TLS messages to a multiple of the specified block length. By default, padding is disabled.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param block_length_padding
 *      A sec_protocol_block_length_padding_t variable specifying the block length padding. Setting the block length padding to 0 disables padding.
 *
 * @return True if the padding policy has been successfully set, false otherwise.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
bool
sec_protocol_options_set_tls_block_length_padding(sec_protocol_options_t options, sec_protocol_block_length_padding_t block_length_padding);

/*!
 * @function sec_protocol_helper_ciphersuite_group_contains_ciphersuite
 *
 * @abstract
 *      This function is exposed for testing purposes only. It MUST NOT be called by clients.
 *
 * @param group
 *      A `tls_ciphersuite_group_t` instance.
 *
 * @param suite
 *      A `tls_ciphersuite_t` instance.
 *
 * @return True if the ciphersuite group contains the given ciphersuite, false otherwise.
*/
API_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0))
bool
sec_protocol_helper_ciphersuite_group_contains_ciphersuite(tls_ciphersuite_group_t group, tls_ciphersuite_t suite);

/*!
 * @function sec_protocol_helper_ciphersuite_minimum_TLS_version
 *
 * @abstract
 *      This function is exposed for testing purposes only. It MUST NOT be called by clients.
 *
 * @param ciphersuite
 *      A `tls_ciphersuite_t` instance.
 *
 * @return The `tls_protocol_version_t` pertaining to the minimum TLS version designated for the given ciphersuite.
*/
API_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0))
tls_protocol_version_t
sec_protocol_helper_ciphersuite_minimum_TLS_version(tls_ciphersuite_t ciphersuite);

/*!
 * @function sec_protocol_helper_ciphersuite_maximum_TLS_version
 *
 * @abstract
 *      This function is exposed for testing purposes only. It MUST NOT be called by clients.
 *
 * @param ciphersuite
 *      A `tls_ciphersuite_t` instance.
 *
 * @return The `tls_protocol_version_t` pertaining to the maximum TLS version designated for the given ciphersuite.
*/
API_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0))
tls_protocol_version_t
sec_protocol_helper_ciphersuite_maximum_TLS_version(tls_ciphersuite_t ciphersuite);

/*!
 * @function sec_protocol_helper_get_ciphersuite_name
 *
 * @abstract
 *      This function is exposed for testing purposes only. It MUST NOT be called by clients.
 *
 * @param ciphersuite
 *      A `tls_ciphersuite_t` instance.
 *
 * @return A string representation of the given ciphersuite, or NULL if it does not exist.
*/
API_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0))
const char * __nullable
sec_protocol_helper_get_ciphersuite_name(tls_ciphersuite_t ciphersuite);

#define SEC_PROTOCOL_HAS_MULTI_PSK_SUPPORT 1

struct sec_protocol_options_content {
    SSLProtocol min_version;
    SSLProtocol max_version;

    char *server_name;
    char *experiment_identifier;
    uuid_t connection_id;
    __nullable xpc_object_t ciphersuites;
    xpc_object_t application_protocols;
    sec_identity_t identity;
    sec_array_t certificates;
    xpc_object_t pre_shared_keys;
    dispatch_data_t psk_identity_hint;
    sec_protocol_key_update_t key_update_block;
    dispatch_queue_t key_update_queue;
    sec_protocol_challenge_t challenge_block;
    dispatch_queue_t challenge_queue;
    sec_protocol_verify_t verify_block;
    dispatch_queue_t verify_queue;
    dispatch_data_t quic_transport_parameters;
    sec_protocol_tls_encryption_secret_update_t tls_secret_update_block;
    dispatch_queue_t tls_secret_update_queue;
    sec_protocol_tls_encryption_level_update_t tls_encryption_level_update_block;
    dispatch_queue_t tls_encryption_level_update_queue;
    sec_protocol_session_update_t session_update_block;
    dispatch_queue_t session_update_queue;
    dispatch_data_t session_state;
    sec_protocol_private_key_sign_t private_key_sign_block;
    sec_protocol_private_key_decrypt_t private_key_decrypt_block;
    dispatch_queue_t private_key_queue;
    dispatch_data_t dh_params;
    xpc_object_t key_exchange_groups;
    sec_protocol_tls_handshake_message_handler_t handshake_message_callback;
    dispatch_queue_t handshake_message_callback_queue;
    sec_protocol_pre_shared_key_selection_t psk_selection_block;
    dispatch_queue_t psk_selection_queue;

    // ATS minimums
    size_t minimum_rsa_key_size;
    size_t minimum_ecdsa_key_size;
    SecSignatureHashAlgorithm minimum_signature_algorithm;

    // Non-boolean options
    uint8_t tls_ticket_request_count;

    // QUIC-specific access block
    sec_protocol_output_handler_access_block_t output_handler_access_block;

    // Boolean flags
    unsigned ats_required : 1;
    unsigned ats_minimum_tls_version_allowed : 1;
    unsigned ats_non_pfs_ciphersuite_allowed : 1;
    unsigned trusted_peer_certificate : 1;
    unsigned trusted_peer_certificate_override : 1;
    unsigned disable_sni : 1;
    unsigned disable_sni_override : 1;
    unsigned enable_fallback_attempt : 1;
    unsigned enable_fallback_attempt_override : 1;
    unsigned enable_false_start : 1;
    unsigned enable_false_start_override : 1;
    unsigned enable_tickets : 1;
    unsigned enable_tickets_override : 1;
    unsigned enable_sct : 1;
    unsigned enable_sct_override : 1;
    unsigned enable_ocsp : 1;
    unsigned enable_ocsp_override : 1;
    unsigned enforce_ev : 1;
    unsigned enforce_ev_override : 1;
    unsigned enable_resumption : 1;
    unsigned enable_resumption_override : 1;
    unsigned enable_renegotiation : 1;
    unsigned enable_renegotiation_override : 1;
    unsigned enable_early_data : 1;
    unsigned enable_early_data_override : 1;
    unsigned enable_ech : 1;
    unsigned peer_authentication_required : 1;
    unsigned peer_authentication_optional : 1;
    unsigned peer_authentication_override : 1;
    unsigned certificate_compression_enabled : 1;
    unsigned eddsa_enabled : 1;
    unsigned tls_delegated_credentials_enabled : 1;
    unsigned tls_grease_enabled : 1;
    unsigned allow_unknown_alpn_protos : 1;
    unsigned allow_unknown_alpn_protos_override : 1;

    sec_protocol_block_length_padding_t tls_block_length_padding;
};

struct sec_protocol_metadata_content {
    void *exporter_context; // Opaque context for the exporter function
    sec_protocol_metadata_exporter exporter_function; // Exporter function pointer. This MUST be set by the metadata allocator.
    void *session_exporter_context; // Opaque context for the session exporter function
    sec_protocol_metadata_session_exporter session_exporter_function;

    SSLProtocol negotiated_protocol_version;
    SSLCipherSuite negotiated_ciphersuite;
    const char *negotiated_protocol;
    const char *server_name;
    const char *experiment_identifier;
    uuid_t connection_id;

    sec_array_t sent_certificate_chain;
    sec_array_t peer_certificate_chain;
    xpc_object_t pre_shared_keys;
    dispatch_data_t peer_public_key;
    xpc_object_t supported_signature_algorithms;
    dispatch_data_t request_certificate_types;
    sec_array_t signed_certificate_timestamps;
    sec_array_t ocsp_response;
    sec_array_t distinguished_names;
    dispatch_data_t quic_transport_parameters;
    sec_identity_t identity;
    sec_trust_t trust_ref;
    const char *negotiated_curve;
    const char *peer_public_key_type;
    const char *certificate_request_type;
    uint64_t ticket_lifetime;
    uint64_t max_early_data_supported;
    uint64_t alert_type;
    uint64_t alert_code;
    uint64_t handshake_state;
    uint64_t stack_error;
    uint64_t handshake_rtt;
    uint16_t certificate_compression_algorithm;
    uint64_t handshake_time;
    uint64_t total_byte_count;
    uint64_t sent_byte_count;
    uint64_t received_byte_count;
    size_t read_stall_count;
    size_t write_stall_count;
    size_t async_call_count;

    unsigned failure : 1;
    unsigned sct_enabled : 1;
    unsigned ocsp_enabled : 1;
    unsigned early_data_accepted : 1;
    unsigned false_start_used : 1;
    unsigned ticket_offered : 1;
    unsigned ticket_received : 1;
    unsigned session_resumed : 1;
    unsigned session_renewed : 1;
    unsigned resumption_attempted : 1;
    unsigned alpn_used : 1;
    unsigned npn_used : 1;
    unsigned early_data_sent : 1;
    unsigned certificate_compression_used : 1;
};

SEC_ASSUME_NONNULL_END

__END_DECLS

#endif /* SecProtocolPriv_h */
