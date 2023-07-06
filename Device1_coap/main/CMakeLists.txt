idf_component_register(SRCS "ds18b20.c" "wifi_conection.c" "coap_server_example_main.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES certs/coap_ca.pem certs/coap_server.crt certs/coap_server.key)
