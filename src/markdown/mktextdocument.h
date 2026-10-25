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
#include <QVector>
#include <QtGlobal>

#include "edittypes.h"
#include "formatcollection.h"
#include "markdownparser.h"
#include "selectrange.h"

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

    void revealAllMkSymbols();

public slots:
    void cursorPosChangedHandle(SelectRange *const range,
                                const bool readOnly = false);
    void removeAllMkDataHandle(int blockNo);
    void applyAllMkDataHandle(int blockNumber);
    void applyMkSingleBlockHandle(int blockNumber);
    void enterKeyPressedHandle(int blockNumber, int &newCursorPos);
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

    void undoStackPush(QUndoCommand *edit);
    void undoStackUndo(bool &success);
    void undoStackRedo(bool &success);

signals:
    void disconnectCursorPos(bool override = false);
    void connectCurosPos(bool override = false);

private:
    struct CheckingBlock
    {
        QTextBlock start;
        QTextBlock end;
    };

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

    SelectRange undoSelectRange;
    SelectRange redoSelectRange;

    void identifyUserData();
    void identifyUserData(QTextBlock &block);
    void formatAllLines(const QTextDocument &original,
                        MkTextDocument &formatted);

    void setCodeBlockMargin(QTextBlock &block, int leftMargin = 0,
                            int rightMargin = 0, int topMargin = 0,
                            int bottomMargin = 0);
    void resetTextBlockFormat(QTextBlock block);
    void applyMkFormat(QTextBlock &block, const BlockSupport &span,
                       FragmentData::FormatSymbol status,
                       FormatCollection &formatCollection);
    void applyCheckBoxLinkEffect(FormatData *data, QTextBlock &block,
                                 QTextCursor &cursor);
    void hideSymbols(QTextBlock &block, const QString &symbol);
    void hideSymbolsAtPos(QString &text, int pos, const QString &symbol);

    void showSymbols(QTextBlock &block, const QString &symbol);
    void removeCheckBoxLinkMousePosition(QTextBlock &block,
                                         FormatData *formatData,
                                         SelectRange *range = nullptr);
    void showSymbolsAtPos(QString &text, int pos, const QString &symbol);

    void autoCompleteCodeBlock(int blockNumber, bool &success);
    BlockData *checkValidCodeBlock(QTextBlock &block);

    void numberListDetect(int blockNumber, int &newCursorPosition);
    int numberListGetSpaces(const QString &text);
    QString numberListGetNextNumber(const QString &text);

    void hideMKSymbolsFromPreviousSelectedBlocks(SelectRange *const range);
    void showMKSymbolsFromCurrentSelectedBlocks(SelectRange *const range);
    void showHideCodeBlock(BlockData *data, bool hide, int fontSize);
};

struct UndoData
{
    QTextEdit *view;
    EditType *viewEditTypeStore;
    QTextDocument *doc;
    QString text;
    int blockNo;
    int posInBlock;
    QString oldText;
    bool undoRedoSkip;
    bool selectAll;
    int scrollValue;
    SelectRange oldSelectRange;
    SelectRange *viewSelectRangeStore;
    QString oldBlock;
    QString newBlock;
    EditType editType;
};

class EditCommand : public QUndoCommand
{
public:
    EditCommand(UndoData &data);

    void undo() override;
    void redo() override;

private:
    QTextEdit *view;
    EditType *viewEditTypeStore;
    MkTextDocument *doc;
    QString redoText;
    QString undoText;
    int blockNo;
    int posInBlock;
    int scrollValue;
    EditType editType;

    bool isConstructorRedo;
    SelectRange undoSelectRange;
    SelectRange *viewSelectRangeStore;
};

#endif  // MKTEXTDOCUMENT_H