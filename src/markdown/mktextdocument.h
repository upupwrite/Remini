#ifndef MKTEXTDOCUMENT_H
#define MKTEXTDOCUMENT_H

#include <blockdata.h>
#include <formatdata.h>
#include <linedata.h>

#include <QAbstractTextDocumentLayout>
#include <QDesktopServices>
#include <QObject>
#include <QPair>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QString>
#include <QStringList>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QUndoCommand>
#include <QUndoStack>
#include <QSet>
#include <QVector>
#include <QtGlobal>

#include "editcommand.h"
#include "edittypes.h"
#include "formatcollection.h"
#include "markdownparser.h"
#include "markdownrenderer.h"
#include "selectrange.h"
#include "tabledata.h"

class MkTextDocument : public QTextDocument
{
    Q_OBJECT
public:
    explicit MkTextDocument(QObject *parent = nullptr);
    void setPlainText(const QString &text);
    void setUndoRedoText(const QString &text);
    void setUndoRedoText(const int blockNo, const QString &text);
    void setUndoEnterPressedText(const int blockNo, const QString &text);
    void clear() override;

    QVector<QPair<int, int>>::const_iterator checkMarkPosBegin()
    {
        return checkMarkPositions.cbegin();
    };
    QVector<QPair<int, int>>::const_iterator checkMarkPosEnd()
    {
        return checkMarkPositions.cend();
    };

    QVector<std::tuple<int, int, int, const QString *>>::const_iterator
    linkPosBegin()
    {
        return linkPositions.cbegin();
    };
    QVector<std::tuple<int, int, int, const QString *>>::const_iterator
    linkPosEnd()
    {
        return linkPositions.cend();
    };

    void setFilePath(const QString &filePath);
    void setFileName(const QString &fileName);
    void setCursorPos(const int blockNo, const int characterNo);
    int getBlockNo() const;
    int getCharacterNo() const;
    QString getFilePath() const;
    QString getFileName() const;
    QTextDocument *getRawDocument();
    QString rawText() const;
    QString rawBlockText(int blockNo) const;

    // Source Markdown rebuilt block by block: rendered blocks take their text
    // from `rawDocument`, blocks being edited contribute their visible text.
    QString sourceText() const;

    void revealAllMkSymbols();

    bool isHoverWholeTable() const;
    int hoveredBlock() const;

private:
    // Re-renders the document so exactly the cursor/selection/hover blocks
    // show their Markdown source.
    void refreshVisibleBlocks();

public slots:
    void cursorPosChangedHandle(SelectRange *const range,
                                const bool readOnly = false);
    // Reveal one block (its Markdown source) while the mouse hovers it.
    // `-1` hides the hovered block again. Only table rows are affected.
    void setHoveredBlock(const int blockNo);
    void setHoverWholeTable(const bool whole);
    void removeAllMkDataHandle(int blockNo);
    void applyAllMkDataHandle(int blockNumber);
    void applyMkSingleBlockHandle(int blockNumber);
    void enterKeyPressedHandle(int blockNumber, int &newCursorPos,
                               bool continueQuote);
    void quoteLeftKeyPressedHandle(int blockNumber, bool &success);
    void checkRightClockOnCodeBlockHandle(int blockNumber, bool &valid);
    void selectBlockCopyHandle(int blockNumber, int &startPos, int &endPos);
    void duplicateLineHandle(int blockNumber);
    void smartSelectionHandle(int blockNumber, QTextCursor &cursor);
    void saveRawDocumentHandler();
    void saveSingleRawBlockHandler(int blockNumber);
    void saveEnterPressRawBlockHandler(int blockNumber, QString text);

    void pushCheckBoxHandle(const int position);
    void pushLinkHandle(const int blockNo, const int posInBlock);
    void autoInsertSymbolHandle(const int position, int &newPosition);
    void setMarkdownHandle(bool state);
    void cursorUpdateHandle(const int blockNo, const int characterNo);

    void undoStackPush(const EditSnapshot &snapshot);
    void undoStackUndo(EditRestore &restore);
    void undoStackRedo(EditRestore &restore);

signals:
    void disconnectCursorPos(bool override = false);
    void connectCursorPos(bool override = false);

private:
    QTextDocument rawDocument;

    QString filePath;
    QString fileName;
    int blockNo;
    int characterNo;
    SelectRange selectRange;

    MarkdownParser parser;

    QRegularExpression regexCodeBlock;
    QRegularExpression regexHorizontalLine;
    QRegularExpression regexNumbering;
    QRegularExpression regexCheckBox;
    QRegularExpression regexBulletCheckBox;
    QRegularExpression regexBulletPoints;

    QUndoStack undoStack;

    QVector<QPair<int, int>> checkMarkPositions;
    QVector<std::tuple<int, int, int, const QString *>> linkPositions;

    QColor linkColor;
    bool disableMarkdownState;

    int hoveredBlockNo = -1;
    bool hoverWholeTable = false;
    // Blocks currently shown because of the caret or selection. Hover-revealed
    // blocks are excluded so they never move the caret.
    QSet<int> cursorVisibleBlocks;

    void identifyUserData();

    // Pixel offsets of the column stops shared by every row of a table.
    QVector<qreal> tableTabStops(const QVector<int> &columnWidths);

    void autoCompleteCodeBlock(int blockNumber, bool &success);
    BlockData *checkValidCodeBlock(QTextBlock &block);

    void numberListDetect(int blockNumber, int &newCursorPosition,
                          bool continueQuote);
    int numberListGetSpaces(const QString &text);
    QString numberListGetNextNumber(const QString &text);

    void hideMKSymbolsFromPreviousSelectedBlocks(SelectRange *const range);
    void showMKSymbolsFromCurrentSelectedBlocks(SelectRange *const range);
};

#endif  // MKTEXTDOCUMENT_H
