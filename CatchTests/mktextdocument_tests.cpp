// Fixed test file for MkTextDocument.
//
// Two behaviours of MkTextDocument that these tests rely on:
//
//   1. setMarkdownHandle(true) hides the markdown symbols in EVERY block
//      of the document, i.e. after that call all blocks are rendered
//      (symbols stripped, checkboxes / links collapsed).
//
//   2. cursorPosChangedHandle(&range) UNHIDES exactly the block whose
//      index equals range.currentBlockNo. If that index does not exist
//      in the document (for example currentBlockNo == 1 when there is
//      only block 0, or currentBlockNo == 2 when there are only blocks
//      0 and 1) the call is a silent no-op and every block stays in the
//      state produced by setMarkdownHandle().
//
// Several tests below deliberately pass an out-of-range currentBlockNo
// (e.g. "= 1" on a single-block document, "= 2" on a two-block document)
// in order to keep the "everything hidden" state after
// setMarkdownHandle(true). Do NOT "fix" those values: they are chosen
// on purpose.

#include <QApplication>
#include <QChar>
#include <QString>
#include <QTextCharFormat>
#include <QTextCursor>
#include <catch2/catch_all.hpp>

#include "mkedit.h"
#include "markdownparser.h"
#include "mktextdocument.h"

TEST_CASE("MkTextDocument simple text", "[MkTextDocument]")
{
    MkTextDocument doc;
    doc.setPlainText("abc");
    QString text = doc.toPlainText();
    REQUIRE("abc" == text);
}

TEST_CASE("MkTextDocument blockquote hides marker and styles block",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("> quoted text");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // out of range keeps block 0 rendered
    doc.cursorPosChangedHandle(&range);

    REQUIRE(doc.begin().text() == "quoted text");
    REQUIRE(doc.begin().blockFormat().leftMargin() == Catch::Approx(18.0));
    REQUIRE(doc.getRawDocument()->toPlainText() == "> quoted text");
}

TEST_CASE("MkTextDocument blockquote supports indentation and empty quotes",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("   > indented quote\n>");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 2;  // keep both blocks rendered
    doc.cursorPosChangedHandle(&range);

    REQUIRE(doc.begin().text() == "indented quote");
    REQUIRE(doc.begin().blockFormat().leftMargin() == Catch::Approx(18.0));
    REQUIRE(doc.begin().next().text().isEmpty());
    REQUIRE(doc.begin().next().blockFormat().leftMargin() == Catch::Approx(18.0));
}

TEST_CASE("MkTextDocument blockquote marker without trailing space",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText(">not a quote");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;
    doc.cursorPosChangedHandle(&range);

    // A '>' marker needs no trailing space (CommonMark rule b).
    REQUIRE(doc.begin().text() == "not a quote");
    REQUIRE(doc.begin().blockFormat().leftMargin() == Catch::Approx(18.0));
}

TEST_CASE("MkTextDocument nested blockquote markers hide and indent",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText(">> nested");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;
    doc.cursorPosChangedHandle(&range);

    REQUIRE(doc.begin().text() == "nested");
    REQUIRE(doc.begin().blockFormat().leftMargin() == Catch::Approx(36.0));
    REQUIRE(doc.getRawDocument()->toPlainText() == ">> nested");
}

TEST_CASE("MkTextDocument spaced nested blockquote markers",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("> > nested");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;
    doc.cursorPosChangedHandle(&range);

    REQUIRE(doc.begin().text() == "nested");
    REQUIRE(doc.begin().blockFormat().leftMargin() == Catch::Approx(36.0));
}

TEST_CASE("MarkdownParser blockquote prefix extraction", "[MarkdownParser]")
{
    REQUIRE(MarkdownParser::blockQuotePrefix("> hello") == "> ");
    REQUIRE(MarkdownParser::blockQuotePrefix(">hello") == ">");
    REQUIRE(MarkdownParser::blockQuotePrefix(">>") == ">>");
    REQUIRE(MarkdownParser::blockQuotePrefix("> > hello") == "> > ");
    REQUIRE(MarkdownParser::blockQuotePrefix("   > hello") == "   > ");
    REQUIRE(MarkdownParser::blockQuotePrefix("    > hello").isEmpty());
    REQUIRE(MarkdownParser::blockQuotePrefix("hello").isEmpty());
}

TEST_CASE("MkTextDocument bold text", "[MkTextDocument]")
{
    MkTextDocument doc;
    doc.setPlainText("**abc**");
    QString text = doc.toPlainText();
    REQUIRE("**abc**" == text);
}

TEST_CASE("MkTextDocument italic text", "[MkTextDocument]")
{
    MkTextDocument doc;
    doc.setPlainText("*abc*");
    QString text = doc.toPlainText();
    REQUIRE("*abc*" == text);
}

