# Copyright (c) 2024 The ZMK Contributors
# SPDX-License-Identifier: MIT

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_ABSOLUTE_TO_RELATIVE input_processor_absolute_to_relative.c)
