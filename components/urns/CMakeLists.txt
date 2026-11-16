# microReticulum and its dependencies, compiled in place from deps/ (fetched by tools/fetch_deps.sh,
# git-ignored) or from the directory named in the URNS_DEPS environment variable. No source is edited.
if(DEFINED ENV{URNS_DEPS})
    set(L $ENV{URNS_DEPS})
else()
    get_filename_component(L "${CMAKE_CURRENT_LIST_DIR}/../../deps" ABSOLUTE)
endif()
if(NOT EXISTS "${L}/microReticulum/src/microReticulum.h")
    message(FATAL_ERROR "microReticulum not found under ${L}. Run tools/fetch_deps.sh or set URNS_DEPS.")
endif()
file(GLOB_RECURSE RNS_SRCS ${L}/microReticulum/src/*.cpp ${L}/microReticulum/src/*.c)
file(GLOB CRYPTO_SRCS ${L}/Crypto/*.cpp)
idf_component_register(
    SRCS ${RNS_SRCS} ${CRYPTO_SRCS}
    INCLUDE_DIRS ${L}/microReticulum/src ${L}/Crypto ${L}/microStore/include
                 ${L}/ArduinoJson/src ${L}/MsgPack ${L}/ArxContainer ${L}/ArxTypeTraits ${L}/DebugLog
    REQUIRES lwip nvs_flash esp_timer pthread newlib esp_hw_support)
# No file storage in this build: the app registers microStore's do-nothing filesystem.
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    USTORE_USE_NOOPFS MSGPACK_USE_BOOST=OFF DEBUGLOG_DEFAULT_LOG_LEVEL_ERROR)
# ESP-IDF turns these warnings into errors; the library trips two of them.
target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-missing-field-initializers -Wno-format -Wno-unused-parameter
    -Wno-error=all -Wno-error=reorder -Wno-error=parentheses)
# The library bundles its own TLSF allocator, whose function names collide at link with the
# TLSF inside ESP-IDF's heap component. Renamed here by macro; source unchanged.
set(RNS_TLSF_FUNCS add_pool align_size alloc_overhead block_size block_size_max block_size_min check check_pool
    create create_with_pool destroy free get_pool malloc memalign pool_overhead realloc remove_pool size walk_pool)
foreach(f ${RNS_TLSF_FUNCS})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE tlsf_${f}=rns_tlsf_${f})
endforeach()
