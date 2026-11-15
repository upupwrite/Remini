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
