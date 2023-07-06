idf_component_register(SRCS "ds18b20.c" "app_main.c" "mqtt.c" "wifi_conection.c"
                    INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
