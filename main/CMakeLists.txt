set(srcs
    main.c
    led_strip_encoder.c
    )

set(requires
    esp-sr
    hardware_driver
    servoim
    )

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS include
                       REQUIRES ${requires})

component_compile_options(-w)