set(src_files
        main.c
        optical_sensor.c
        network_config.c
        tcp_client.c
        tcp_bsp.c
        config.c
        touch_pad.c
        ble_gatt_server.c)

set(COMPONENT_SRCS ${src_files})
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
