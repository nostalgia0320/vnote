#include "veditutils.h"

#include <QTextDocument>
#include <QDebug>
#include <QTextEdit>
#include <QScrollBar>

#include "vutils.h"

void VEditUtils::removeBlock(QTextBlock &p_block, QString *p_text)
{
    QTextCursor cursor(p_block);
    removeBlock(cursor, p_text);
}

void VEditUtils::removeBlock(QTextCursor &p_cursor, QString *p_text)
{
    const QTextDocument *doc = p_cursor.document();
    int blockCount = doc->blockCount();
    int blockNum = p_cursor.block().blockNumber();

    p_cursor.select(QTextCursor::BlockUnderCursor);
    if (p_text) {
        *p_text = selectedText(p_cursor) + "\n";
    }

    p_cursor.deleteChar();

    // Deleting the first block will leave an empty block.
    // Deleting the last empty block will not work with deleteChar().
    if (blockCount == doc->blockCount()) {
        if (blockNum == blockCount - 1) {
            // The last block.
            p_cursor.deletePreviousChar();
        } else {
            p_cursor.deleteChar();
        }
    }

    if (p_cursor.block().blockNumber() < blockNum) {
        p_cursor.movePosition(QTextCursor::NextBlock);
    }

    p_cursor.movePosition(QTextCursor::StartOfBlock);
}

bool VEditUtils::insertBlockWithIndent(QTextCursor &p_cursor)
{
    V_ASSERT(!p_cursor.hasSelection());
    p_cursor.insertBlock();
    return indentBlockAsPreviousBlock(p_cursor);
}

bool VEditUtils::insertListMarkAsPreviousBlock(QTextCursor &p_cursor)
{
    bool ret = false;
    QTextBlock block = p_cursor.block();
    QTextBlock preBlock = block.previous();
    if (!preBlock.isValid()) {
        return false;
    }

    QString text = preBlock.text();
    QRegExp regExp("^\\s*(-|\\d+\\.)\\s");
    int regIdx = regExp.indexIn(text);
    if (regIdx != -1) {
        ret = true;
        V_ASSERT(regExp.captureCount() == 1);
        QString markText = regExp.capturedTexts()[1];
        if (markText == "-") {
            // Insert - in front.
            p_cursor.insertText("- ");
        } else {
            // markText is like "123.".
            V_ASSERT(markText.endsWith('.'));
            bool ok = false;
            int num = markText.left(markText.size() - 1).toInt(&ok, 10);
            V_ASSERT(ok);
            num++;
            p_cursor.insertText(QString::number(num, 10) + ". ");
        }
    }

    return ret;

}

bool VEditUtils::indentBlockAsPreviousBlock(QTextCursor &p_cursor)
{
    bool changed = false;
    QTextBlock block = p_cursor.block();
    if (block.blockNumber() == 0) {
        // The first block.
        return false;
    }

    QTextBlock preBlock = block.previous();
    QString text = preBlock.text();
    QRegExp regExp("(^\\s*)");
    regExp.indexIn(text);
    V_ASSERT(regExp.captureCount() == 1);
    QString leadingSpaces = regExp.capturedTexts()[1];

    moveCursorFirstNonSpaceCharacter(p_cursor, QTextCursor::MoveAnchor);
    if (!p_cursor.atBlockStart()) {
        p_cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
        p_cursor.removeSelectedText();
        changed = true;
    }

    if (!leadingSpaces.isEmpty()) {
        p_cursor.insertText(leadingSpaces);
        changed = true;
    }

    p_cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);

    return changed;
}

void VEditUtils::moveCursorFirstNonSpaceCharacter(QTextCursor &p_cursor,
                                                  QTextCursor::MoveMode p_mode)
{
    QTextBlock block = p_cursor.block();
    QString text = block.text();
    int idx = 0;
    for (; idx < text.size(); ++idx) {
        if (text[idx].isSpace()) {
            continue;
        } else {
            break;
        }
    }

    p_cursor.setPosition(block.position() + idx, p_mode);
}

void VEditUtils::removeObjectReplacementCharacter(QString &p_text)
{
    QRegExp orcBlockExp(QString("[\\n|^][ |\\t]*\\xfffc[ |\\t]*(?=\\n)"));
    p_text.remove(orcBlockExp);
    p_text.remove(QChar::ObjectReplacementCharacter);
}

QString VEditUtils::selectedText(const QTextCursor &p_cursor)
{
    QString text = p_cursor.selectedText();
    text.replace(QChar::ParagraphSeparator, '\n');
    return text;
}

// Use another QTextCursor to remain the selection.
void VEditUtils::indentSelectedBlocks(const QTextDocument *p_doc,
                                      const QTextCursor &p_cursor,
                                      const QString &p_indentationText,
                                      bool p_isIndent)
{
    int nrBlocks = 1;
    int start = p_cursor.selectionStart();
    int end = p_cursor.selectionEnd();

    QTextBlock sBlock = p_doc->findBlock(start);
    if (start != end) {
        QTextBlock eBlock = p_doc->findBlock(end);
        nrBlocks = eBlock.blockNumber() - sBlock.blockNumber() + 1;
    }

    QTextCursor bCursor(sBlock);
    bCursor.beginEditBlock();
    for (int i = 0; i < nrBlocks; ++i) {
        if (p_isIndent) {
            indentBlock(bCursor, p_indentationText);
        } else {
            unindentBlock(bCursor, p_indentationText);
        }

        bCursor.movePosition(QTextCursor::NextBlock);
    }
    bCursor.endEditBlock();
}

