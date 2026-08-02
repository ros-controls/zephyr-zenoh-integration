#!/usr/bin/env bash
set -euo pipefail

ZENOH_PICO_CONFIG="${1:-/zephyr_ws/modules/lib/zenoh-pico/include/zenoh-pico/config.h}"

awk ' \
  /^#define[ \t]+[A-Za-z_][A-Za-z0-9_]*/ { \
    name = $2; \
    if (prev == "#ifndef " name) { print; prev = $0; next } \
    print "#ifndef " name; \
    print; \
    print "#endif"; \
    prev = $0; \
    next; \
  } \
  { print; prev = $0 } \
' "${ZENOH_PICO_CONFIG}" > "${ZENOH_PICO_CONFIG}.wrapped" \
  && mv "${ZENOH_PICO_CONFIG}.wrapped" "${ZENOH_PICO_CONFIG}"
  
