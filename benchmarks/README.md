# Markdown parser performance benchmark

This optional benchmark measures Remini's Markdown parser independently and
through the document-level block-structure pipeline. It also runs the same full
source documents through MD4C 0.6.0 for an external parser comparison. It is deliberately kept
separate from Catch2 tests: timing results are machine-dependent and must not
make normal CI pass/fail decisions.

## Build

Configure from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DREMINI_BUILD_BENCHMARKS=ON -DBUILD_TESTING=OFF
cmake --build build --target MarkdownParserBenchmark --parallel
```

The target uses the same Qt version selected by the main project and does not
require Catch2. MD4C is fetched at the pinned `v0.6.0` tag and compiled as a
private static benchmark dependency; it is not linked into Remini. This requires
network access on the first configure. Disable the comparison with
`-DREMINI_BENCHMARK_WITH_MD4C=OFF`. A Release build is recommended.

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
| `--profile normal|stress|high-pressure` | normal | Scale each workload by 1x, 8x, or 32x source lines |
| `--format text|csv|json` | text | Select human-readable, CSV, or JSON output |
| `--help` | — | Show usage |

Examples:

```sh
# Quick smoke run
./build/benchmarks/MarkdownParserBenchmark --iterations 3 --warmup 1 --passes 1

# Stress the long-line parser with an 8x corpus
./build/benchmarks/MarkdownParserBenchmark --profile stress --filter long-lines

# High-pressure run: 32x input volume across every workload
./build/benchmarks/MarkdownParserBenchmark --profile high-pressure --iterations 5 --warmup 1 --passes 1

# Focus the high-pressure run on the full document pipeline
./build/benchmarks/MarkdownParserBenchmark --profile high-pressure --filter mixed-document --format json

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

## Pressure profiles

Profiles enlarge each built-in corpus before timing; they do not change parser
semantics or add artificial sleeps/concurrency:

- `normal` (1x): baseline workloads.
- `stress` (8x): repeats each workload eight times to exercise larger documents
  and sustained parser/scanner work.
- `high-pressure` (32x): repeats each workload 32 times to expose scaling,
  allocation, and large-document throughput costs.

The profile is included in JSON and text output. High-pressure runs can take
substantially longer and consume more memory, especially for `long-lines`.
Use smaller `--iterations` and `--passes` values for an initial run. These are
single-process throughput stress tests, not concurrent-user/load tests and not
hard pass/fail performance tests.

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
4. `md4c-full-document`: calls MD4C's `md_parse()` once per complete document,
   with callbacks that count parser events but do not build a DOM or render HTML.
   UTF-8 conversion is prepared before timing, so the measured stage is parsing.
   The enabled extensions are tables, strikethrough, task lists, and permissive
   autolinks—the extensions exercised by this benchmark corpus.

Reported metrics include median and p95 latency, min/max, mean and standard
deviation, source lines per second, UTF-8 MiB per second, and nanoseconds per
UTF-16 code unit. The character count follows Qt's `QString::size()`
semantics; it is not a Unicode grapheme count.

## Comparing results

### Interpreting the MD4C comparison

MD4C is a full-document, event/callback parser, while Remini currently exposes
a per-block parser plus a separate block-structure scanner. Therefore
`md4c-full-document` is not a drop-in measurement of Remini's renderer or its
editing pipeline. Compare it primarily with Remini's `document-pipeline` as a
parser-throughput reference, and use `parse-only` to isolate Remini's inline
parser. Callback event counting is intentionally lightweight; no HTML generation,
DOM allocation, Qt rendering, or UTF-8 conversion is included in MD4C timing.
The two parsers also have different syntax coverage and output semantics, so
these numbers are performance context, not proof of feature equivalence.

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
