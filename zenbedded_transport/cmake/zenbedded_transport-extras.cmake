# Copyright 2026 Open Source Robotics Foundation, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

find_package(yaml-cpp QUIET)
if(NOT yaml-cpp_FOUND)
  find_package(yaml_cpp_vendor QUIET)
  find_package(yaml-cpp QUIET)
endif()

function(zenbedded_generate_header)
  cmake_parse_arguments(ARG "" "OUTPUT;YAML;TARGET" "" ${ARGN})

  if(NOT ARG_YAML OR NOT ARG_OUTPUT)
    message(FATAL_ERROR "zenbedded_generate_header requires YAML and OUTPUT arguments")
  endif()

  if(NOT IS_ABSOLUTE "${ARG_YAML}")
    set(ARG_YAML "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_YAML}")
  endif()
  if(NOT IS_ABSOLUTE "${ARG_OUTPUT}")
    set(ARG_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${ARG_OUTPUT}")
  endif()

  get_filename_component(_gen_name "${ARG_OUTPUT}" NAME_WE)
  set(_target "zenbedded_generate_${_gen_name}")

  # Find the Python script installed by zenbedded_transport
  get_filename_component(_prefix "${zenbedded_transport_DIR}/../../.." ABSOLUTE)
  set(_gen_script "${_prefix}/lib/zenbedded_transport/generate_tier2_payload.py")

  add_custom_command(
    OUTPUT "${ARG_OUTPUT}"
    COMMAND python3 "${_gen_script}" --yaml "${ARG_YAML}" --output "${ARG_OUTPUT}"
    DEPENDS "${ARG_YAML}"
    COMMENT "Generating interface header: ${ARG_OUTPUT}"
  )

  add_custom_target("${_target}" DEPENDS "${ARG_OUTPUT}")

  if(ARG_TARGET)
    set("${ARG_TARGET}" "${_target}" PARENT_SCOPE)
  endif()
endfunction()
