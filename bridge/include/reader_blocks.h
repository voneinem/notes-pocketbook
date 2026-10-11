#pragma once
// Reader block model: the parsed Markdown of a note as a list of QVariantMaps for the QML reader
// (contracts/qml-bridge.md). Pure conversion; the store is only read, to learn the state of attachments.

#include <QString>
#include <QVariantList>

#include "pbnotes/note_store.h"
#include "pbnotes/types.h"

// With a store the attachment blocks know their state (placeholder, ready, pending, error), size and image
// source; without one every attachment is a plain placeholder.
QVariantList reader_blocks(const pbnotes::Note& note, const pbnotes::NoteStore* store = nullptr);

// "0 B", "1023 B", "1.5 KB", "1.0 MB"; an empty string for a negative (unknown) size.
QString format_size(qlonglong bytes);

// One plain sentence for an attachment failure key (types.h is_attachment_error_key); never a name, path or
// address. Empty for "" and unknown keys.
QString attachment_error_text(const std::string& key);
