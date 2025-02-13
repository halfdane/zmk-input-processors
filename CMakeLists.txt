zephyr_library()


zephyr_library_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_TAP drivers/input/input_processor_tap.c)
zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)

