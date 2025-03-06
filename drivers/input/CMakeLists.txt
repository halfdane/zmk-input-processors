# Copyright (c) 2024 The ZMK Contributors
# SPDX-License-Identifier: MIT

if (CONFIG_DT_HAS_ZMK_INPUT_PROCESSOR_ABSOLUTE_TO_RELATIVE_ENABLED AND NOT ZMK_POINTING)
    message( FATAL_ERROR "You don't have ZMK_POINTING configured, but zmk_input_processors depends on it." )    
endif()

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_ABSOLUTE_TO_RELATIVE input_processor_absolute_to_relative.c)
