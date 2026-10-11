#include "reader_blocks.h"

#include <QString>
#include <QVariantMap>

#include "pbnotes/attachment_ref.h"
#include "pbnotes/markdown.h"

namespace {

QString html_escape(const std::string& text) {
    return QString::fromStdString(text).toHtmlEscaped();
}

QString styled(const pbnotes::Span& s) {
    const QString t = html_escape(s.text);
    switch (s.kind) {
        case pbnotes::Span::Bold: return QStringLiteral("<b>") + t + QStringLiteral("</b>");
        case pbnotes::Span::Italic: return QStringLiteral("<i>") + t + QStringLiteral("</i>");
        case pbnotes::Span::BoldItalic: return QStringLiteral("<b><i>") + t + QStringLiteral("</i></b>");
        case pbnotes::Span::Plain: break;
    }
    return t;
}

// Styled text for the QML Text item. A run of spans with the same link target becomes one
// <a href="l<N>"><u>..</u></a>; N indexes `links` ({url, confirm}). The address itself never appears in
// the markup, so nothing in it can be mistaken for markup (spec 002 FR-001).
QString spans_to_html(const std::vector<pbnotes::Span>& spans, QVariantList* links) {
    QString out;
    size_t i = 0;
    while (i < spans.size()) {
        if (spans[i].url.empty()) {
            out += styled(spans[i++]);
            continue;
        }
        const size_t first = i;
        QString inner;
        while (i < spans.size() && spans[i].url == spans[first].url && spans[i].link_confirm == spans[first].link_confirm)
            inner += styled(spans[i++]);
        QVariantMap link;
        link.insert(QStringLiteral("url"), QString::fromStdString(spans[first].url));
        link.insert(QStringLiteral("confirm"), spans[first].link_confirm);
        out += QStringLiteral("<a href=\"l%1\"><u>").arg(links->size()) + inner + QStringLiteral("</u></a>");
        links->append(link);
    }
    return out;
}

const char* block_kind(pbnotes::BlockKind k) {
    switch (k) {
        case pbnotes::BlockKind::Heading: return "heading";
        case pbnotes::BlockKind::ListItem: return "listitem";
        case pbnotes::BlockKind::Blank: return "blank";
        case pbnotes::BlockKind::Table: return "tablerow";
        case pbnotes::BlockKind::Attachment: return "attachment";
        case pbnotes::BlockKind::Paragraph: break;
    }
    return "paragraph";
}

const char* align_name(pbnotes::TableData::Align a) {
    switch (a) {
        case pbnotes::TableData::Center: return "center";
        case pbnotes::TableData::Right: return "right";
        case pbnotes::TableData::Left:
        case pbnotes::TableData::None: break;
    }
    return "left";
}

// A table becomes one block per row, so a page break can never fall inside a row.
void append_table_rows(QVariantList* blocks, const pbnotes::Block& b) {
    const pbnotes::TableData& t = b.table;
    QVariantList weights;
    for (double w : t.weight) weights.append(w);
    for (size_t r = 0; r < t.rows.size(); ++r) {
        QVariantMap m;
        m.insert(QStringLiteral("kind"), QStringLiteral("tablerow"));
        m.insert(QStringLiteral("line"), b.line_index + (r == 0 ? 0 : static_cast<int>(r) + 1));
        m.insert(QStringLiteral("unsplittable"), true);
        m.insert(QStringLiteral("header"), r == 0);
        m.insert(QStringLiteral("lastRow"), r + 1 == t.rows.size());
        m.insert(QStringLiteral("weights"), weights);
        QVariantList cols;
        for (size_t c = 0; c < t.rows[r].size(); ++c) {
            QVariantList links;
            QVariantMap col;
            col.insert(QStringLiteral("html"), spans_to_html(t.rows[r][c], &links));
            col.insert(QStringLiteral("links"), links);
            col.insert(QStringLiteral("align"), QString::fromLatin1(align_name(t.align[c])));
            cols.append(col);
        }
        m.insert(QStringLiteral("cols"), cols);
        blocks->append(m);
    }
}

}  // namespace

// One plain sentence per failure reason; never a name, a path or an address (spec FR-033).
QString attachment_error_text(const std::string& key) {
    if (key == "too_large") return QStringLiteral("This file is larger than the 10 MB limit.");
    if (key == "not_found") return QStringLiteral("The file was not found on the server.");
    if (key == "rejected") return QStringLiteral("The server did not accept the file.");
    if (key == "unsupported_server") return QStringLiteral("This server does not support attachments.");
    if (key == "storage_full") return QStringLiteral("There is not enough storage space on the reader.");
    if (key == "unreadable") return QStringLiteral("The file could not be read.");
    if (key == "format_unsupported") return QStringLiteral("This picture format cannot be shown.");
    if (key == "network") return QStringLiteral("The connection failed. Try again.");
    return QString();
}

