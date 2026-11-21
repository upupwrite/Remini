#include "blockstructure.h"
#include "formatdata.h"
#include "markdownparser.h"

#include <QGuiApplication>
#include <QElapsedTimer>
#include <QByteArray>
#ifdef REMINI_BENCHMARK_HAS_MD4C
#include <md4c.h>
#endif
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextStream>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
struct Line { QString text; bool table = false; bool delimiter = false; };

struct Workload
{
    QString name;
    QVector<Line> lines;
    QString source;
    QByteArray sourceUtf8;
    qint64 characters = 0;
    qint64 bytes = 0;

    void finalize()
    {
        QStringList rows;
        rows.reserve(lines.size());
        for (const Line &line : lines)
            rows.append(line.text);
        source = rows.join(QLatin1Char('\n'));
        sourceUtf8 = source.toUtf8();
        characters = source.size();
        bytes = sourceUtf8.size();
    }
};

struct Options
{
    int samples = 15;
    int warmup = 3;
    int passes = 5;
    QString filter;
    QString format = QStringLiteral("text");
    QString profile = QStringLiteral("normal");
    int workloadScale = 1;
};

struct Result
{
    QString workload, stage;
    int lines = 0;
    qint64 characters = 0, bytes = 0;
    double minMs = 0, medianMs = 0, p95Ms = 0, maxMs = 0;
    double meanMs = 0, stddevMs = 0, linesPerSecond = 0;
    double mibPerSecond = 0, nsPerCharacter = 0;
};

volatile quint64 resultSink = 0;

void consume(const FormatData *data, quint64 &sum)
{
    if (!data) return;
    sum += static_cast<quint64>(data->getFormatCounts());
    sum += static_cast<quint64>(data->getMask().count(true));
    sum += static_cast<quint64>(data->totalCollapsedChars());
}

// ---------------------------------------------------------------------------
// Benchmark hygiene helpers. These standardize how measurements are taken while
// leaving the reported columns untouched: they defeat dead-code elimination,
// evict the data cache between samples, and stop the optimizer from hoisting
// work across the clock. None of them add or rename any output field.
// ---------------------------------------------------------------------------

