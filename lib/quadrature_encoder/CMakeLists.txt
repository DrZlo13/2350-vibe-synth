project("quadrature_encoder")

add_library(${PROJECT_NAME} INTERFACE)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/quadrature_encoder.pio)

set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
)

file(GLOB_RECURSE SOURCES
    "*.c"
)

target_sources(${PROJECT_NAME} INTERFACE
    ${SOURCES}
)

target_link_libraries(${PROJECT_NAME} INTERFACE
    pico_stdlib
    pico_multicore
    hardware_pio
)

target_include_directories(${PROJECT_NAME} INTERFACE ${INCLUDES})