namespace {

QString error_text(const std::string& key) { return attachment_error_text(key); }

// The attachment block of a reference: state and details from the store row, if there is one.
QVariantMap attachment_block(const pbnotes::Block& b, const pbnotes::Note& note, const pbnotes::NoteStore* store) {
    const pbnotes::AttachmentRef& ref = b.attachment;
    std::optional<pbnotes::Attachment> row;
    if (store) {
        if (ref.token_id > 0) {
            row = store->get_attachment(ref.token_id);
            // a token only means something in the note its row belongs to (a pasted copy stays plain text)
            if (row && (!row->note_local_id || *row->note_local_id != note.local_id)) row.reset();
        } else {
            row = store->attachment_for(note.local_id, ref.path);
        }
    }
    QVariantMap m;
    m.insert(QStringLiteral("kind"), QStringLiteral("attachment"));
    m.insert(QStringLiteral("line"), b.line_index);
    m.insert(QStringLiteral("unsplittable"), true);

    std::string name = ref.name;
    if (name.empty() && !ref.path.empty()) name = pbnotes::attachment_file_name(ref.path);
    if (name.empty() && row) name = row->display_name;
    const pbnotes::AttachmentKind kind = row ? row->kind : pbnotes::attachment_kind_for(ref.path);
    m.insert(QStringLiteral("name"), QString::fromStdString(name));
    m.insert(QStringLiteral("attKind"), QString::fromLatin1(pbnotes::to_string(kind)));
    m.insert(QStringLiteral("remotePath"), QString::fromStdString(ref.path));
    m.insert(QStringLiteral("attachmentId"), static_cast<qlonglong>(row ? row->id : 0));
    m.insert(QStringLiteral("size"), static_cast<qlonglong>(row && row->size ? *row->size : -1));

    QString state = QStringLiteral("placeholder"), image_source, error;
    bool viewable = false;
    if (ref.token_id > 0 && !row) {
        state = QStringLiteral("error");  // a local token whose file is gone
        error = QStringLiteral("This attachment is no longer on the reader.");
    } else if (row) {
        if (row->state == pbnotes::AttachmentState::Pending || row->state == pbnotes::AttachmentState::Staged) {
            state = QStringLiteral("pending");
            error = error_text(row->error_key);
            if (kind == pbnotes::AttachmentKind::Image) image_source = QStringLiteral("image://attachment/%1").arg(row->id);
        } else if (row->state == pbnotes::AttachmentState::Downloaded) {
            state = QStringLiteral("ready");
            if (kind == pbnotes::AttachmentKind::Image) image_source = QStringLiteral("image://attachment/%1").arg(row->id);
            else viewable = pbnotes::is_viewable_extension(pbnotes::safe_extension(row->local_file));
        } else if (!row->error_key.empty()) {
            state = QStringLiteral("error");
            error = error_text(row->error_key);
        }
    }
    m.insert(QStringLiteral("attState"), state);
    m.insert(QStringLiteral("imageSource"), image_source);
    m.insert(QStringLiteral("viewable"), viewable);
    m.insert(QStringLiteral("errorText"), error);
    return m;
}

}  // namespace

QString format_size(qlonglong bytes) {
    if (bytes < 0) return QString();
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

QVariantList reader_blocks(const pbnotes::Note& note, const pbnotes::NoteStore* store) {
    QVariantList blocks;
    for (const pbnotes::Block& b : pbnotes::parse_markdown(note.content)) {
        if (b.kind == pbnotes::BlockKind::Table) {
            append_table_rows(&blocks, b);
            continue;
        }
        if (b.kind == pbnotes::BlockKind::Attachment) {
            blocks.append(attachment_block(b, note, store));
            continue;
        }
        QVariantMap m;
        m.insert(QStringLiteral("kind"), QString::fromLatin1(block_kind(b.kind)));
        m.insert(QStringLiteral("line"), b.line_index);
        m.insert(QStringLiteral("level"), b.level);
        m.insert(QStringLiteral("ordered"), b.ordered);
        m.insert(QStringLiteral("number"), b.number);
        m.insert(QStringLiteral("depth"), b.depth);
        m.insert(QStringLiteral("task"), b.is_task);
        m.insert(QStringLiteral("checked"), b.checked);
        QVariantList links;
        m.insert(QStringLiteral("html"), spans_to_html(b.spans, &links));
        m.insert(QStringLiteral("links"), links);
        blocks.append(m);
    }
    return blocks;
}
