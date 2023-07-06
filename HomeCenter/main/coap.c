#include "coap.h"
#include "app_main.h"


#define COAP_DEFAULT_TIME_SEC 60

/* The examples use simple Pre-Shared-Key configuration that you can set via
   'idf.py menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_COAP_PSK_KEY "some-agreed-preshared-key"

   Note: PSK will only be used if the URI is prefixed with coaps://
   instead of coap:// and the PSK must be one that the server supports
   (potentially associated with the IDENTITY)
*/
#define EXAMPLE_COAP_PSK_KEY CONFIG_EXAMPLE_COAP_PSK_KEY
#define EXAMPLE_COAP_PSK_IDENTITY CONFIG_EXAMPLE_COAP_PSK_IDENTITY

/* The examples use uri Logging Level that
   you can set via 'idf.py menuconfig'.

   If you'd rather not, just change the below entry to a value
   that is between 0 and 7 with
   the config you want - ie #define EXAMPLE_COAP_LOG_DEFAULT_LEVEL 7
*/
#define EXAMPLE_COAP_LOG_DEFAULT_LEVEL CONFIG_COAP_LOG_DEFAULT_LEVEL

/* The examples use uri "coap://californium.eclipseprojects.io" that
   you can set via the project configuration (idf.py menuconfig)

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define COAP_DEFAULT_DEMO_URI "coaps://californium.eclipseprojects.io"
*/

const static char *TAG = "CoAP_client";

static int resp_wait = 1;
static coap_optlist_t *optlist = NULL;
static int wait_ms;



static coap_response_t
message_handler(coap_session_t *session,
                const coap_pdu_t *sent,
                const coap_pdu_t *received,
                const coap_mid_t mid)
{
    const unsigned char *data = NULL;
    size_t data_len;
    size_t offset;
    size_t total;
    coap_pdu_code_t rcvd_code = coap_pdu_get_code(received);
    if (COAP_RESPONSE_CLASS(rcvd_code) == 2) {
        if (coap_get_data_large(received, &data_len, &data, &offset, &total)) {
            if (data_len != total) {
                printf("Unexpected partial data received offset %u, length %u\n", offset, data_len);
            }
	        if (data[0] == 'o')
            {
                printf("motor status:%.*s\n", (int)data_len, data);
            }
            else 
            {
                temp_value_outdoor.temperature_value = (*data);
                temp_value_outdoor.ID_DEVICE = 1;
                //send to data_queue
                xQueueSend( data_queue,( void * ) &temp_value_outdoor,( TickType_t ) 10 );
                printf("outdoor temperature value: %d\n", (*data));
            }
            resp_wait = 0;
        }
        return COAP_RESPONSE_OK;
    }

    printf("%d.%02d", (rcvd_code >> 5), rcvd_code & 0x1F);
    if (coap_get_data_large(received, &data_len, &data, &offset, &total)) {
        printf(": ");
        while(data_len--) {
            printf("%c", isprint(*data) ? *data : '.');
            data++;
        }
    }
    printf("\n");
    resp_wait = 0;
    return COAP_RESPONSE_OK;
}

#ifdef CONFIG_COAP_MBEDTLS_PKI

static int
verify_cn_callback(const char *cn,
                   const uint8_t *asn1_public_cert,
                   size_t asn1_length,
                   coap_session_t *session,
                   unsigned depth,
                   int validated,
                   void *arg
                  )
{
    coap_log(LOG_INFO, "CN '%s' presented by server (%s)\n",
             cn, depth ? "CA" : "Certificate");
    return 1;
}
#endif /* CONFIG_COAP_MBEDTLS_PKI */

static void
coap_log_handler (coap_log_t level, const char *message)
{
    uint32_t esp_level = ESP_LOG_INFO;
    char *cp = strchr(message, '\n');

    if (cp)
        ESP_LOG_LEVEL(esp_level, TAG, "%.*s", (int)(cp-message), message);
    else
        ESP_LOG_LEVEL(esp_level, TAG, "%s", message);
}

