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
#include <catch2/catch.hpp>

#include "mkedit.h"
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
    REQUIRE(doc.begin().blockFormat().leftMargin() == Approx(18.0));
    REQUIRE(doc.getRawDocument()->toPlainText() == "> quoted text");
}

TEST_CASE("MkTextDocument blockquote supports indentation and empty quotes",
          "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    SelectRange range;

    doc.setPlainText("   > indented quote\\n>");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    range.hasSelection = false;
    range.currentBlockNo = 2;  // keep both blocks rendered
    doc.cursorPosChangedHandle(&range);

    REQUIRE(doc.begin().text() == "indented quote");
    REQUIRE(doc.begin().blockFormat().leftMargin() == Approx(18.0));
    REQUIRE(doc.begin().next().text().isEmpty());
    REQUIRE(doc.begin().next().blockFormat().leftMargin() == Approx(18.0));
}

TEST_CASE("MkTextDocument blockquote marker requires whitespace",
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

    REQUIRE(doc.begin().text() == ">not a quote");
    REQUIRE(doc.begin().blockFormat().leftMargin() == Approx(0.0));
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
