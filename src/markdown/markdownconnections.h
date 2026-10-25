#ifndef MARKDOWNCONNECTIONS_H
#define MARKDOWNCONNECTIONS_H

class MkEdit;
class MkTextDocument;

// Centralized wiring between a Markdown editor view and its document. Both the
// application (ViewsHandler) and the tests use these helpers so the signal/slot
// contract lives in exactly one place.
void connectMarkdownEditorToDocument(MkEdit &editor, MkTextDocument &document);
void disconnectMarkdownEditorFromDocument(MkEdit &editor,
                                          MkTextDocument &document);

#endif  // MARKDOWNCONNECTIONS_H