TEST_CASE("MkTextDocument single word bold, hide symbols", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;
    doc.setPlainText("**abc**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    // currentBlockNo == 1 is out of range for a single-block document,
    // so this is a deliberate no-op that keeps block 0 hidden.
    range.hasSelection = false;
    range.currentBlockNo = 1;
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc" == text);
}

TEST_CASE("MkTextDocument bold, hide symbols, multiple words",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("**abc** **qwerty**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    // Out of range -> no-op, everything stays hidden.
    range.hasSelection = false;
    range.currentBlockNo = 1;
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc qwerty" == text);
}

TEST_CASE("MkTextDocument bold, hide symbols on multiple lines",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("**abc** **qwerty**\n **new** **line**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    // Out of range for a two-block document -> no-op, both blocks stay hidden.
    range.hasSelection = false;
    range.currentBlockNo = 2;
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc qwerty\n new line" == text);
}

TEST_CASE("MkTextDocument bold, hide symbols only on 1st line",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("**abc** **qwerty**\n **new** **line**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    // Unhides block index 1 (the second line), so block 0 stays rendered.
    range.hasSelection = false;
    range.currentBlockNo = 1;
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc qwerty\n **new** **line**" == text);
}

TEST_CASE("MkTextDocument bold, hide symbols, underscore", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("__abc__");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op on single-block document
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc" == text);
}

TEST_CASE("MkTextDocument bold, hide underscore symbols only on 1st line",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("__abc__\n __123__");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // unhides the second line
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc\n __123__" == text);
}

TEST_CASE("MkTextDocument bold, hide underscore symbols only on both lines",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("__abc__\n __123__");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 2;  // no-op -> both lines stay rendered
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc\n 123" == text);
}

TEST_CASE("MkTextDocument single word italic, hide symbols", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("*abc*");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 2;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc" == text);
}

TEST_CASE("MkTextDocument italic, hide symbol on 1st line", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("*abc*\n*hello*");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // unhide 2nd line
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc\n*hello*" == text);
}

TEST_CASE("MkTextDocument italic, hide symbols on both lines",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("*abc*\n*hello*");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 2;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc\nhello" == text);
}

TEST_CASE("MkTextDocument single word italic, hide symbols, underscore",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("_abc_");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc" == text);
}

TEST_CASE("MkTextDocument italic, hide underscore symbols on 1st line",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("_abc_\n_hello_");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // unhide 2nd line
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc\n_hello_" == text);
}

TEST_CASE("MkTextDocument italic, hide underscore symbols on both lines",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("_abc_\n_hello_");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 2;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc\nhello" == text);
}

TEST_CASE(
    "MkTextDocument single word bold plus false bold sign, hide only bold "
    "symbols",
    "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("**abc** **");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc **" == text);
}

TEST_CASE(
    "MkTextDocument single word bold plus false bold sign, hide only bold "
    "symbols, underscore",
    "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("__abc__ __");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc __" == text);
}

TEST_CASE(
    "MkTextDocument single word italic plus false italic sign, hide only "
    "italic sign",
    "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("*abc* *");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc *" == text);
}

TEST_CASE(
    "MkTextDocument single word italic plus false italic sign, hide only "
    "italic sign, underscore",
    "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("_abc_ _");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc _" == text);
}

