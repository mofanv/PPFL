# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.
add_custom_command(
  OUTPUT secure_aggregation_u.h secure_aggregation_u.c secure_aggregation_args.h
  DEPENDS ${CMAKE_SOURCE_DIR}/secure_aggregation.edl
  COMMAND
    openenclave::oeedger8r --untrusted ${CMAKE_SOURCE_DIR}/secure_aggregation.edl
    --search-path ${OE_INCLUDEDIR} --search-path
    ${OE_INCLUDEDIR}/openenclave/edl/sgx)

add_executable(secure_aggregation_host host.c
                               ${CMAKE_CURRENT_BINARY_DIR}/secure_aggregation_u.c)

if (WIN32)
  copy_oedebugrt_target(secure_aggregation_host_oedebugrt)
  add_dependencies(secure_aggregation_host secure_aggregation_host_oedebugrt)
endif ()

target_include_directories(
  secure_aggregation_host PRIVATE # Needed for the generated file secure_aggregation_u.h
                          ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(secure_aggregation_host openenclave::oehost)
