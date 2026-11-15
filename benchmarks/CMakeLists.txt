set(MARKDOWN_DIR ${CMAKE_SOURCE_DIR}/src/markdown)

add_executable(MarkdownParserBenchmark
    markdown_parser_benchmark.cpp
    ${MARKDOWN_DIR}/markdownparser.cpp
    ${MARKDOWN_DIR}/formatdata.cpp
    ${MARKDOWN_DIR}/blockstructure.cpp
    ${MARKDOWN_DIR}/blockdata.cpp
    ${MARKDOWN_DIR}/linedata.cpp
)

target_include_directories(MarkdownParserBenchmark PRIVATE
    ${MARKDOWN_DIR}
)

target_link_libraries(MarkdownParserBenchmark PRIVATE
    Qt${QT_VERSION_MAJOR}::Gui
)

target_compile_features(MarkdownParserBenchmark PRIVATE cxx_std_17)

target_compile_definitions(MarkdownParserBenchmark PRIVATE
    REMINI_BENCHMARK_BUILD_CONFIG="$<CONFIG>"
)


# MD4C is used only by this opt-in benchmark executable. Pin the upstream
# release so results are reproducible and normal Remini builds stay offline.
option(REMINI_BENCHMARK_WITH_MD4C "Compare benchmark workloads with MD4C" ON)
if(REMINI_BENCHMARK_WITH_MD4C)
    enable_language(C)
    include(FetchContent)

    FetchContent_Declare(md4c
        GIT_REPOSITORY https://github.com/mity/md4c.git
        GIT_TAG v0.6.0
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(md4c)
    if(NOT md4c_POPULATED)
        FetchContent_Populate(md4c)
    endif()

    add_library(md4c_benchmark STATIC
        ${md4c_SOURCE_DIR}/src/md4c.c
    )
    target_include_directories(md4c_benchmark PUBLIC
        ${md4c_SOURCE_DIR}/src
    )
    set_target_properties(md4c_benchmark PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
    )

    target_link_libraries(MarkdownParserBenchmark PRIVATE md4c_benchmark)
    target_compile_definitions(MarkdownParserBenchmark PRIVATE
        REMINI_BENCHMARK_HAS_MD4C=1
    )
endif()
