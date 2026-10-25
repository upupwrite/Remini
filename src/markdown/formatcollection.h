#ifndef FORMATCOLLECTION_H
#define FORMATCOLLECTION_H

#include <QColor>
#include <QString>
#include <QTextCharFormat>
#include <QtGlobal>

#define MAXIMUM_FONT_SIZE 30
#define MINIMUM_FONT_SIZE 7

// Owns the QTextCharFormat objects used to style rendered Markdown. Created
// per render pass with the current font size so heading sizes scale with zoom.
class FormatCollection
{
public:
    explicit FormatCollection(int fontSize)
    {
        linkColor.setRgb(50, 118, 254);

        bold.setFontWeight(QFont::ExtraBold);
        italic.setFontItalic(true);
        strikethrough.setFontStrikeOut(true);
        heading1.setFontPointSize(fontSize * 1.65);
        heading2.setFontPointSize(fontSize * 1.42);
        heading3.setFontPointSize(fontSize * 1.28);
        // Keep H4 and H5 close to the established H3/H4 styles so they remain
        // clearly readable instead of becoming too small and low-contrast.
        heading4.setFontPointSize(fontSize * 1.28);
        heading5.setFontPointSize(fontSize * 1.16);
        heading6.setFontPointSize(fontSize);
        heading1.setFontWeight(QFont::ExtraBold);
        heading2.setFontWeight(QFont::Bold);
        heading3.setFontWeight(QFont::DemiBold);
        heading4.setFontWeight(QFont::DemiBold);
        heading5.setFontWeight(QFont::DemiBold);
        heading6.setFontWeight(QFont::DemiBold);
        heading1.setForeground(QColor(QStringLiteral("#315F91")));
        heading2.setForeground(QColor(QStringLiteral("#3C6F9F")));
        heading3.setForeground(QColor(QStringLiteral("#477DA5")));
        heading4.setForeground(QColor(QStringLiteral("#477DA5")));
        heading5.setForeground(QColor(QStringLiteral("#4F7188")));
        heading6.setForeground(QColor(QStringLiteral("#65727C")));
        heading6.setFontItalic(true);
        link.setFontUnderline(true);
        link.setUnderlineColor(linkColor);
        link.setForeground(linkColor);

        // Qt 6.1 deprecated setFontFamily() in favour of setFontFamilies().
        // Qt 5 has no setFontFamilies(), so pick the API at compile time.
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
        codeSpan.setFontFamilies({QStringLiteral("Monospace")});
#else
        codeSpan.setFontFamily(QStringLiteral("Monospace"));
#endif
        codeSpan.setFontFixedPitch(true);
    }

    QTextCharFormat *getBold() { return &bold; }
    QTextCharFormat *getItalic() { return &italic; }
    QTextCharFormat *getStrikethrough() { return &strikethrough; }
    QTextCharFormat *getHeading1() { return &heading1; }
    QTextCharFormat *getHeading2() { return &heading2; }
    QTextCharFormat *getHeading3() { return &heading3; }
    QTextCharFormat *getHeading4() { return &heading4; }
    QTextCharFormat *getHeading5() { return &heading5; }
    QTextCharFormat *getHeading6() { return &heading6; }
    QTextCharFormat *getLink() { return &link; }
    QTextCharFormat *getCodeSpan() { return &codeSpan; }
    QColor linkColor;

private:
    QTextCharFormat bold;
    QTextCharFormat italic;
    QTextCharFormat strikethrough;
    QTextCharFormat heading1;
    QTextCharFormat heading2;
    QTextCharFormat heading3;
    QTextCharFormat heading4;
    QTextCharFormat heading5;
    QTextCharFormat heading6;
    QTextCharFormat link;
    QTextCharFormat codeSpan;
};

#endif  // FORMATCOLLECTION_H
