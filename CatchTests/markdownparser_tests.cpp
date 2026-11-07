#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_all.hpp>

#include <QTextBlock>
#include <QTextDocument>

#include "formatdata.h"
#include "markdownparser.h"

namespace
{
bool hasFormat(const FormatData &data, FragmentData::FormatSymbol expected)
{
    for (auto it = const_cast<FormatData &>(data).formats_begin();
         it != const_cast<FormatData &>(data).formats_end(); ++it)
    {
        if ((*it)->getStatus() == expected)
            return true;
    }
    return false;
}
}  // namespace

TEST_CASE("MarkdownParser recognizes common inline constructs",
          "[MarkdownParser]")
{
    struct Case
    {
        QString source;
        FragmentData::FormatSymbol expected;
    };

    const Case cases[] = {
        {QStringLiteral("**strong text**"), FragmentData::BOLD},
        {QStringLiteral("*emphasis*"), FragmentData::ITALIC},
        {QStringLiteral("~~removed~~"), FragmentData::STRIKETHROUGH},
        {QStringLiteral("`literal * markers`"), FragmentData::CODE_SPAN},
        {QStringLiteral("# heading"), FragmentData::HEADING1},
        {QStringLiteral("[label](https://example.com)"),
         FragmentData::LINK_TITLE},
    };

    MarkdownParser parser;
    for (const Case &test : cases)
    {
        QTextDocument document;
        document.setPlainText(test.source);
        QTextBlock block = document.begin();
        FormatData parsed;

        parser.identifyFormatData(block, &parsed);

        INFO(test.source.toStdString());
        REQUIRE(hasFormat(parsed, test.expected));
        REQUIRE(parsed.getMask().size() == test.source.size());
        REQUIRE(block.text() == test.source);
    }
}

TEST_CASE("MarkdownParser leaves escaped and code-span markers literal",
          "[MarkdownParser]")
{
    const QString source = QStringLiteral(R"(\*literal\* and `**code**`)");
    QTextDocument document;
    document.setPlainText(source);
    QTextBlock block = document.begin();
    FormatData parsed;
    MarkdownParser parser;

    parser.identifyFormatData(block, &parsed);

    REQUIRE(parsed.getMask().size() == source.size());
    REQUIRE(block.text() == source);
    REQUIRE_FALSE(hasFormat(parsed, FragmentData::BOLD));
    REQUIRE(hasFormat(parsed, FragmentData::CODE_SPAN));
}

// Hidden by default because wall-clock measurements vary by machine and build
// configuration. Run explicitly with: CatchTests "[.performance]".
TEST_CASE("MarkdownParser throughput on a mixed Markdown workload",
          "[.performance][MarkdownParser]")
{
    QStringList lines;
    lines.reserve(256);
    for (int i = 0; i < 256; ++i)
    {
        switch (i % 6)
        {
        case 0:
            lines.append(QStringLiteral("## Heading %1 with **bold** text")
                             .arg(i));
            break;
        case 1:
            lines.append(QStringLiteral(
                             "A paragraph with *emphasis*, ~~strike~~, and `code %1`.")
                             .arg(i));
            break;
        case 2:
            lines.append(QStringLiteral(
                             "[link %1](https://example.com/path) and "
                             "<https://example.com/%1>")
                             .arg(i));
            break;
        case 3:
            lines.append(QStringLiteral("- [ ] task %1 with \\*escaped\\* text")
                             .arg(i));
            break;
        case 4:
            lines.append(QStringLiteral(
                             "A longer plain-text line for scanning: %1 abcdefghijklmnopqrstuvwxyz "
                             "0123456789.")
                             .arg(i));
            break;
        default:
            lines.append(QStringLiteral("> > quoted **content** %1").arg(i));
            break;
        }
    }

    QTextDocument document;
    document.setPlainText(lines.join(QLatin1Char('\n')));
    MarkdownParser parser;
    REQUIRE(document.blockCount() == lines.size());

    BENCHMARK("parse 256 mixed Markdown blocks")
    {
        int parsedBlocks = 0;
        for (QTextBlock block = document.begin(); block.isValid();
             block = block.next())
        {
            // Use a fresh result for each block and iteration so the measured
            // work is parsing, not accumulation into an old FormatData object.
            FormatData parsed;
            parser.identifyFormatData(block, &parsed);
            ++parsedBlocks;
        }
        return parsedBlocks;
    };
}
