add_executable(basic_example
    main.c
)

target_link_libraries(basic_example
    pico_stdlib
    pico2_swd_riscv
)

pico_enable_stdio_usb(basic_example 1)
pico_enable_stdio_uart(basic_example 0)

pico_add_extra_outputs(basic_example)
