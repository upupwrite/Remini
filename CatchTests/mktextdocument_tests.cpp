// Catch2 v2 / v3 compatibility.
#if __has_include(<catch2/catch_test_macros.hpp>)
#  include <catch2/catch_test_macros.hpp>
#else
#  include <catch2/catch_all.hpp>
#endif

#include "mktextdocument.h"
#include "mkedit.h"
#include <QApplication>

// ---------------------------------------------------------------------------
// Helper: run the "hide markdown symbols for this block" pipeline the same
// way the editor does. The second argument (readOnly) must be provided
// because MkTextDocument::cursorPosChangedHandle takes it explicitly.
// ---------------------------------------------------------------------------
static void hideSymbolsAt(MkTextDocument &doc, int currentBlockNo)
{
    SelectRange range{};
    range.hasSelection = false;
    range.currentBlockNo = currentBlockNo;
    doc.cursorPosChangedHandle(&range, false);
}

TEST_CASE("MkTextDocument simple text", "[MkTextDocument]")
{
    MkTextDocument doc;
    doc.setPlainText("abc");
    REQUIRE("abc" == doc.toPlainText());
}

TEST_CASE("MkTextDocument bold text", "[MkTextDocument]")
{
    MkTextDocument doc;
    doc.setPlainText("**abc**");
    REQUIRE("**abc**" == doc.toPlainText());
}

TEST_CASE("MkTextDocument italic text", "[MkTextDocument]")
{
    MkTextDocument doc;
    doc.setPlainText("*abc*");
    REQUIRE("*abc*" == doc.toPlainText());
}

TEST_CASE("MkTextDocument single word bold, hide symbols", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("**abc**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("abc" == edit.toPlainText());
}

TEST_CASE("MkTextDocument bold, hide symbols, multiple words", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("**abc** **qwerty**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("abc qwerty" == edit.toPlainText());
}

TEST_CASE("MkTextDocument bold, hide symbols on multiple lines", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("**abc** **qwerty**\n **new** **line**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 2);

    REQUIRE("abc qwerty\n new line" == edit.toPlainText());
}

TEST_CASE("MkTextDocument bold, hide symbols only on 1st line", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("**abc** **qwerty**\n **new** **line**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("abc qwerty\n **new** **line**" == edit.toPlainText());
}

TEST_CASE("MkTextDocument bold, hide symbols, underscore", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("__abc__");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("abc" == edit.toPlainText());
}

TEST_CASE("MkTextDocument bold, hide underscore symbols only on 1st line", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("__abc__\n __123__");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("abc\n __123__" == edit.toPlainText());
}

TEST_CASE("MkTextDocument bold, hide underscore symbols only on both lines", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("__abc__\n __123__");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 2);

    REQUIRE("abc\n 123" == edit.toPlainText());
}

TEST_CASE("MkTextDocument single word italic, hide symbols", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("*abc*");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 2);

    REQUIRE("abc" == edit.toPlainText());
}

TEST_CASE("MkTextDocument italic, hide symbol on 1st line", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("*abc*\n*hello*");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("abc\n*hello*" == edit.toPlainText());
}

TEST_CASE("MkTextDocument italic, hide symbols on both lines", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("*abc*\n*hello*");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 2);

    REQUIRE("abc\nhello" == edit.toPlainText());
}

TEST_CASE("MkTextDocument single word italic, hide symbols, underscore", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("_abc_");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("abc" == edit.toPlainText());
}

TEST_CASE("MkTextDocument italic, hide underscore symbols on 1st line", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("_abc_\n_hello_");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("abc\n_hello_" == edit.toPlainText());
}

TEST_CASE("MkTextDocument italic, hide underscore symbols on both lines", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("_abc_\n_hello_");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 2);

    REQUIRE("abc\nhello" == edit.toPlainText());
}

TEST_CASE("MkTextDocument single word bold plus false bold sign, hide only bold symbols", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("**abc** **");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("abc **" == edit.toPlainText());
}

