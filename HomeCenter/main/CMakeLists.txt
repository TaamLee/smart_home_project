# Embed CA, certificate & key directly into binary
idf_component_register(SRCS  "coap.c" "mqtt.c" "app_main.c" "wifi.c" "http.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES certs/coap_ca.pem certs/coap_client.crt certs/coap_client.key)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")