void VEditUtils::indentBlock(QTextCursor &p_cursor,
                             const QString &p_indentationText)
{
    QTextBlock block = p_cursor.block();
    if (block.length() > 1) {
        p_cursor.movePosition(QTextCursor::StartOfBlock);
        p_cursor.insertText(p_indentationText);
    }
}

void VEditUtils::unindentBlock(QTextCursor &p_cursor,
                               const QString &p_indentationText)
{
    QTextBlock block = p_cursor.block();
    QString text = block.text();
    if (text.isEmpty()) {
        return;
    }

    p_cursor.movePosition(QTextCursor::StartOfBlock);
    if (text[0] == '\t') {
        p_cursor.deleteChar();
    } else if (text[0].isSpace()) {
        int width = p_indentationText.size();
        for (int i = 0; i < width; ++i) {
            if (text[i] == ' ') {
                p_cursor.deleteChar();
            } else {
                break;
            }
        }
    }
}

bool VEditUtils::findTargetWithinBlock(QTextCursor &p_cursor,
                                       QTextCursor::MoveMode p_mode,
                                       QChar p_target,
                                       bool p_forward,
                                       bool p_inclusive)
{
    qDebug() << "find target" << p_target << p_forward << p_inclusive;
    QTextBlock block = p_cursor.block();
    QString text = block.text();
    int pib = p_cursor.positionInBlock();
    int delta = p_forward ? 1 : -1;
    int idx = pib + delta;

repeat:
    for (; idx < text.size() && idx >= 0; idx += delta) {
        if (text[idx] == p_target) {
            break;
        }
    }

    if (idx < 0 || idx >= text.size()) {
        return false;
    }

    if ((p_forward && p_inclusive && p_mode == QTextCursor::KeepAnchor)
        || (!p_forward && !p_inclusive)) {
        ++idx;
    } else if (p_forward && !p_inclusive && p_mode == QTextCursor::MoveAnchor) {
        --idx;
    }

    if (idx == pib) {
        // We need to skip current match.
        idx = pib + delta * 2;
        goto repeat;
    }

    p_cursor.setPosition(block.position() + idx, p_mode);
    return true;
}

int VEditUtils::selectedBlockCount(const QTextCursor &p_cursor)
{
    if (!p_cursor.hasSelection()) {
        return 0;
    }

    QTextDocument *doc = p_cursor.document();
    int sbNum = doc->findBlock(p_cursor.selectionStart()).blockNumber();
    int ebNum = doc->findBlock(p_cursor.selectionEnd()).blockNumber();

    return ebNum - sbNum + 1;
}

void VEditUtils::scrollBlockInPage(QTextEdit *p_edit,
                                   int p_blockNum,
                                   int p_dest)
{
    QTextDocument *doc = p_edit->document();
    QTextCursor cursor = p_edit->textCursor();
    if (p_blockNum >= doc->blockCount()) {
        p_blockNum = doc->blockCount() - 1;
    }

    QTextBlock block = doc->findBlockByNumber(p_blockNum);

    int pib = cursor.positionInBlock();
    if (cursor.block().blockNumber() != p_blockNum) {
        // Move the cursor to the block.
        if (pib >= block.length()) {
            pib = block.length() - 1;
        }

        cursor.setPosition(block.position() + pib);
        p_edit->setTextCursor(cursor);
    }

    // Scroll to let current cursor locate in proper position.
    p_edit->ensureCursorVisible();
    QScrollBar *vsbar = p_edit->verticalScrollBar();

    if (!vsbar || !vsbar->isVisible()) {
        // No vertical scrollbar. No need to scrool.
        return;
    }

    QRect rect = p_edit->cursorRect();
    int height = p_edit->rect().height();
    QScrollBar *sbar = p_edit->horizontalScrollBar();
    if (sbar && sbar->isVisible()) {
        height -= sbar->height();
    }

    switch (p_dest) {
    case 0:
    {
        // Top.
        while (rect.y() > 0 && vsbar->value() < vsbar->maximum()) {
            vsbar->setValue(vsbar->value() + vsbar->singleStep());
            rect = p_edit->cursorRect();
        }

        break;
    }

    case 1:
    {
        // Center.
        height = qMax(height / 2, 1);
        if (rect.y() > height) {
            while (rect.y() > height && vsbar->value() < vsbar->maximum()) {
                vsbar->setValue(vsbar->value() + vsbar->singleStep());
                rect = p_edit->cursorRect();
            }
        } else if (rect.y() < height) {
            while (rect.y() < height && vsbar->value() > vsbar->minimum()) {
                vsbar->setValue(vsbar->value() - vsbar->singleStep());
                rect = p_edit->cursorRect();
            }
        }

        break;
    }

    case 2:
        // Bottom.
        while (rect.y() < height && vsbar->value() > vsbar->minimum()) {
            vsbar->setValue(vsbar->value() - vsbar->singleStep());
            rect = p_edit->cursorRect();
        }

        break;

    default:
        break;
    }

    p_edit->ensureCursorVisible();
}
