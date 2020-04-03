set(src_files
        main.c
        sensor.c
        network_config.c
        tcp_client.c
        tcp_bsp.c
        config.c
        touch_pad.c
        ble_spp_server.c
        protocol_parser.c
        motor_controller.c
        adc_driver.c
        work_bench.c
        network_time_sntp.c)

set(COMPONENT_SRCS ${src_files})
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
