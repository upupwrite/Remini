#include <catch2/catch_all.hpp>

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
} // namespace

TEST_CASE("MarkdownParser recognizes common inline constructs", "[MarkdownParser]")
{
    const QString tick(1, QLatin1Char(96));
    struct Case { QString source; FragmentData::FormatSymbol expected; };
    const Case cases[] = {
        {QStringLiteral("**strong text**"), FragmentData::BOLD},
        {QStringLiteral("*emphasis*"), FragmentData::ITALIC},
        {QStringLiteral("~~removed~~"), FragmentData::STRIKETHROUGH},
        {tick + QStringLiteral("literal * markers") + tick, FragmentData::CODE_SPAN},
        {QStringLiteral("# heading"), FragmentData::HEADING1},
        {QStringLiteral("[label](https://example.com)"), FragmentData::LINK_TITLE},
    };

    MarkdownParser parser;
    for (const Case &test : cases)
    {
        FormatData parsed;
        const FormatData *result = parser.parseBlock(test.source, &parsed);
        INFO(test.source.toStdString());
        REQUIRE(result == &parsed);
        REQUIRE(hasFormat(parsed, test.expected));
        REQUIRE(parsed.getMask().size() == test.source.size());
        REQUIRE(!test.source.isEmpty());
    }
}

TEST_CASE("MarkdownParser leaves escaped and code-span markers literal",
          "[MarkdownParser]")
{
    const QString tick(1, QLatin1Char(96));
    const QString source = QStringLiteral("\\*literal\\* and ") +
                           tick + QStringLiteral("**code**") + tick;
    FormatData parsed;
    MarkdownParser parser;
    parser.parseBlock(source, &parsed);

    REQUIRE(parsed.getMask().size() == source.size());
    REQUIRE_FALSE(hasFormat(parsed, FragmentData::BOLD));
    REQUIRE(hasFormat(parsed, FragmentData::CODE_SPAN));
}

TEST_CASE("MarkdownParser handles many protected inline ranges", "[MarkdownParser]")
{
    const QString source =
        QStringLiteral("`*literal*` **bold** \\*escaped\\* ~~removed~~ "
                       "`_also literal_` and *emphasis*");
    FormatData parsed;
    MarkdownParser parser;
    parser.parseBlock(source, &parsed);

    REQUIRE(parsed.getMask().size() == source.size());
    REQUIRE(hasFormat(parsed, FragmentData::BOLD));
    REQUIRE(hasFormat(parsed, FragmentData::STRIKETHROUGH));
    REQUIRE(hasFormat(parsed, FragmentData::ITALIC));
    REQUIRE(hasFormat(parsed, FragmentData::CODE_SPAN));
}

TEST_CASE("MarkdownParser handles odd and even backslash runs", "[MarkdownParser]")
{
    MarkdownParser parser;

    // One backslash escapes the opening delimiter; two do not.
    const QString escaped = QStringLiteral("\\") + QStringLiteral("**literal**");
    FormatData escapedData;
    parser.parseBlock(escaped, &escapedData);
    REQUIRE_FALSE(hasFormat(escapedData, FragmentData::BOLD));

    const QString unescaped = QStringLiteral("\\\\") + QStringLiteral("**bold**");
    FormatData unescapedData;
    parser.parseBlock(unescaped, &unescapedData);
    REQUIRE(hasFormat(unescapedData, FragmentData::BOLD));
}

TEST_CASE("MarkdownParser applies table transformations only when requested",
          "[MarkdownParser]")
{
    MarkdownParser parser;
    const QString row = QStringLiteral("| left | right |");
    FormatData parsedRow;
    parser.parseBlock(row, &parsedRow, true, false);

    REQUIRE(parsedRow.hasCharacterReplacement(0));
    REQUIRE(parsedRow.characterReplacementAt(0) == QLatin1Char('\t'));
    REQUIRE(parsedRow.getMask().size() == row.size());

    const QString delimiter = QStringLiteral("| --- | --- |");
    FormatData parsedDelimiter;
    parser.parseBlock(delimiter, &parsedDelimiter, true, true);

    REQUIRE(parsedDelimiter.getMask().size() == delimiter.size());
    REQUIRE(parsedDelimiter.getMask().count(true) == delimiter.size());
}