TEST_CASE("MkTextDocument single word bold plus false bold sign, hide only bold symbols, underscore", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("__abc__ __");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("abc __" == edit.toPlainText());
}

TEST_CASE("MkTextDocument single word italic plus false italic sign, hide only italic sign", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("*abc* *");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("abc *" == edit.toPlainText());
}

TEST_CASE("MkTextDocument single word italic plus false italic sign, hide only italic sign, underscore", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("_abc_ _");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("abc _" == edit.toPlainText());
}

TEST_CASE("MkTextDocument link texts", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("[youtube](<www.youtube.com>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("youtube" == edit.toPlainText());
}

TEST_CASE("MkTextDocument link texts, hide symbols only in 1st line", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("[youtube](<www.youtube.com>)\n[google](<www.google.com>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("youtube\n[google](<www.google.com>)" == edit.toPlainText());
}

TEST_CASE("MkTextDocument link texts, hide symbols only in both lines", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("[youtube](<www.youtube.com>)\n[google](<www.google.com>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 2);

    REQUIRE("youtube\ngoogle" == edit.toPlainText());
}

TEST_CASE("MkTextDocument link texts with underscore", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("[lang_explain](<https://sqlite.org/lang_explain.html>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 2);

    REQUIRE("lang_explain" == edit.toPlainText());
}

TEST_CASE("MkTextDocument link texts with empty title", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("[](<www.google.com>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 2);

    REQUIRE("" == edit.toPlainText());
}

TEST_CASE("MkTextDocument link texts false positive", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("abc](<123");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("abc](<123" == edit.toPlainText());
}

TEST_CASE("MkTextDocument local link texts, with () round brackets in the path", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("[microsoft](<file:///C:\\Program Files (x86)\\Microsoft>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("microsoft" == edit.toPlainText());
}

TEST_CASE("MkTextDocument local link texts, with ()() round brackets in the path", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("[folder](<file:///C:\\New folder(folder)\\New folder(folder)>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("folder" == edit.toPlainText());
}

TEST_CASE("MkTextDocument local link texts, with [,],(,),$,%,. symbols in the path", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("[folder](<file:///C:\\New folder(folder)\\New folder(folder)\\New folder([h]folder&&$%).))))))))\\New (this) folder>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("folder" == edit.toPlainText());
}

TEST_CASE("MkTextDocument strikethrough", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("~~strike~~");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("strike" == edit.toPlainText());
}

TEST_CASE("MkTextDocument strikethrough, hide symbols only in 1st line", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("~~strike~~\n~~new line~~");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("strike\n~~new line~~" == edit.toPlainText());
}

TEST_CASE("MkTextDocument strikethrough, hide symbols in both lines", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("~~strike~~\n~~new line~~");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 2);

    REQUIRE("strike\nnew line" == edit.toPlainText());
}

TEST_CASE("MkTextDocument strikethrough, false positive at the back", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("strike~~");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("strike~~" == edit.toPlainText());
}

TEST_CASE("MkTextDocument strikethrough, false positive at the front", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("~~strike");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("~~strike" == edit.toPlainText());
}

TEST_CASE("MkTextDocument strikethrough with false positive at the back", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("~~strike~~ ~~");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("strike ~~" == edit.toPlainText());
}

TEST_CASE("MkTextDocument strikethrough with false positive at the front", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("~~ ~~strike~~");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE(" strike~~" == edit.toPlainText());
}

TEST_CASE("MkTestDocument single checkbox unchecked", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("- [ ] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("☐" == edit.toPlainText());
}

TEST_CASE("MkTestDocument double checkbox unchecked", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("- [ ] - [ ] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("☐☐" == edit.toPlainText());
}

TEST_CASE("MkTestDocument checkbox unchecked, hidden only in 1st line", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("- [ ] \n- [ ] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 2);

    REQUIRE("☐\n☐" == edit.toPlainText());
}

TEST_CASE("MkTestDocument single checkbox checked", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("- [x] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("☑" == edit.toPlainText());
}

TEST_CASE("MkTestDocument double checkbox checked", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("- [x] - [x] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("☑☑" == edit.toPlainText());
}

TEST_CASE("MkTestDocument checkbox checked, hidden only in 1st line", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("- [x] \n- [x] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 2);

    REQUIRE("☑\n☑" == edit.toPlainText());
}

TEST_CASE("MkTextDocument single word bold, setMarkdown = false", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("**abc**");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("**abc**" == doc.toPlainText());
}

TEST_CASE("MkTextDocument single word bold, setMarkdown = true", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("**abc**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("abc" == doc.toPlainText());
}

TEST_CASE("MkTextDocument single word italic, underscore, setMarkdown = false", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("_abc_");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("_abc_" == edit.toPlainText());
}

TEST_CASE("MkTextDocument single word italic, underscore, setMarkdown = true", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("_abc_");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("abc" == edit.toPlainText());
}

TEST_CASE("MkTextDocument strikethrough, setMarkdown = false", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("~~strike~~");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("~~strike~~" == edit.toPlainText());
}

TEST_CASE("MkTextDocument strikethrough, setMarkdown = true", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("~~strike~~");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("strike" == edit.toPlainText());
}

TEST_CASE("MkTextDocument link texts with underscore, setMarkdown = false", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("[lang_explain](https://sqlite.org/lang_explain.html)");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("[lang_explain](https://sqlite.org/lang_explain.html)" == edit.toPlainText());
}

TEST_CASE("MkTextDocument link texts with underscore, setMarkdown = true", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("[lang_explain](<https://sqlite.org/lang_explain.html>)");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("lang_explain" == edit.toPlainText());
}

TEST_CASE("MkTestDocument single checkbox unchecked, setMarkdown = false", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("- [ ] ");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("- [ ] " == edit.toPlainText());
}

TEST_CASE("MkTestDocument single checkbox unchecked, setMarkdown = true", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("- [ ] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("☐" == edit.toPlainText());
}

TEST_CASE("MkTestDocument single checkbox checked, setMarkdown = false", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("- [x] ");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("- [x] " == edit.toPlainText());
}

TEST_CASE("MkTestDocument single checkbox checked, setMarkdown = true", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("- [x] ");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("☑" == edit.toPlainText());
}

TEST_CASE("MkTextDocument bold and italic, setMarkdown = false", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("**bold** _italic_");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("**bold** _italic_" == doc.toPlainText());
}

TEST_CASE("MkTextDocument bold and italic, setMarkdown = true", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("**bold** _italic_");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("bold italic" == doc.toPlainText());
}

TEST_CASE("MkTextDocument bold and italic in two lines, setMarkdown = false", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("**bold** _italic_ \n **new line**");
    doc.setMarkdownHandle(false);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("**bold** _italic_ \n **new line**" == doc.toPlainText());
}

TEST_CASE("MkTextDocument bold and italic in two lines, setMarkdown = true", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("**bold** _italic_ \n **new line**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("bold italic \n **new line**" == doc.toPlainText());
}

TEST_CASE("MkTextDocument bold and italic in two lines, setMarkdown = true, focus on 1st line", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("**bold** _italic_ \n **new line**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 0);

    REQUIRE("**bold** _italic_ \n new line" == doc.toPlainText());
}

TEST_CASE("MkTextDocument bold and italic in two lines, setMarkdown = true, focus on 2nd line", "[MkTextDocument]")
{
    MkTextDocument doc;
    MkEdit edit;
    doc.setPlainText("**bold** _italic_ \n **new line**");
    doc.setMarkdownHandle(true);
    edit.setDocument(&doc);

    hideSymbolsAt(doc, 1);

    REQUIRE("bold italic \n **new line**" == doc.toPlainText());
}