// Portable compiler barrier. Keeps the measured region and the timer on the
// order written, so the optimizer cannot sink warm-up work into a timed window
// or hoist measured work out of it. asm is used where available, otherwise the
// equivalent, zero-runtime-cost signal fence from <atomic>.
void compilerBarrier()
{
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" ::: "memory");
#else
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

// Evict the data cache before each timed sample so every sample starts from a
// comparable cache state, mirroring the explicit cache-flush step used by mature
// benchmarking harnesses (Google Benchmark, nanobench). The accumulator is
// volatile so the touching loop is never elided as dead code. The buffer is sized
// to exceed L2 and typical small L3s; raise it on machines with a very large LLC.
void flushCaches()
{
    static const std::vector<unsigned char> scratch(8u * 1024u * 1024u, 0xA5u);
    volatile unsigned long long acc = 0;
    for (unsigned char cell : scratch)
        acc += cell;
    resultSink = resultSink + static_cast<quint64>(acc);
}

// Measure the floor cost of an empty timed region once per stage so that the
// clock's own overhead can be subtracted from every sample. Returns the median
// overhead in milliseconds (nearest-rank), staying stable against outliers.
double calibrateTimer(int samples)
{
    QVector<double> overhead;
    overhead.reserve(qMax(1, samples));
    const int n = qMax(1, samples);
    for (int i = 0; i < n; ++i)
    {
        flushCaches();
        compilerBarrier();
        QElapsedTimer timer; timer.start();
        compilerBarrier();
        overhead.append(static_cast<double>(timer.nsecsElapsed()) / 1.0e6);
    }
    std::sort(overhead.begin(), overhead.end());
    return overhead.at(overhead.size() / 2);
}

QVector<Workload> makeWorkloads()
{
    QVector<Workload> all;
    const QString tick(1, QLatin1Char(96));
    const QString fence(3, QLatin1Char(96));

    Workload plain;
    plain.name = "plain-prose";
    for (int i = 0; i < 512; ++i)
        plain.lines.append({QStringLiteral("Paragraph %1: ordinary text, punctuation, numbers 12345, and enough words to scan.").arg(i)});
    plain.finalize(); all.append(plain);

    Workload inlineText;
    inlineText.name = "inline-formatting";
    for (int i = 0; i < 512; ++i)
        inlineText.lines.append({QStringLiteral("Line %1 with **bold**, *italic*, ~~strike~~, ").arg(i) +
            tick + "code" + tick + QStringLiteral(", __strong__, and escaped \\*markers\\*.")});
    inlineText.finalize(); all.append(inlineText);

    Workload escapeHeavy;
    escapeHeavy.name = "escape-heavy";
    const QChar backslash = QLatin1Char('\\');
    for (int i = 0; i < 512; ++i)
    {
        QString line = QStringLiteral("Escape parity workload %1: ").arg(i);
        for (int run = 0; run < 24; ++run)
        {
            line += QString(1, backslash) + QStringLiteral("*escaped* ");
            line += QString(2, backslash) + QStringLiteral("**bold** ");
            line += QString(3, backslash) + QStringLiteral("_literal_ ");
        }
        escapeHeavy.lines.append({line});
    }
    escapeHeavy.finalize(); all.append(escapeHeavy);

    Workload headings;
    headings.name = "headings-and-lists";
    for (int i = 0; i < 512; ++i)
    {
        switch (i % 6)
        {
        case 0: headings.lines.append({QStringLiteral("# Heading one %1").arg(i)}); break;
        case 1: headings.lines.append({QStringLiteral("## Heading two %1 ##").arg(i)}); break;
        case 2: headings.lines.append({QStringLiteral("###### Heading six %1").arg(i)}); break;
        case 3: headings.lines.append({QStringLiteral("- [ ] unchecked task %1").arg(i)}); break;
        case 4: headings.lines.append({QStringLiteral("- [x] completed task %1").arg(i)}); break;
        default: headings.lines.append({QStringLiteral("> > nested quote with **emphasis** %1").arg(i)}); break;
        }
    }
    headings.finalize(); all.append(headings);

    Workload linksImages;
    linksImages.name = "links-and-images";
    for (int i = 0; i < 512; ++i)
    {
        switch (i % 4)
        {
        case 0: linksImages.lines.append({QStringLiteral("[label %1](https://example.com/path/%1 \"title\")").arg(i)}); break;
        case 1: linksImages.lines.append({QStringLiteral("<https://example.com/%1> and <mailto:user@example.com>").arg(i)}); break;
        case 2: linksImages.lines.append({QStringLiteral("![image %1](images/photo-%1.png \"caption\")").arg(i)}); break;
        default: linksImages.lines.append({QStringLiteral("![diagram](<assets/a diagram %1.svg>) beside [docs](https://docs.example.com)").arg(i)}); break;
        }
    }
    linksImages.finalize(); all.append(linksImages);

    Workload tables;
    tables.name = "gfm-tables";
    for (int t = 0; t < 64; ++t)
    {
        tables.lines.append({QStringLiteral("| Name | Count | Description |"), true, false});
        tables.lines.append({QStringLiteral("| :--- | ---: | :---: |"), true, true});
        for (int row = 0; row < 6; ++row)
            tables.lines.append({QStringLiteral("| item-%1-%2 | %3 | **cell** with code |").arg(t).arg(row).arg(row * 17), true, false});
        tables.lines.append({QString()});
    }
    tables.finalize(); all.append(tables);

    Workload fences;
    fences.name = "fenced-code-blocks";
    for (int b = 0; b < 64; ++b)
    {
        fences.lines.append({QStringLiteral("Text before code block %1.").arg(b)});
        fences.lines.append({fence + "cpp"});
        for (int row = 0; row < 6; ++row)
            fences.lines.append({QStringLiteral("const int value_%1_%2 = %3;").arg(b).arg(row).arg(row)});
        fences.lines.append({fence});
        fences.lines.append({QStringLiteral("Text after code block with *emphasis*.")});
    }
    fences.finalize(); all.append(fences);

    Workload longLines;
    longLines.name = "long-lines";
    const QString segment = QStringLiteral("Long input with **bold**, *italic*, ~~strike~~, and [link](https://example.com/path). ");
    for (int i = 0; i < 32; ++i)
        longLines.lines.append({QStringLiteral("Line %1: ").arg(i) + segment.repeated(96)});
    longLines.finalize(); all.append(longLines);

    Workload mixed;
    mixed.name = "mixed-document";
    for (int i = 0; i < 512; ++i)
    {
        switch (i % 10)
        {
        case 0: mixed.lines.append({QStringLiteral("## Section %1").arg(i)}); break;
        case 1: mixed.lines.append({QStringLiteral("Paragraph with **bold**, *italic*, and ~~deleted~~ text %1.").arg(i)}); break;
        case 2: mixed.lines.append({QStringLiteral("- [ ] task %1").arg(i)}); break;
        case 3: mixed.lines.append({QStringLiteral("[reference %1](https://example.com/%1)").arg(i)}); break;
        case 4: mixed.lines.append({QStringLiteral("> quoted text with inline code %1").arg(i)}); break;
        case 5:
            mixed.lines.append({QStringLiteral("| key | value |"), true, false});
            mixed.lines.append({QStringLiteral("| --- | ---: |"), true, true});
            mixed.lines.append({QStringLiteral("| alpha | 1 |"), true, false});
            break;
        case 6:
            mixed.lines.append({fence + "text"});
            mixed.lines.append({QStringLiteral("code fence content is not inline parsed")});
            mixed.lines.append({fence});
            break;
        case 7: mixed.lines.append({QStringLiteral("![photo](assets/photo-%1.png)").arg(i)}); break;
        case 8: mixed.lines.append({QStringLiteral("Plain text paragraph %1 with enough words to scan.").arg(i)}); break;
        default: mixed.lines.append({QStringLiteral("#### Heading four %1").arg(i)}); break;
        }
    }
    mixed.finalize(); all.append(mixed);
    return all;
}

void applyWorkloadScale(QVector<Workload> &workloads, int scale)
{
    if (scale <= 1) return;
    for (Workload &workload : workloads)
    {
        const QVector<Line> original = workload.lines;
        workload.lines.clear();
        workload.lines.reserve(original.size() * scale);
        for (int copy = 0; copy < scale; ++copy)
            for (const Line &line : original)
                workload.lines.append(line);

        workload.characters = 0;
        workload.bytes = 0;
        workload.source.clear();
        workload.sourceUtf8.clear();
        workload.finalize();
    }
}

bool positiveInt(const QString &text, int &value, bool allowZero = false)
{
    bool ok = false;
    const int parsed = text.toInt(&ok);
    if (!ok || parsed < (allowZero ? 0 : 1)) return false;
    value = parsed;
    return true;
}

bool optionsFromArgs(const QStringList &args, Options &o)
{
    for (int i = 1; i < args.size(); ++i)
    {
        const QString key = args.at(i);
        if (key == "--help" || key == "-h") return false;
        if (i + 1 >= args.size()) return false;
        const QString value = args.at(++i);
        if (key == "--iterations") { if (!positiveInt(value, o.samples)) return false; }
        else if (key == "--warmup") { if (!positiveInt(value, o.warmup, true)) return false; }
        else if (key == "--passes") { if (!positiveInt(value, o.passes)) return false; }
        else if (key == "--filter") o.filter = value;
        else if (key == "--profile")
        {
            o.profile = value.toLower();
            if (o.profile == "normal") o.workloadScale = 1;
            else if (o.profile == "stress") o.workloadScale = 8;
            else if (o.profile == "high-pressure") o.workloadScale = 32;
            else return false;
        }
        else if (key == "--format")
        {
            o.format = value.toLower();
            if (o.format != "text" && o.format != "csv" && o.format != "json") return false;
        }
        else return false;
    }
    return true;
}


#ifdef REMINI_BENCHMARK_HAS_MD4C
int md4cEnterBlock(MD_BLOCKTYPE, void *, void *userdata)
{
    ++*static_cast<quint64 *>(userdata);
    return 0;
}

int md4cLeaveBlock(MD_BLOCKTYPE, void *, void *userdata)
{
    ++*static_cast<quint64 *>(userdata);
    return 0;
}

int md4cEnterSpan(MD_SPANTYPE, void *, void *userdata)
{
    ++*static_cast<quint64 *>(userdata);
    return 0;
}

int md4cLeaveSpan(MD_SPANTYPE, void *, void *userdata)
{
    ++*static_cast<quint64 *>(userdata);
    return 0;
}

int md4cText(MD_TEXTTYPE, const MD_CHAR *, MD_SIZE, void *userdata)
{
    ++*static_cast<quint64 *>(userdata);
    return 0;
}

quint64 md4cFullDocument(const Workload &workload, int passes)
{
    if (static_cast<quint64>(workload.sourceUtf8.size()) >
        static_cast<quint64>(std::numeric_limits<MD_SIZE>::max()))
        return 0;

    MD_PARSER parser{};
    // Match the extensions exercised by the shared corpus. This deliberately
    // avoids enabling MD4C-only extensions that Remini does not benchmark.
    parser.flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH |
                   MD_FLAG_TASKLISTS | MD_FLAG_PERMISSIVEAUTOLINKS;
    parser.enter_block = md4cEnterBlock;
    parser.leave_block = md4cLeaveBlock;
    parser.enter_span = md4cEnterSpan;
    parser.leave_span = md4cLeaveSpan;
    parser.text = md4cText;

    quint64 events = 0;
    for (int pass = 0; pass < passes; ++pass)
    {
        const int status = md_parse(
            workload.sourceUtf8.constData(),
            static_cast<MD_SIZE>(workload.sourceUtf8.size()),
            &parser, &events);
        if (status != 0)
            return 0;
    }
    return events;
}
#endif

quint64 parseOnly(const Workload &w, int passes)
{
    MarkdownParser parser;
    quint64 checksum = 0;
    for (int p = 0; p < passes; ++p)
        for (const Line &line : w.lines)
        {
            FormatData data;
            consume(parser.parseBlock(line.text, &data, line.table, line.delimiter), checksum);
        }
    return checksum;
}

quint64 scanOnly(QTextDocument &document, int passes)
{
    quint64 checksum = 0;
    for (int p = 0; p < passes; ++p)
    {
        const auto structures = BlockStructureScanner(document).scan();
        checksum += static_cast<quint64>(structures.size());
        for (const auto &s : structures)
            checksum += static_cast<quint64>(s.lastBlock - s.firstBlock + 1);
    }
    return checksum;
}

quint64 documentPipeline(QTextDocument &document, int passes)
{
    quint64 checksum = 0;
    MarkdownParser parser;
    for (int p = 0; p < passes; ++p)
    {
        QVector<int> roles(document.blockCount(), -1); // -2=fence, 1=table, 2=delimiter
        const auto structures = BlockStructureScanner(document).scan();
        for (const auto &s : structures)
            for (int n = s.firstBlock; n <= s.lastBlock; ++n)
                roles[n] = s.kind == BlockStructure::Kind::CodeFence
                    ? -2 : (s.roleAt(n) == BlockStructure::Role::Delimiter ? 2 : 1);

        for (QTextBlock block = document.begin(); block.isValid(); block = block.next())
        {
            const int role = roles.at(block.blockNumber());
            if (role == -2) continue;
            FormatData data;
            consume(parser.parseBlock(block.text(), &data, role == 1 || role == 2,
                                      role == 2), checksum);
        }
    }
    return checksum;
}

Result runBenchmark(const Workload &w, const QString &stage,
                    const Options &o, QTextDocument &document)
{
    Result r;
    r.workload = w.name; r.stage = stage; r.lines = w.lines.size();
    r.characters = w.characters; r.bytes = w.bytes;
    auto run = [&]() -> quint64 {
        if (stage == "parse-only") return parseOnly(w, o.passes);
#ifdef REMINI_BENCHMARK_HAS_MD4C
        if (stage == "md4c-full-document") return md4cFullDocument(w, o.passes);
#endif
        if (stage == "structure-scan") return scanOnly(document, o.passes);
        return documentPipeline(document, o.passes);
    };
    // Standard warm-up: iterate the unit of work until branch predictors, caches
    // and any lazy initialization reach steady state, then discard these results.
    for (int i = 0; i < o.warmup; ++i)
        resultSink = resultSink + run();

    // Remove the timer's own overhead so short samples are not biased upward.
    const double timerOverheadMs = calibrateTimer(o.samples);

    QVector<double> times;
    times.reserve(o.samples);
    for (int i = 0; i < o.samples; ++i)
    {
        flushCaches();        // cold-cache start, like a reference harness
        compilerBarrier();    // keep the flush outside the timed region
        QElapsedTimer timer; timer.start();
        const quint64 checksum = run();
        compilerBarrier();    // keep the work inside the timed region
        double elapsedMs = static_cast<double>(timer.nsecsElapsed()) / 1.0e6;
        elapsedMs -= timerOverheadMs;            // subtract the measured floor
        if (elapsedMs < 0.0) elapsedMs = 0.0;    // never report a negative span
        times.append(elapsedMs);
        resultSink = resultSink + checksum;      // single anti-DCE write-back
    }
    std::sort(times.begin(), times.end());
    r.minMs = times.first(); r.maxMs = times.last();
    r.medianMs = times.size() % 2 == 0
        ? (times.at(times.size() / 2 - 1) + times.at(times.size() / 2)) / 2.0
        : times.at(times.size() / 2);
    r.p95Ms = times.at(qMax(0, qMin(times.size() - 1,
        static_cast<int>(std::ceil(times.size() * 0.95)) - 1)));
    for (double t : times) r.meanMs += t;
    r.meanMs /= times.size();
    for (double t : times) r.stddevMs += (t - r.meanMs) * (t - r.meanMs);
    r.stddevMs = std::sqrt(r.stddevMs / times.size());

    const double seconds = r.medianMs / 1000.0;
    if (seconds > 0.0)
    {
        r.linesPerSecond = (static_cast<double>(r.lines) * o.passes) / seconds;
        r.mibPerSecond = (static_cast<double>(r.bytes) * o.passes) / seconds / (1024.0 * 1024.0);
        const double chars = static_cast<double>(r.characters) * o.passes;
        r.nsPerCharacter = chars > 0.0 ? r.medianMs * 1.0e6 / chars : 0.0;
    }
    return r;
}

void printHelp()
{
    QTextStream out(stdout);
    out << "Remini Markdown parser benchmark\n"
        << "Usage: MarkdownParserBenchmark [--profile normal|stress|high-pressure] "
           "[--iterations N] [--warmup N] [--passes N] "
           "[--filter TEXT] [--format text|csv|json]\n"
        << "Defaults: iterations=15, warmup=3, passes=5\n"
        << "Profiles scale each workload: normal=1x, stress=8x, high-pressure=32x\n"
        << "Stages: parse-only, structure-scan, document-pipeline\n";
}

QJsonObject jsonResult(const Result &r)
{
    QJsonObject o;
    o.insert("workload", r.workload); o.insert("stage", r.stage);
    o.insert("lines", r.lines); o.insert("characters", static_cast<double>(r.characters));
    o.insert("utf8_bytes", static_cast<double>(r.bytes));
    o.insert("median_ms", r.medianMs); o.insert("p95_ms", r.p95Ms);
    o.insert("min_ms", r.minMs); o.insert("max_ms", r.maxMs);
    o.insert("mean_ms", r.meanMs); o.insert("stddev_ms", r.stddevMs);
    o.insert("lines_per_second", r.linesPerSecond);
    o.insert("mib_per_second", r.mibPerSecond);
    o.insert("nanoseconds_per_character", r.nsPerCharacter);
    return o;
}

void output(const QVector<Result> &results, const Options &o)
{
    QTextStream out(stdout);
    if (o.format == "json")
    {
        QJsonObject root;
        root.insert("benchmark", "Remini Markdown parser and MD4C comparison");
#ifdef REMINI_BENCHMARK_HAS_MD4C
        root.insert("md4c_version", "0.6.0");
        root.insert("md4c_dialect", "tables,strikethrough,tasklists,permissive-autolinks");
#endif
        root.insert("qt_version", QT_VERSION_STR);
#ifdef REMINI_BENCHMARK_BUILD_CONFIG
        root.insert("build_config", REMINI_BENCHMARK_BUILD_CONFIG);
#endif
        root.insert("profile", o.profile);
        root.insert("workload_scale", o.workloadScale);
        root.insert("iterations", o.samples); root.insert("warmup", o.warmup);
        root.insert("passes_per_sample", o.passes);
#if defined(__clang__)
        root.insert("compiler", QStringLiteral("Clang ") + QStringLiteral(__clang_version__));
#elif defined(__GNUC__)
        root.insert("compiler", QStringLiteral("GCC ") + QStringLiteral(__VERSION__));
#elif defined(_MSC_VER)
        root.insert("compiler", "MSVC");
#else
        root.insert("compiler", "unknown");
#endif
        QJsonArray rows;
        for (const Result &r : results) rows.append(jsonResult(r));
        root.insert("results", rows);
        out << QJsonDocument(root).toJson(QJsonDocument::Indented);
        return;
    }
    if (o.format == "csv")
    {
        out << "workload,stage,lines,characters,utf8_bytes,median_ms,p95_ms,min_ms,max_ms,mean_ms,stddev_ms,lines_per_second,mib_per_second,nanoseconds_per_character\n";
        for (const Result &r : results)
            out << '"' << r.workload << "\",\"" << r.stage << "\"," << r.lines
                << ',' << r.characters << ',' << r.bytes << ',' << r.medianMs
                << ',' << r.p95Ms << ',' << r.minMs << ',' << r.maxMs << ','
                << r.meanMs << ',' << r.stddevMs << ',' << r.linesPerSecond
                << ',' << r.mibPerSecond << ',' << r.nsPerCharacter << '\n';
        return;
    }

    out << "Remini Markdown parser comparison"
#ifdef REMINI_BENCHMARK_HAS_MD4C
        << " | MD4C 0.6.0"
#endif
        << " | Qt " << QT_VERSION_STR
        << " | profile=" << o.profile << " (" << o.workloadScale << "x)"
        << " | samples=" << o.samples << " warmup=" << o.warmup
        << " passes/sample=" << o.passes << "\n\n";
    out << "WORKLOAD                 STAGE                 MEDIAN ms   P95 ms       lines/s       MiB/s      ns/char\n";
    for (const Result &r : results)
        out << QStringLiteral("%1 %2 %3 %4 %5 %6 %7\n")
            .arg(r.workload, -24).arg(r.stage, -20)
            .arg(r.medianMs, 10, 'f', 3).arg(r.p95Ms, 9, 'f', 3)
            .arg(r.linesPerSecond, 13, 'f', 0).arg(r.mibPerSecond, 10, 'f', 2)
            .arg(r.nsPerCharacter, 11, 'f', 2);
    out << "\nCompare on the same machine, Qt version, compiler, and build configuration.\n";
}
} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    Options options;
    if (!optionsFromArgs(app.arguments(), options))
    {
        printHelp();
        return app.arguments().contains("--help") || app.arguments().contains("-h") ? 0 : 2;
    }

    QVector<Workload> workloads = makeWorkloads();
    applyWorkloadScale(workloads, options.workloadScale);

    QVector<Result> results;
    for (const Workload &w : workloads)
    {
        if (!options.filter.isEmpty() && !w.name.contains(options.filter, Qt::CaseInsensitive))
            continue;
        QTextDocument document;
        document.setPlainText(w.source);
        if (document.blockCount() != w.lines.size())
        {
            QTextStream(stderr) << "Unexpected QTextDocument block count for " << w.name << '\n';
            return 1;
        }
        results.append(runBenchmark(w, "parse-only", options, document));
        results.append(runBenchmark(w, "structure-scan", options, document));
        results.append(runBenchmark(w, "document-pipeline", options, document));
#ifdef REMINI_BENCHMARK_HAS_MD4C
        results.append(runBenchmark(w, "md4c-full-document", options, document));
#endif
    }
    if (results.isEmpty())
    {
        QTextStream(stderr) << "No workloads matched --filter.\n";
        return 2;
    }
    output(results, options);
    return 0;
}
