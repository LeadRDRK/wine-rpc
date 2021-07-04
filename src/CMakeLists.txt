# detect system processor for lib folder
if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64|ia64")
    set(ARCH_DIR "x86_64")
elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86|i686|i386")
    set(ARCH_DIR "x86")
else()
    message(FATAL_ERROR "Processor architecture not supported by the Discord Game SDK")
endif()

# check if it exists
set(DISCORD_LIB_PATH "${CMAKE_SOURCE_DIR}/discord_game_sdk/lib/${ARCH_DIR}/discord_game_sdk.so")
if(NOT EXISTS "${DISCORD_LIB_PATH}")
    message(FATAL_ERROR "Discord Game SDK library not found!")
endif()
# create symlinks of the library in the build directory
file(CREATE_LINK "${DISCORD_LIB_PATH}" "${CMAKE_BINARY_DIR}/discord_game_sdk.so" SYMBOLIC)
file(CREATE_LINK "${DISCORD_LIB_PATH}" "${CMAKE_BINARY_DIR}/src/discord_game_sdk.so" SYMBOLIC)
add_library(discord_game_sdk SHARED IMPORTED)
set_target_properties(discord_game_sdk PROPERTIES IMPORTED_LOCATION discord_game_sdk.so)

set(WINE_RPC_SOURCES
    app_list.c
    config.c
    main.c
    procs.c
)

set(WINE_RPC_HEADERS
    app_list.h
    config.h
    procs.h
)

add_executable(wine-rpc ${WINE_RPC_SOURCES} ${WINE_RPC_HEADERS})
target_include_directories(wine-rpc PRIVATE "${CMAKE_SOURCE_DIR}/discord_game_sdk/c")
target_link_libraries(wine-rpc discord_game_sdk)
set_target_properties(wine-rpc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    INSTALL_RPATH $ORIGIN
    BUILD_WITH_INSTALL_RPATH TRUE
)
# copy the library to the binary's directory
add_custom_command(TARGET wine-rpc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${DISCORD_LIB_PATH}
            "${CMAKE_BINARY_DIR}/bin"
)
