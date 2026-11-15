# Markdown parser performance benchmark

This optional benchmark measures Remini's Markdown parser independently and
through the document-level block-structure pipeline. It is deliberately kept
separate from Catch2 tests: timing results are machine-dependent and must not
make normal CI pass/fail decisions.

## Build

Configure from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DREMINI_BUILD_BENCHMARKS=ON -DBUILD_TESTING=OFF
cmake --build build --target MarkdownParserBenchmark --parallel
```

The target uses the same Qt version selected by the main project and does not
require Catch2. A Release build is recommended for performance comparisons.

## Run

```sh
./build/benchmarks/MarkdownParserBenchmark

# Headless Linux sessions can use the offscreen Qt platform plugin
QT_QPA_PLATFORM=offscreen ./build/benchmarks/MarkdownParserBenchmark --filter plain-prose
```

Options:

| Option | Default | Meaning |
| --- | ---: | --- |
| `--iterations N` | 15 | Measured samples for each workload |
| `--warmup N` | 3 | Warm-up samples excluded from statistics |
| `--passes N` | 5 | Complete workload passes in each sample |
| `--filter TEXT` | — | Run workload names containing TEXT |
| `--format text|csv|json` | text | Select human-readable, CSV, or JSON output |
| `--help` | — | Show usage |

Examples:

```sh
# Quick smoke run
./build/benchmarks/MarkdownParserBenchmark --iterations 3 --warmup 1 --passes 1

# Stress the long-line parser
./build/benchmarks/MarkdownParserBenchmark --filter long-lines

# Save machine-readable output
./build/benchmarks/MarkdownParserBenchmark --format json > benchmark.json
./build/benchmarks/MarkdownParserBenchmark --format csv > benchmark.csv
```

## Workloads

- Plain prose: ordinary paragraphs without Markdown syntax.
- Inline formatting: emphasis, strong text, strikethrough, code spans, and escapes.
- Headings and lists: ATX headings, task checkboxes, and nested blockquotes.
- Links and images: inline links, autolinks, image destinations, and titles.
- GFM tables: header, delimiter, and body rows with pipe replacement.
- Fenced code blocks: code-fence classification and non-Markdown code contents.
- Long lines: repeated inline constructs on long source lines.
- Mixed document: a representative combination of block and inline syntax.

## Measurement stages

Each workload reports three stages:

1. `parse-only`: calls `MarkdownParser::parseBlock` on pre-built source lines.
   It excludes document construction and block-structure scanning.
2. `structure-scan`: measures `BlockStructureScanner::scan` on a pre-built
   `QTextDocument`, isolating fenced-code and table discovery.
3. `document-pipeline`: scans block structures, classifies table/code blocks,
   then parses eligible blocks. This is the closest benchmark stage to the
   parser and block-classification portion of the editor pipeline; it does not
   measure Qt text rendering, image loading, painting, or user interaction.

Reported metrics include median and p95 latency, min/max, mean and standard
deviation, source lines per second, UTF-8 MiB per second, and nanoseconds per
UTF-16 code unit. The character count follows Qt's `QString::size()`
semantics; it is not a Unicode grapheme count.

## Comparing results

For meaningful comparisons:

- Use the same machine, operating system, Qt version, compiler, and build type.
- Prefer Release builds and close unnecessary background workloads.
- Keep the same sample, warm-up, and pass counts between runs.
- Run at least twice and compare medians; inspect p95 and standard deviation
  when results are noisy.
- Do not treat a single machine's absolute timing as a universal performance
  guarantee.

The benchmark intentionally has no hard performance thresholds. CI can compile
and smoke-test the executable, but noisy wall-clock results should not gate
merges.