static coap_address_t *
coap_get_address(coap_uri_t *uri)
{
  static coap_address_t dst_addr;
    char *phostname = NULL;
    struct addrinfo hints;
    struct addrinfo *addrres;
    int error;
    char tmpbuf[INET6_ADDRSTRLEN];

    phostname = (char *)calloc(1, uri->host.length + 1);
    if (phostname == NULL) {
        ESP_LOGE(TAG, "calloc failed");
        return NULL;
    }
    memcpy(phostname, uri->host.s, uri->host.length);

    memset ((char *)&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family = AF_UNSPEC;

    error = getaddrinfo(phostname, NULL, &hints, &addrres);
    if (error != 0) {
        ESP_LOGE(TAG, "DNS lookup failed for destination address %s. error: %d", phostname, error);
        free(phostname);
        return NULL;
    }
    if (addrres == NULL) {
        ESP_LOGE(TAG, "DNS lookup %s did not return any addresses", phostname);
        free(phostname);
        return NULL;
    }
    free(phostname);
    coap_address_init(&dst_addr);
    switch (addrres->ai_family) {
    case AF_INET:
        memcpy(&dst_addr.addr.sin, addrres->ai_addr, sizeof(dst_addr.addr.sin));
        dst_addr.addr.sin.sin_port        = htons(uri->port);
        inet_ntop(AF_INET, &dst_addr.addr.sin.sin_addr, tmpbuf, sizeof(tmpbuf));
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", tmpbuf);
        break;
    case AF_INET6:
        memcpy(&dst_addr.addr.sin6, addrres->ai_addr, sizeof(dst_addr.addr.sin6));
        dst_addr.addr.sin6.sin6_port        = htons(uri->port);
        inet_ntop(AF_INET6, &dst_addr.addr.sin6.sin6_addr, tmpbuf, sizeof(tmpbuf));
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", tmpbuf);
        break;
    default:
        ESP_LOGE(TAG, "DNS lookup response failed");
        return NULL;
    }
    freeaddrinfo(addrres);

    return &dst_addr;
}

static int
coap_build_optlist(coap_uri_t *uri)
{
#define BUFSIZE 40
    unsigned char _buf[BUFSIZE];
    unsigned char *buf;
    size_t buflen;
    int res;

    optlist = NULL;

    if (uri->scheme == COAP_URI_SCHEME_COAPS && !coap_dtls_is_supported()) {
        ESP_LOGE(TAG, "MbedTLS DTLS Client Mode not configured");
        return 0;
    }
    if (uri->scheme == COAP_URI_SCHEME_COAPS_TCP && !coap_tls_is_supported()) {
        ESP_LOGE(TAG, "MbedTLS TLS Client Mode not configured");
        return 0;
    }
    if (uri->scheme == COAP_URI_SCHEME_COAP_TCP && !coap_tcp_is_supported()) {
        ESP_LOGE(TAG, "TCP Client Mode not configured");
        return 0;
    }

    if (uri->path.length) {
        buflen = BUFSIZE;
        buf = _buf;
        res = coap_split_path(uri->path.s, uri->path.length, buf, &buflen);

        while (res--) {
            coap_insert_optlist(&optlist,
                                coap_new_optlist(COAP_OPTION_URI_PATH,
                                                 coap_opt_length(buf),
                                                 coap_opt_value(buf)));

            buf += coap_opt_size(buf);
        }
    }

    if (uri->query.length) {
        buflen = BUFSIZE;
        buf = _buf;
        res = coap_split_query(uri->query.s, uri->query.length, buf, &buflen);

        while (res--) {
            coap_insert_optlist(&optlist,
                                coap_new_optlist(COAP_OPTION_URI_QUERY,
                                                 coap_opt_length(buf),
                                                 coap_opt_value(buf)));

            buf += coap_opt_size(buf);
        }
    }
    return 1;
}
#ifdef CONFIG_COAP_MBEDTLS_PSK
static coap_session_t *
coap_start_psk_session(coap_context_t *ctx, coap_address_t *dst_addr, coap_uri_t *uri)
{
 static coap_dtls_cpsk_t dtls_psk;
 static char client_sni[256];

    memset(client_sni, 0, sizeof(client_sni));
    memset (&dtls_psk, 0, sizeof(dtls_psk));
    dtls_psk.version = COAP_DTLS_CPSK_SETUP_VERSION;
    dtls_psk.validate_ih_call_back = NULL;
    dtls_psk.ih_call_back_arg = NULL;
    if (uri->host.length)
        memcpy(client_sni, uri->host.s, MIN(uri->host.length, sizeof(client_sni) - 1));
    else
        memcpy(client_sni, "localhost", 9);
    dtls_psk.client_sni = client_sni;
    dtls_psk.psk_info.identity.s = (const uint8_t *)EXAMPLE_COAP_PSK_IDENTITY;
    dtls_psk.psk_info.identity.length = sizeof(EXAMPLE_COAP_PSK_IDENTITY)-1;
    dtls_psk.psk_info.key.s = (const uint8_t *)EXAMPLE_COAP_PSK_KEY;
    dtls_psk.psk_info.key.length = sizeof(EXAMPLE_COAP_PSK_KEY)-1;
    return coap_new_client_session_psk2(ctx, NULL, dst_addr,
                                       uri->scheme == COAP_URI_SCHEME_COAPS ? COAP_PROTO_DTLS : COAP_PROTO_TLS,
                                       &dtls_psk);
}
#endif /* CONFIG_COAP_MBEDTLS_PSK */

#ifdef CONFIG_COAP_MBEDTLS_PKI
static coap_session_t *
coap_start_pki_session(coap_context_t *ctx, coap_address_t *dst_addr, coap_uri_t *uri)
{
    unsigned int ca_pem_bytes = ca_pem_end - ca_pem_start;
    unsigned int client_crt_bytes = client_crt_end - client_crt_start;
    unsigned int client_key_bytes = client_key_end - client_key_start;
 static coap_dtls_pki_t dtls_pki;
 static char client_sni[256];

    memset (&dtls_pki, 0, sizeof(dtls_pki));
    dtls_pki.version = COAP_DTLS_PKI_SETUP_VERSION;
    if (ca_pem_bytes) {
        /*
         * Add in additional certificate checking.
         * This list of enabled can be tuned for the specific
         * requirements - see 'man coap_encryption'.
         *
         * Note: A list of root cas file can be setup separately using
         * coap_context_set_pki_root_cas(), but the below is used to
         * define what checking actually takes place.
         */
        dtls_pki.verify_peer_cert        = 1;
        dtls_pki.check_common_ca         = 1;
        dtls_pki.allow_self_signed       = 1;
        dtls_pki.allow_expired_certs     = 1;
        dtls_pki.cert_chain_validation   = 1;
        dtls_pki.cert_chain_verify_depth = 2;
        dtls_pki.check_cert_revocation   = 1;
        dtls_pki.allow_no_crl            = 1;
        dtls_pki.allow_expired_crl       = 1;
        dtls_pki.allow_bad_md_hash       = 1;
        dtls_pki.allow_short_rsa_length  = 1;
        dtls_pki.validate_cn_call_back   = verify_cn_callback;
        dtls_pki.cn_call_back_arg        = NULL;
        dtls_pki.validate_sni_call_back  = NULL;
        dtls_pki.sni_call_back_arg       = NULL;
        memset(client_sni, 0, sizeof(client_sni));
        if (uri->host.length) {
            memcpy(client_sni, uri->host.s, MIN(uri->host.length, sizeof(client_sni)));
        } else {
            memcpy(client_sni, "localhost", 9);
        }
        dtls_pki.client_sni = client_sni;
    }
    dtls_pki.pki_key.key_type = COAP_PKI_KEY_PEM_BUF;
    dtls_pki.pki_key.key.pem_buf.public_cert = client_crt_start;
    dtls_pki.pki_key.key.pem_buf.public_cert_len = client_crt_bytes;
    dtls_pki.pki_key.key.pem_buf.private_key = client_key_start;
    dtls_pki.pki_key.key.pem_buf.private_key_len = client_key_bytes;
    dtls_pki.pki_key.key.pem_buf.ca_cert = ca_pem_start;
    dtls_pki.pki_key.key.pem_buf.ca_cert_len = ca_pem_bytes;

    return coap_new_client_session_pki(ctx, NULL, dst_addr,
                                              uri->scheme == COAP_URI_SCHEME_COAPS ? COAP_PROTO_DTLS : COAP_PROTO_TLS,
                                              &dtls_pki);
}
#endif /* CONFIG_COAP_MBEDTLS_PKI */
void GetTemperature_Coap (void)
{
    coap_address_t   *dst_addr;
    static coap_uri_t uri;
    const char       *server_uri = COAP_DEFAULT_DEMO_URI;
    coap_context_t *ctx = NULL;
    unsigned char token[8];
    size_t tokenlength;
    coap_pdu_t *put_request = NULL;
    coap_pdu_t *get_request = NULL;
    coap_session_t *session = NULL;
    /* Set up the CoAP context */
    ctx = coap_new_context(NULL);
    if (!ctx) {
        ESP_LOGE(TAG, "coap_new_context() failed");
    }
    coap_context_set_block_mode(ctx,
                                COAP_BLOCK_USE_LIBCOAP|COAP_BLOCK_SINGLE_BODY);

    coap_register_response_handler(ctx, message_handler);

    if (coap_split_uri((const uint8_t *)server_uri, strlen(server_uri), &uri) == -1) {
        ESP_LOGE(TAG, "CoAP server uri error");
    }
    if (!coap_build_optlist(&uri))
        ESP_LOGE(TAG, "CoAP server error");

    dst_addr = coap_get_address(&uri);
    if (!dst_addr)
        ESP_LOGE(TAG, "CoAP server error");

    /*
     * Note that if the URI starts with just coap:// (not coaps://) the
     * session will still be plain text.
     */
    if (uri.scheme == COAP_URI_SCHEME_COAPS || uri.scheme == COAP_URI_SCHEME_COAPS_TCP) {
#ifndef CONFIG_MBEDTLS_TLS_CLIENT
        ESP_LOGE(TAG, "MbedTLS (D)TLS Client Mode not configured");

#endif /* CONFIG_MBEDTLS_TLS_CLIENT */

#ifdef CONFIG_COAP_MBEDTLS_PSK
        session = coap_start_psk_session(ctx, dst_addr, &uri);
#endif /* CONFIG_COAP_MBEDTLS_PSK */

#ifdef CONFIG_COAP_MBEDTLS_PKI
        session = coap_start_pki_session(ctx, dst_addr, &uri);
#endif /* CONFIG_COAP_MBEDTLS_PKI */
    } else {
        session = coap_new_client_session(ctx, NULL, dst_addr,
                                          uri.scheme == COAP_URI_SCHEME_COAP_TCP ? COAP_PROTO_TCP :
                                          COAP_PROTO_UDP);
    }
    if (!session) {
        ESP_LOGE(TAG, "coap_new_client_session() failed");
    }

    get_request = coap_new_pdu(coap_is_mcast(dst_addr) ? COAP_MESSAGE_NON : COAP_MESSAGE_CON,
                            COAP_REQUEST_CODE_GET, session);
    if (!get_request) {
        ESP_LOGE(TAG, "coap_new_pdu() failed");

    }
    /* Add in an unique token */
    coap_session_new_token(session, &tokenlength, token);
    coap_add_token(get_request, tokenlength, token);

    coap_add_optlist_pdu(get_request, &optlist);

    resp_wait = 1;
    coap_send(session, get_request);

    wait_ms =  2000;

    while (resp_wait) {
        int result = coap_io_process(ctx, wait_ms > 1000 ? 1000 : wait_ms);
        if (result >= 0) {
            if (result >= wait_ms) {
                ESP_LOGE(TAG, "No response from server");
                break;
            } else {
                wait_ms -= result;
            }
        }
    }

    if (optlist) {
        coap_delete_optlist(optlist);
        optlist = NULL;
    }
    if (session) {
        coap_session_release(session);
    }
    if (ctx) {
        coap_free_context(ctx);
    }
    coap_cleanup();
}
void ControlMotor_Coap (char *cmd)
{
    coap_address_t   *dst_addr;
    static coap_uri_t uri;
    const char       *server_uri = COAP_DEFAULT_DEMO_URI;
    coap_context_t *ctx = NULL;
    unsigned char token[8];
    size_t tokenlength;
    coap_pdu_t *put_request = NULL;
    coap_pdu_t *get_request = NULL;
    coap_session_t *session = NULL;
    /* Set up the CoAP context */
    ctx = coap_new_context(NULL);
    if (!ctx) {
        ESP_LOGE(TAG, "coap_new_context() failed");
    }
    coap_context_set_block_mode(ctx,
                                COAP_BLOCK_USE_LIBCOAP|COAP_BLOCK_SINGLE_BODY);

    coap_register_response_handler(ctx, message_handler);

    if (coap_split_uri((const uint8_t *)server_uri, strlen(server_uri), &uri) == -1) {
        ESP_LOGE(TAG, "CoAP server uri error");
    }
    if (!coap_build_optlist(&uri))
        ESP_LOGE(TAG, "CoAP server error");

    dst_addr = coap_get_address(&uri);
    if (!dst_addr)
        ESP_LOGE(TAG, "CoAP server error");

    /*
     * Note that if the URI starts with just coap:// (not coaps://) the
     * session will still be plain text.
     */
    if (uri.scheme == COAP_URI_SCHEME_COAPS || uri.scheme == COAP_URI_SCHEME_COAPS_TCP) {
#ifndef CONFIG_MBEDTLS_TLS_CLIENT
        ESP_LOGE(TAG, "MbedTLS (D)TLS Client Mode not configured");

#endif /* CONFIG_MBEDTLS_TLS_CLIENT */

#ifdef CONFIG_COAP_MBEDTLS_PSK
        session = coap_start_psk_session(ctx, dst_addr, &uri);
#endif /* CONFIG_COAP_MBEDTLS_PSK */

#ifdef CONFIG_COAP_MBEDTLS_PKI
        session = coap_start_pki_session(ctx, dst_addr, &uri);
#endif /* CONFIG_COAP_MBEDTLS_PKI */
    } else {
        session = coap_new_client_session(ctx, NULL, dst_addr,
                                          uri.scheme == COAP_URI_SCHEME_COAP_TCP ? COAP_PROTO_TCP :
                                          COAP_PROTO_UDP);
    }
    if (!session) {
        ESP_LOGE(TAG, "coap_new_client_session() failed");
    }

    put_request = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_PUT,
                coap_new_message_id(session) /* message id */,
                coap_session_max_pdu_size(session));
    if (!put_request) {
        ESP_LOGE(TAG, "coap_new_pdu() failed");
    }
    /* Add in an unique token */
    coap_session_new_token(session, &tokenlength, token);
    coap_add_token(put_request, tokenlength, token);

    //add content-type
    u_char buf[4];
    coap_insert_optlist(&optlist,
                        coap_new_optlist(COAP_OPTION_CONTENT_FORMAT,coap_encode_var_safe (buf, sizeof (buf),
                                                                    COAP_MEDIATYPE_TEXT_PLAIN),
                                                                    buf));
    /* Prepare the request */
    coap_split_uri((const uint8_t *)server_uri, strlen(server_uri), &uri);
    coap_add_option(put_request, COAP_OPTION_URI_PATH, uri.path.length, uri.path.s);

    coap_add_data(put_request, strlen(cmd), (unsigned char *)cmd);
    /**/

    coap_add_optlist_pdu(put_request, &optlist);

    resp_wait = 1;
    coap_send(session, put_request);

    wait_ms = 2000;

    while (resp_wait) {
        int result = coap_io_process(ctx, wait_ms > 1000 ? 1000 : wait_ms);
        if (result >= 0) {
            if (result >= wait_ms) {
                ESP_LOGE(TAG, "No response from server");
                break;
            } else {
                wait_ms -= result;
            }
        }
    }
    if (optlist) {
        coap_delete_optlist(optlist);
        optlist = NULL;
    }
    if (session) {
        coap_session_release(session);
    }
    if (ctx) {
        coap_free_context(ctx);
    }
    coap_cleanup();
}
