set(srcs
    main.c
    )

set(requires
    esp-sr
    hardware_driver
    servoim
    ledim
    )

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS include
                       REQUIRES ${requires})

component_compile_options(-w)