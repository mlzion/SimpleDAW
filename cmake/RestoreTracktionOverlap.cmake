# Restore the pinned graph builder in build trees configured before native summing was reinstated.
set(graph_path "modules/tracktion_engine/playback/graph/tracktion_EditNodeBuilder.cpp")
set(graph_file "${tracktion_SOURCE_DIR}/${graph_path}")
file(STRINGS "${graph_file}" legacy_hook REGEX "^// SimpleDAW audio overlap hook$")
if(legacy_hook)
    file(SHA256 "${graph_file}" graph_sha)
    if(NOT graph_sha STREQUAL "5f667ab8c1fbad42cdd9c9205874d28a4f1e0b05d0ab1d8b257b32346dd34b46")
        message(FATAL_ERROR "The Tracktion graph has local edits alongside the retired SimpleDAW overlap hook. Configure with fresh pinned Tracktion sources.")
    endif()
    find_package(Git REQUIRED)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${tracktion_SOURCE_DIR}" show "0a5f4e6a5f53d09c89b414a44386a12df7fa1ec6:${graph_path}"
        OUTPUT_VARIABLE pinned_graph RESULT_VARIABLE graph_result)
    if(NOT graph_result STREQUAL "0")
        message(FATAL_ERROR "Could not restore the retired SimpleDAW overlap hook. Configure with fresh pinned Tracktion sources.")
    endif()
    file(WRITE "${graph_file}" "${pinned_graph}")
endif()
