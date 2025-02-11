set(srcs
    main.c
    speech_commands_action.c
    servoim.c
    led_strip_encoder.c
    )

set(requires
    esp-sr
    hardware_driver
    )

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS include
                       REQUIRES ${requires})

component_compile_options(-w)