TEST_CASE("MkTextDocument link texts", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("[youtube](<www.youtube.com>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("youtube" == text);
}

TEST_CASE("MkTextDocument link texts, hide symbols only in 1st line",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText(
        "[youtube](<www.youtube.com>)\n[google](<www.google.com>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // unhide 2nd line
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("youtube\n[google](<www.google.com>)" == text);
}

TEST_CASE("MkTextDocument link texts, hide symbols only in both lines",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText(
        "[youtube](<www.youtube.com>)\n[google](<www.google.com>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 2;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("youtube\ngoogle" == text);
}

TEST_CASE("MkTextDocument link texts with underscore", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("[lang_explain](<https://sqlite.org/lang_explain.html>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 2;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("lang_explain" == text);
}

TEST_CASE("MkTextDocument link texts with empty title", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("[](<www.google.com>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 2;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("" == text);
}

TEST_CASE("MkTextDocument link texts false positive", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("abc](<123");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc](<123" == text);
}

TEST_CASE("MkTextDocument local link texts, with () round brackets in the path",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText(
        "[microsoft](<file:///C:\\Program Files (x86)\\Microsoft>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("microsoft" == text);
}

TEST_CASE(
    "MkTextDocument local link texts, with ()() round brackets in the path",
    "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText(
        "[folder](<file:///C:\\New folder(folder)\\New folder(folder)>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("folder" == text);
}

TEST_CASE(
    "MkTextDocument local link texts, with [,],(,),$,%,. symbols in the path",
    "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText(
        "[folder](<file:///C:\\New folder(folder)\\New folder(folder)\\New "
        "folder([h]folder&&$%).))))))))\\New (this) folder>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("folder" == text);
}

TEST_CASE("MkTextDocument strikethrough", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("~~strike~~");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("strike" == text);
}

TEST_CASE("MkTextDocument strikethrough, hide symbols only in 1st line",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("~~strike~~\n~~new line~~");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // unhide 2nd line
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("strike\n~~new line~~" == text);
}

TEST_CASE("MkTextDocument strikethrough, hide symbols in both lines",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("~~strike~~\n~~new line~~");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 2;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("strike\nnew line" == text);
}

TEST_CASE("MkTextDocument strikethrough, false positive at the back",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("strike~~");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("strike~~" == text);
}

TEST_CASE("MkTextDocument strikethrough, false positive at the front",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("~~strike");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("~~strike" == text);
}

TEST_CASE("MkTextDocument strikethrough with false positive at the back",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("~~strike~~ ~~");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("strike ~~" == text);
}

TEST_CASE("MkTextDocument strikethrough with false positive at the front",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("~~ ~~strike~~");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("~~ strike" == text);
}

TEST_CASE("MkTestDocument single checkbox unchecked", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("- [ ] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("☐" == text);
}

TEST_CASE("MkTestDocument double checkbox unchecked", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("- [ ] - [ ] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("☐☐" == text);
}

TEST_CASE("MkTestDocument checkbox unchecked, hidden only in 1st line",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("- [ ] \n- [ ] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 2;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("☐\n☐" == text);
}

TEST_CASE("MkTestDocument single checkbox checked", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("- [x] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("☑" == text);
}

TEST_CASE("MkTestDocument double checkbox checked", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("- [x] - [x] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("☑☑" == text);
}

TEST_CASE("MkTestDocument checkbox checked, hidden only in 1st line",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("- [x] \n- [x] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 2;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("☑\n☑" == text);
}

TEST_CASE("MkTextDocument single word bold, setMarkdown = false",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("**abc**");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = doc.toPlainText();
    REQUIRE("**abc**" == text);
}

TEST_CASE("MkTextDocument single word bold, setMarkdown = true",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("**abc**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = doc.toPlainText();
    REQUIRE("abc" == text);
}

TEST_CASE("MkTextDocument single word italic, underscore, setMarkdown = false",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("_abc_");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("_abc_" == text);
}

TEST_CASE("MkTextDocument single word italic, underscore, setMarkdown = true",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("_abc_");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("abc" == text);
}

TEST_CASE("MkTextDocument strikethrough, setMarkdown = false",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("~~strike~~");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("~~strike~~" == text);
}

TEST_CASE("MkTextDocument strikethrough, setMarkdown = true",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("~~strike~~");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("strike" == text);
}

TEST_CASE("MkTextDocument link texts with underscore, setMarkdown = false",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    // Note: no angle brackets around the URL. The parser only recognises
    // the '(</.../>)' form, so with markdown disabled the raw text is kept.
    doc.setPlainText("[lang_explain](https://sqlite.org/lang_explain.html)");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("[lang_explain](https://sqlite.org/lang_explain.html)" == text);
}

TEST_CASE("MkTextDocument link texts with underscore, setMarkdown = true",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("[lang_explain](<https://sqlite.org/lang_explain.html>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("lang_explain" == text);
}

TEST_CASE("MkTestDocument single checkbox unchecked, setMarkdown = false",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("- [ ] ");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("- [ ] " == text);
}

TEST_CASE("MkTestDocument single checkbox unchecked, setMarkdown = true",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("- [ ] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("☐" == text);
}

TEST_CASE("MkTestDocument single checkbox checked, setMarkdown = false",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("- [x] ");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("- [x] " == text);
}

TEST_CASE("MkTestDocument single checkbox checked, setMarkdown = true",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("- [x] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = edit.toPlainText();
    REQUIRE("☑" == text);
}

TEST_CASE("MkTextDocument bold and italic, setMarkdown = false",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("**bold** _italic_");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = doc.toPlainText();
    REQUIRE("**bold** _italic_" == text);
}

TEST_CASE("MkTextDocument bold and italic, setMarkdown = true",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("**bold** _italic_");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = doc.toPlainText();
    REQUIRE("bold italic" == text);
}

TEST_CASE("MkTextDocument bold and italic in two lines, setMarkdown = false",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("**bold** _italic_ \n **new line**");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = doc.toPlainText();
    REQUIRE("**bold** _italic_ \n **new line**" == text);
}

TEST_CASE("MkTextDocument bold and italic in two lines, setMarkdown = true",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("**bold** _italic_ \n **new line**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // no-op
    doc.cursorPosChangedHandle(&range);

    QString text = doc.toPlainText();
    REQUIRE("bold italic \n **new line**" == text);
}

TEST_CASE(
    "MkTextDocument bold and italic in two lines, setMarkdown = true, focus on "
    "1st line",
    "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("**bold** _italic_ \n **new line**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    // currentBlockNo == 0 is a VALID block index: it unhides the first
    // line, so the second line stays rendered. This is the one test in
    // the file that intentionally passes 0 to demonstrate the "unhide
    // a specific block" semantics with a valid index.
    range.hasSelection = false;
    range.currentBlockNo = 0;
    doc.cursorPosChangedHandle(&range);

    QString text = doc.toPlainText();
    REQUIRE("**bold** _italic_ \n new line" == text);
}

TEST_CASE(
    "MkTextDocument bold and italic in two lines, setMarkdown = true, focus on "
    "2nd line",
    "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("**bold** _italic_ \n **new line**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 1;  // unhide the second line
    doc.cursorPosChangedHandle(&range);

    QString text = doc.toPlainText();
    REQUIRE("bold italic \n **new line**" == text);
}

TEST_CASE("MkTextDocument inline triple backticks do not start a code block",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    doc.setPlainText("literal ``` inside a line\\nordinary text");

    REQUIRE(dynamic_cast<BlockData *>(doc.begin().userData()) == nullptr);
    REQUIRE(dynamic_cast<BlockData *>(doc.begin().next().userData()) == nullptr);
}


TEST_CASE("MkTextDocument supports all six ATX heading levels",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    doc.setPlainText(
        "# H1\n## H2\n### H3\n#### H4\n##### H5\n###### H6\n"
        "   ### Indented H3 ###\n####### not a heading\n#not a heading");

    const FragmentData::FormatSymbol expected[] = {
        FragmentData::HEADING1, FragmentData::HEADING2,
        FragmentData::HEADING3, FragmentData::HEADING4,
        FragmentData::HEADING5, FragmentData::HEADING6,
        FragmentData::HEADING3
    };

    for (int i = 0; i < 7; ++i)
    {
        QTextBlock block = doc.findBlockByNumber(i);
        FormatData *data = dynamic_cast<FormatData *>(block.userData());
        REQUIRE(data != nullptr);

        bool foundHeading = false;
        for (auto it = data->formats_begin(); it != data->formats_end(); ++it)
        {
            if ((*it)->getStatus() >= FragmentData::HEADING1 &&
                (*it)->getStatus() <= FragmentData::HEADING6)
            {
                REQUIRE((*it)->getStatus() == expected[i]);
                foundHeading = true;
                break;
            }
        }
        REQUIRE(foundHeading);
    }

    REQUIRE(doc.findBlockByNumber(7).userData() == nullptr);
    REQUIRE(doc.findBlockByNumber(8).userData() == nullptr);
}

TEST_CASE("MkTextDocument recognizes basic Markdown images", "[MkTextDocument]")
{
    MkTextDocument doc;
    doc.setPlainText("Before ![a cat](images/cat.png) after");

    FormatData *data = dynamic_cast<FormatData *>(doc.begin().userData());
    REQUIRE(data != nullptr);

    bool foundImage = false;
    for (auto it = data->formats_begin(); it != data->formats_end(); ++it)
    {
        if ((*it)->getStatus() == FragmentData::IMAGE)
        {
            foundImage = true;
            REQUIRE(data->getImageUrl((*it)->getStart()) == "images/cat.png");
            REQUIRE(data->getImageAltText((*it)->getStart()) == "a cat");
        }
    }
    REQUIRE(foundImage);
}

TEST_CASE("MkTextDocument preserves image Markdown when syncing a rendered block",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    const QString source = "Before ![a cat](images/cat.png) after";
    doc.setPlainText(source);

    // Apply the inline image formatting, which replaces the Markdown
    // expression in the visible document with a QTextImageFormat object.
    doc.applyMkSingleBlockHandle(0);
    REQUIRE(doc.begin().text().contains(QChar::ObjectReplacementCharacter));

    doc.saveSingleRawBlockHandler(0);
    REQUIRE(doc.getRawDocument()->toPlainText() == source);
}

TEST_CASE("MkTextDocument does not parse image syntax inside inline code",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    doc.setPlainText("`![cat](cat.png)`");

    FormatData *data = dynamic_cast<FormatData *>(doc.begin().userData());
    REQUIRE(data != nullptr);
    for (auto it = data->formats_begin(); it != data->formats_end(); ++it)
        REQUIRE((*it)->getStatus() != FragmentData::IMAGE);
}

TEST_CASE("Code blocks and inline fragments share BlockSupport",
          "[MkTextDocument]")
{
    BlockData codeBlock(BlockData::content);
    codeBlock.setStartBlock(2);
    codeBlock.setEndBlock(5);

    BlockSupport *codeSupport = &codeBlock;
    REQUIRE(codeSupport->getStart() == 2);
    REQUIRE(codeSupport->getEnd() == 5);

    FragmentData imageFragment(4, 17, FragmentData::IMAGE);
    BlockSupport *imageSupport = &imageFragment;
    REQUIRE(imageSupport->getStart() == 4);
    REQUIRE(imageSupport->getEnd() == 17);
}


// Regression: setting an empty document must clear both the rendered and raw
// Markdown documents instead of leaving the previous file visible.
TEST_CASE("MkTextDocument setPlainText empty clears existing content",
          "[MkTextDocument][regression]")
{
    MkTextDocument doc;
    doc.setPlainText("old content");
    REQUIRE(doc.toPlainText() == "old content");
    REQUIRE(doc.getRawDocument()->toPlainText() == "old content");

    doc.setPlainText(QString());

    REQUIRE(doc.toPlainText().isEmpty());
    REQUIRE(doc.getRawDocument()->toPlainText().isEmpty());
}

// clear() is a public reset operation, so neither document may retain stale
// source text after the visible document has been cleared.
TEST_CASE("MkTextDocument clear resets rendered and raw documents",
          "[MkTextDocument][regression]")
{
    MkTextDocument doc;
    doc.setPlainText("**content**");

    doc.clear();

    REQUIRE(doc.toPlainText().isEmpty());
    REQUIRE(doc.getRawDocument()->toPlainText().isEmpty());
}

// removeAllMkDataHandle() temporarily reveals Markdown syntax. It must preserve
// the raw source while clearing rendered formatting metadata.
TEST_CASE("MkTextDocument removeAllMkData preserves raw Markdown",
          "[MkTextDocument][regression]")
{
    MkTextDocument doc;
    const QString source = "Before ![cat](cat.png) and **bold**";
    doc.setPlainText(source);
    doc.applyMkSingleBlockHandle(0);
    REQUIRE(doc.begin().text().contains(QChar::ObjectReplacementCharacter));

    doc.removeAllMkDataHandle(0);

    REQUIRE(doc.toPlainText() == source);
    REQUIRE(doc.getRawDocument()->toPlainText() == source);
}

// Multiple images in one block must be restored in source order when the
// rendered block is synchronized back into the raw Markdown document.
TEST_CASE("MkTextDocument preserves multiple images when syncing a rendered block",
          "[MkTextDocument][regression]")
{
    MkTextDocument doc;
    const QString source =
        "![first](images/first.png) middle ![second](images/second.png)";
    doc.setPlainText(source);
    doc.applyMkSingleBlockHandle(0);

    REQUIRE(doc.begin().text().count(QChar::ObjectReplacementCharacter) == 2);

    doc.saveSingleRawBlockHandler(0);

    REQUIRE(doc.getRawDocument()->toPlainText() == source);
}


TEST_CASE("MkTextDocument supports image destinations with spaces and parentheses",
          "[MkTextDocument][regression]")
{
    MkTextDocument doc;
    const QString source =
        "![space](<images/cat photo (1).png>) and "
        "![paren](images/cat_(2).png)";
    doc.setPlainText(source);

    FormatData *data = dynamic_cast<FormatData *>(doc.begin().userData());
    REQUIRE(data != nullptr);

    QStringList imageUrls;
    for (auto it = data->formats_begin(); it != data->formats_end(); ++it)
    {
        if ((*it)->getStatus() == FragmentData::IMAGE)
            imageUrls.append(data->getImageUrl((*it)->getStart()));
    }
    REQUIRE(imageUrls.size() == 2);
    REQUIRE(imageUrls.at(0) == "images/cat photo (1).png");
    REQUIRE(imageUrls.at(1) == "images/cat_(2).png");

    doc.applyMkSingleBlockHandle(0);
    doc.saveSingleRawBlockHandler(0);
    REQUIRE(doc.getRawDocument()->toPlainText() == source);
}

TEST_CASE("MkTextDocument ignores escaped image openers",
          "[MkTextDocument][regression]")
{
    MkTextDocument doc;
    doc.setPlainText(R"(Not an image: \![cat](cat.png))");

    FormatData *data = dynamic_cast<FormatData *>(doc.begin().userData());
    if (data)
    {
        for (auto it = data->formats_begin(); it != data->formats_end(); ++it)
            REQUIRE((*it)->getStatus() != FragmentData::IMAGE);
    }
}


TEST_CASE("MkTextDocument keeps the correct image after deleting an earlier image",
          "[MkTextDocument][regression]")
{
    MkTextDocument doc;
    const QString source =
        "![first](images/first.png) middle ![second](images/second.png)";
    doc.setPlainText(source);
    doc.applyMkSingleBlockHandle(0);

    QTextCursor cursor(&doc);
    cursor.setPosition(doc.begin().position());
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    REQUIRE(cursor.selectedText() == QString(QChar::ObjectReplacementCharacter));
    cursor.removeSelectedText();

    doc.saveSingleRawBlockHandler(0);

    REQUIRE(doc.getRawDocument()->toPlainText() ==
            " middle ![second](images/second.png)");
}

// ============================================================================
// Images and symbol masking coexist
// ============================================================================

TEST_CASE("MkTextDocument hides markers in a block that also contains an image",
          "[MkTextDocument][regression]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("**bold** ![a](x.png)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 99;  // out of range: everything stays rendered
    doc.cursorPosChangedHandle(&range);

    const QString text = doc.begin().text();
    REQUIRE_FALSE(text.contains("**"));
    REQUIRE(text.startsWith("bold"));
    REQUIRE(text.count(QChar::ObjectReplacementCharacter) == 1);
    REQUIRE(doc.getRawDocument()->toPlainText() == "**bold** ![a](x.png)");
}

TEST_CASE("MkTextDocument hides markers around a leading image",
          "[MkTextDocument][regression]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("![a](x.png) *tail*");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 99;
    doc.cursorPosChangedHandle(&range);

    const QString text = doc.begin().text();
    REQUIRE(text.count(QChar::ObjectReplacementCharacter) == 1);
    REQUIRE_FALSE(text.contains("*"));
    REQUIRE(text.endsWith("tail"));
}

TEST_CASE("MkTextDocument renders plain parenthesis links",
          "[MkTextDocument][regression]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("[title](https://example.com/a(b))");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 99;
    doc.cursorPosChangedHandle(&range);

    REQUIRE(doc.begin().text() == "title");
    REQUIRE(doc.getRawDocument()->toPlainText() ==
            "[title](https://example.com/a(b))");
}

TEST_CASE("MkTextDocument keeps emphasis inside a plain link title",
          "[MkTextDocument][regression]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("[**bold**](https://example.com)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 99;
    doc.cursorPosChangedHandle(&range);

    REQUIRE(doc.begin().text() == "bold");
}

TEST_CASE("MkTextDocument renders autolinks", "[MkTextDocument][regression]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("see <https://example.com/x> now");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 99;
    doc.cursorPosChangedHandle(&range);

    REQUIRE(doc.begin().text() == "see https://example.com/x now");
    REQUIRE(doc.getRawDocument()->toPlainText() ==
            "see <https://example.com/x> now");
}

TEST_CASE("MkTextDocument ignores non-URI angle brackets",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("<not a link>");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 99;
    doc.cursorPosChangedHandle(&range);

    REQUIRE(doc.begin().text() == "<not a link>");
}

TEST_CASE("MkTextDocument honours backslash escapes",
          "[MkTextDocument][regression]")
{
    SECTION("escaped emphasis")
    {
        MkTextDocument doc;
        MkEdit edit;
        SelectRange range;

        doc.setPlainText("\\*not italic\\*");
        doc.setMarkdownHandle(true);
        edit.setDocument(&doc);

        range.hasSelection = false;
        range.currentBlockNo = 99;
        doc.cursorPosChangedHandle(&range);

        REQUIRE(doc.begin().text() == "*not italic*");
    }

    SECTION("escaped link")
    {
        MkTextDocument doc;
        MkEdit edit;
        SelectRange range;

        doc.setPlainText("\\[t\\](u)");
        doc.setMarkdownHandle(true);
        edit.setDocument(&doc);

        range.hasSelection = false;
        range.currentBlockNo = 99;
        doc.cursorPosChangedHandle(&range);

        REQUIRE(doc.begin().text() == "[t](u)");
    }
}

// ============================================================================
// Rendered <-> source position mapping
// ============================================================================

TEST_CASE("FormatData maps rendered positions back to source",
          "[MkTextDocument][FormatData]")
{
    MkTextDocument doc;
    doc.setPlainText("**ab**");

    FormatData *data = dynamic_cast<FormatData *>(doc.begin().userData());
    REQUIRE(data != nullptr);

    // Rendered "ab": position 0 -> source 2, position 2 (end) -> source 6.
    REQUIRE(data->getCalculatedCursorPos(0) == 2);
    REQUIRE(data->getCalculatedCursorPos(1) == 3);
    REQUIRE(data->getCalculatedCursorPos(2) == 6);
}

TEST_CASE("FormatData accounts for collapsed image characters",
          "[MkTextDocument][FormatData][regression]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("**a** ![i](p.png) b");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    FormatData *data = dynamic_cast<FormatData *>(doc.begin().userData());
    REQUIRE(data != nullptr);
    REQUIRE(data->imageSpans().size() == 1);
    REQUIRE(data->totalCollapsedChars() > 0);

    range.hasSelection = false;
    range.currentBlockNo = 99;
    doc.cursorPosChangedHandle(&range);

    // The image expression collapses to one object character. Mapping that
    // rendered position back must point at the start of the expression.
    const QString text = doc.begin().text();
    const int renderedImageStart = text.indexOf(QChar::ObjectReplacementCharacter);
    REQUIRE(renderedImageStart >= 0);
    REQUIRE(data->getCalculatedCursorPos(renderedImageStart) ==
            data->imageSpans().first().first);

    // A rendered position after the image maps past the whole expression.
    REQUIRE(data->getCalculatedCursorPos(renderedImageStart + 2) >
            data->imageSpans().first().second);
}

TEST_CASE("MkTextDocument registers link hit areas after an image",
          "[MkTextDocument][regression]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("![i](p.png) [title](<https://x.dev>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 99;
    doc.cursorPosChangedHandle(&range);

    const QString text = doc.begin().text();
    REQUIRE(text.endsWith("title"));

    bool found = false;
    for (auto it = doc.linkPosBegin(); it != doc.linkPosEnd(); ++it)
    {
        if (std::get<1>(*it) == text.indexOf("title"))
        {
            const QString *title = std::get<3>(*it);
            REQUIRE(title != nullptr);
            REQUIRE(*title == "title");
            found = true;
        }
    }
    REQUIRE(found);
}

TEST_CASE("MkTextDocument toggles a checkbox that follows an image",
          "[MkTextDocument][regression]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("![i](p.png) - [ ] task");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 99;
    doc.cursorPosChangedHandle(&range);

    const QString text = doc.begin().text();
    const int checkPos = text.indexOf(QChar(0x2610));  // ☐
    REQUIRE(checkPos >= 0);

    doc.pushCheckBoxHandle(doc.begin().position() + checkPos);

    REQUIRE(doc.getRawDocument()->toPlainText() ==
            "![i](p.png) - [x] task");
}

TEST_CASE("MkTextDocument strips a link title attribute from the destination",
          "[MkTextDocument][regression]")
{
    MkTextDocument doc;
    doc.setPlainText("[t](https://x.dev/a \"tip\")");

    FormatData *data = dynamic_cast<FormatData *>(doc.begin().userData());
    REQUIRE(data != nullptr);

    bool foundLink = false;
    for (auto it = data->formats_begin(); it != data->formats_end(); ++it)
    {
        if ((*it)->getStatus() != FragmentData::LINK_TITLE)
            continue;

        const QString *url =
            data->getLinkUrlFromTitleStart((*it)->getStart());
        REQUIRE(url != nullptr);
        REQUIRE(*url == "https://x.dev/a");
        foundLink = true;
    }
    REQUIRE(foundLink);
}

TEST_CASE("MkTextDocument ignores an escaped backtick", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("\\`a`");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 99;
    doc.cursorPosChangedHandle(&range);

    REQUIRE(doc.begin().text() == "`a`");
}

TEST_CASE("MkTextDocument keeps code spans working next to escapes",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("\\* `a*b*`");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 99;
    doc.cursorPosChangedHandle(&range);

    REQUIRE(doc.begin().text() == "* a*b*");
}

// ============================================================================
// GFM pipe tables
// ============================================================================

TEST_CASE("MkTextDocument detects table rows", "[MkTextDocument][Table]")
{
    MkTextDocument doc;
    doc.setPlainText("| a | b |\n| --- | --- |\n| 1 | 2 |");

    auto kindOf = [&doc](int blockNo)
    {
        QTextBlock block = doc.findBlockByNumber(blockNo);
        auto *table = dynamic_cast<TableData *>(block.userData());
        return table ? table->getRowKind() : TableData::Body;
    };

    REQUIRE(kindOf(0) == TableData::Header);
    REQUIRE(kindOf(1) == TableData::Delimiter);
    REQUIRE(kindOf(2) == TableData::Body);

    auto *header =
        dynamic_cast<TableData *>(doc.findBlockByNumber(0).userData());
    REQUIRE(header != nullptr);
    REQUIRE(header->getColumnCount() == 2);
    REQUIRE(header->getTableStartBlock() == 0);
    REQUIRE(header->getTableEndBlock() == 2);
    REQUIRE(header->getTabStops().size() == 3);
}

TEST_CASE("MkTextDocument renders table rows as tab separated cells",
          "[MkTextDocument][Table]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("| name | qty |\n| --- | --- |\n| tea | 2 |");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 99;
    doc.cursorPosChangedHandle(&range);

    const QTextBlock header = doc.begin();
    const QTextBlock delimiter = header.next();
    const QTextBlock body = delimiter.next();

    REQUIRE_FALSE(header.text().contains(QLatin1Char('|')));
    REQUIRE(header.text().contains(QLatin1Char('\t')));
    REQUIRE(header.text().contains(QStringLiteral("name")));

    // The delimiter row keeps no visible text.
    REQUIRE(delimiter.text().isEmpty());

    REQUIRE_FALSE(body.text().contains(QLatin1Char('|')));
    REQUIRE(body.text().contains(QLatin1Char('\t')));
    REQUIRE(body.text().contains(QStringLiteral("tea")));

    // The raw document is untouched.
    REQUIRE(doc.getRawDocument()->toPlainText() ==
            "| name | qty |\n| --- | --- |\n| tea | 2 |");
}

TEST_CASE("MkTextDocument shares tab stops between table rows",
          "[MkTextDocument][Table]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("| a | b |\n| --- | --- |\n| longer cell | 2 |");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 99;
    doc.cursorPosChangedHandle(&range);

    auto positions = [&doc](int blockNo)
    {
        QStringList values;
        for (const auto &tab :
             doc.findBlockByNumber(blockNo).blockFormat().tabPositions())
            values << QString::number(tab.position, 'f', 1);
        return values;
    };

    REQUIRE(positions(0) == positions(1));
    REQUIRE(positions(1) == positions(2));
    REQUIRE(positions(0).size() == 3);
}

TEST_CASE("MkTextDocument parses table column alignment",
          "[MkTextDocument][Table]")
{
    MkTextDocument doc;
    doc.setPlainText("| l | c | r |\n| :-- | :-: | --: |\n| 1 | 2 | 3 |");

    auto *delimiter =
        dynamic_cast<TableData *>(doc.findBlockByNumber(1).userData());
    REQUIRE(delimiter != nullptr);
    REQUIRE(delimiter->getAlignment(0) == Qt::AlignLeft);
    REQUIRE(delimiter->getAlignment(1) == Qt::AlignHCenter);
    REQUIRE(delimiter->getAlignment(2) == Qt::AlignRight);
}

TEST_CASE("MkTextDocument requires a delimiter row to start a table",
          "[MkTextDocument][Table]")
{
    SECTION("pipes alone are not a table")
    {
        MkTextDocument doc;
        doc.setPlainText("a | b\nc | d");
        REQUIRE(dynamic_cast<TableData *>(doc.begin().userData()) == nullptr);
    }

    SECTION("delimiter row needs a header with the same width")
    {
        MkTextDocument doc;
        doc.setPlainText("| a | b |\n| --- |");
        REQUIRE(dynamic_cast<TableData *>(doc.begin().userData()) == nullptr);
    }
}

TEST_CASE("MkTextDocument keeps table rows after a single-row edit",
          "[MkTextDocument][Table]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("| a | b |\n| --- | --- |\n| 1 | 2 |");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 99;
    doc.cursorPosChangedHandle(&range);

    // Edit the body row through the rendered document and sync the raw line.
    QTextCursor cursor(&doc);
    cursor.setPosition(doc.findBlockByNumber(2).position());
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertText(QStringLiteral("\tupdated\t9\t"));
    doc.saveSingleRawBlockHandler(2);
    doc.applyMkSingleBlockHandle(2);

    auto *body =
        dynamic_cast<TableData *>(doc.findBlockByNumber(2).userData());
    REQUIRE(body != nullptr);
    REQUIRE(body->getRowKind() == TableData::Body);
    REQUIRE(body->getColumnCount() == 2);
    REQUIRE(body->getTabStops().size() == 3);
}

// ============================================================================
// Source reconstruction (raw document safety)
// ============================================================================

TEST_CASE("MkTextDocument sourceText keeps table pipes and image syntax",
          "[MkTextDocument][RawSync][Table]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    const QString source =
        "| a | b |\n| --- | --- |\n| 1 | 2 |\n![x](p.png) **bold**";
    doc.setPlainText(source);
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 99;
    doc.cursorPosChangedHandle(&range);

    // The rendered document no longer holds the pipes or the image Markdown.
    REQUIRE(doc.toPlainText().contains(QLatin1Char('\t')));
    REQUIRE(doc.toPlainText().contains(QChar::ObjectReplacementCharacter));

    // Rebuilding the source must not leak rendered artifacts.
    REQUIRE(doc.sourceText() == source);

    doc.saveRawDocumentHandler();
    REQUIRE(doc.getRawDocument()->toPlainText() == source);
}

TEST_CASE("MkTextDocument sourceText uses the visible text of edited blocks",
          "[MkTextDocument][RawSync]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("**bold**\nplain");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    // Reveal block 1 only, then type into it.
    range.hasSelection = false;
    range.currentBlockNo = 1;
    doc.cursorPosChangedHandle(&range);

    QTextCursor cursor(&doc);
    cursor.setPosition(doc.findBlockByNumber(1).position());
    cursor.movePosition(QTextCursor::EndOfBlock);
    cursor.insertText(QStringLiteral("!"));
    doc.saveSingleRawBlockHandler(1);

    REQUIRE(doc.sourceText() == "**bold**\nplain!");
}

// ============================================================================
// Hover reveal (mouse over a table row shows its Markdown source)
// ============================================================================

TEST_CASE("MkTextDocument reveals the hovered table row and locks to the table",
          "[MkTextDocument][Table][Hover]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("| a | b |\n| --- | --- |\n| 1 | 2 |\ntail");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    // Cursor parked outside the table so only hover can reveal rows.
    range.hasSelection = false;
    range.currentBlockNo = 3;
    doc.cursorPosChangedHandle(&range);
    doc.setHoveredBlock(2);

    // Only row 2 shows source; the header keeps its rendered form.
    REQUIRE(doc.findBlockByNumber(2).text() == "| 1 | 2 |");
    REQUIRE(doc.findBlockByNumber(0).text().contains(QLatin1Char('\t')));

    // Leaving the row renders it again.
    doc.setHoveredBlock(-1);
    REQUIRE(doc.findBlockByNumber(2).text().contains(QLatin1Char('\t')));
    REQUIRE_FALSE(doc.findBlockByNumber(2).text().contains(QLatin1Char('|')));
}

TEST_CASE("MkTextDocument can reveal the whole hovered table",
          "[MkTextDocument][Table][Hover]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("| a | b |\n| --- | --- |\n| 1 | 2 |\ntail");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 3;
    doc.cursorPosChangedHandle(&range);

    doc.setHoverWholeTable(true);
    doc.setHoveredBlock(2);

    REQUIRE(doc.findBlockByNumber(0).text() == "| a | b |");
    REQUIRE(doc.findBlockByNumber(1).text() == "| --- | --- |");
    REQUIRE(doc.findBlockByNumber(2).text() == "| 1 | 2 |");

    doc.setHoveredBlock(-1);
    REQUIRE(doc.findBlockByNumber(0).text().contains(QLatin1Char('\t')));
}

TEST_CASE("MkTextDocument shows the source of every selected block",
          "[MkTextDocument][Table]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("| a | b |\n| --- | --- |\n| 1 | 2 |\ntail");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = true;
    range.selectionFirstStartBlock = 1;
    range.selectionEndBlock = 2;
    doc.cursorPosChangedHandle(&range);

    REQUIRE(doc.findBlockByNumber(1).text() == "| --- | --- |");
    REQUIRE(doc.findBlockByNumber(2).text() == "| 1 | 2 |");
    // Blocks outside the selection stay rendered.
    REQUIRE(doc.findBlockByNumber(0).text().contains(QLatin1Char('\t')));
}
