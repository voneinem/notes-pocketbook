// US5 (spec 002 FR-025, FR-029, research R-3, R-5): AttachmentService::add copies a chosen file into the app's
// attachment folder as a staged attachment. The user's original is only ever read.
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include "pbnotes/attachment_ref.h"
#include "pbnotes/attachment_service.h"
#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "test_support.h"
#include "tmp_dir.h"

using namespace pbnotes;

namespace {

// The service never sends anything for `add`; a transport that fails every request proves it.
class NoNetwork : public HttpTransport {
public:
    Response perform(const Request&) override {
        ++requests;
        Response r;
        r.error = ErrorClass::Network;
        return r;
    }
    int requests = 0;
};

struct Env {
    pbtest::TmpDir dir;
    std::string attachments;
    std::string source_dir;
    std::unique_ptr<NoteStore> store;
    NoNetwork transport;
    std::unique_ptr<AttachmentService> service;
    Env() {
        attachments = dir.file("attachments");
        source_dir = dir.file("source");
        ::mkdir(source_dir.c_str(), 0755);
        store = NoteStore::open(dir.file("db"));
        service.reset(new AttachmentService(*store, transport, attachments));
    }
    std::string make(const std::string& name, const std::string& content) {
        const std::string path = source_dir + "/" + name;
        std::ofstream f(path, std::ios::binary);
        f << content;
        return path;
    }
    std::vector<std::string> files() const {
        std::vector<std::string> out;
        if (DIR* d = opendir(attachments.c_str())) {
            while (dirent* e = readdir(d)) {
                const std::string n = e->d_name;
                if (n != "." && n != "..") out.push_back(n);
            }
            closedir(d);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    std::string read(const std::string& path) const {
        std::ifstream f(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
};

time_t mtime(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 ? st.st_mtime : 0;
}

}  // namespace

PB_TEST(adding_a_file_copies_it_and_stages_a_row) {
    Env e;
    const std::string src = e.make("photo.PNG", "PNGDATA-123");
    auto r = e.service->add(src);
    PB_CHECK(r.ok);
    PB_CHECK_EQ(r.error_key, std::string());
    PB_CHECK_EQ(r.display_name, std::string("photo.PNG"));
    PB_CHECK_EQ(r.size, static_cast<int64_t>(11));
    PB_CHECK_EQ(r.token, make_token(r.id, "photo.PNG"));
    auto row = e.store->get_attachment(r.id);
    PB_CHECK(row.has_value());
    PB_CHECK(row->state == AttachmentState::Staged);
    PB_CHECK(!row->note_local_id.has_value());
    PB_CHECK(row->kind == AttachmentKind::Image);
    PB_CHECK_EQ(row->size.value_or(-1), static_cast<int64_t>(11));
    PB_CHECK_EQ(row->local_file, std::to_string(r.id) + ".png");
    PB_CHECK((e.files() == std::vector<std::string>{row->local_file}));   // no .part left
    PB_CHECK_EQ(e.read(e.attachments + "/" + row->local_file), std::string("PNGDATA-123"));
    PB_CHECK_EQ(e.transport.requests, 0);
}

PB_TEST(the_original_file_is_never_modified) {
    Env e;
    const std::string src = e.make("doc.pdf", "ORIGINAL");
    const time_t before = mtime(src);
    PB_CHECK(e.service->add(src).ok);
    PB_CHECK_EQ(e.read(src), std::string("ORIGINAL"));
    PB_CHECK_EQ(mtime(src), before);
    PB_CHECK(::access(src.c_str(), F_OK) == 0);   // copied, not moved
}

PB_TEST(a_file_over_ten_mebibytes_is_refused_and_nothing_is_stored) {
    Env e;
    const std::string src = e.source_dir + "/big.bin";
    {
        std::ofstream f(src, std::ios::binary);
        f.seekp(kMaxAttachmentBytes);   // a sparse file of exactly 10 MiB + 1 byte
        f.put('x');
    }
    auto r = e.service->add(src);
    PB_CHECK(!r.ok);
    PB_CHECK_EQ(r.error_key, std::string("too_large"));
    PB_CHECK(e.files().empty());
    PB_CHECK(!e.store->get_attachment(1).has_value());
}

PB_TEST(a_file_of_exactly_ten_mebibytes_is_accepted) {
    Env e;
    const std::string src = e.source_dir + "/exact.bin";
    {
        std::ofstream f(src, std::ios::binary);
        f.seekp(kMaxAttachmentBytes - 1);
        f.put('x');
    }
    auto r = e.service->add(src);
    PB_CHECK(r.ok);
    PB_CHECK_EQ(r.size, kMaxAttachmentBytes);
}

PB_TEST(a_missing_file_a_directory_or_an_unreadable_file_is_unreadable) {
    Env e;
    PB_CHECK_EQ(e.service->add(e.source_dir + "/nope.png").error_key, std::string("unreadable"));
    PB_CHECK_EQ(e.service->add(e.source_dir).error_key, std::string("unreadable"));
    PB_CHECK_EQ(e.service->add("").error_key, std::string("unreadable"));
    const std::string secret = e.make("secret.png", "x");
    ::chmod(secret.c_str(), 0);
    if (::access(secret.c_str(), R_OK) != 0) PB_CHECK_EQ(e.service->add(secret).error_key, std::string("unreadable"));
    ::chmod(secret.c_str(), 0644);
    PB_CHECK(e.files().empty());
    PB_CHECK(!e.store->get_attachment(1).has_value());
}

PB_TEST(adding_the_same_file_twice_gives_two_distinct_rows_and_files) {
    Env e;
    const std::string src = e.make("a.png", "SAME");
    auto r1 = e.service->add(src);
    auto r2 = e.service->add(src);
    PB_CHECK(r1.ok && r2.ok);
    PB_CHECK(r1.id != r2.id);
    PB_CHECK(e.store->get_attachment(r1.id)->local_file != e.store->get_attachment(r2.id)->local_file);
    PB_CHECK_EQ(e.files().size(), static_cast<size_t>(2));
}

PB_TEST(files_with_the_same_name_from_different_folders_are_distinct) {
    Env e;
    ::mkdir((e.source_dir + "/other").c_str(), 0755);
    const std::string a = e.make("photo.jpg", "AAA");
    std::ofstream(e.source_dir + "/other/photo.jpg", std::ios::binary) << "BBBB";
    auto r1 = e.service->add(a);
    auto r2 = e.service->add(e.source_dir + "/other/photo.jpg");
    PB_CHECK(r1.ok && r2.ok);
    PB_CHECK_EQ(e.read(e.attachments + "/" + e.store->get_attachment(r1.id)->local_file), std::string("AAA"));
    PB_CHECK_EQ(e.read(e.attachments + "/" + e.store->get_attachment(r2.id)->local_file), std::string("BBBB"));
}

PB_TEST(a_full_storage_stops_cleanly_without_a_part_file_or_a_row) {
    Env e;
    const std::string src = e.make("a.png", std::string(1000, 'x'));
    e.service->test_fail_copy_after(100);
    auto r = e.service->add(src);
    PB_CHECK(!r.ok);
    PB_CHECK_EQ(r.error_key, std::string("storage_full"));
    PB_CHECK(e.files().empty());
    PB_CHECK(!e.store->get_attachment(1).has_value());
    PB_CHECK_EQ(e.read(src).size(), static_cast<size_t>(1000));
    e.service->test_fail_copy_after(-1);
    PB_CHECK(e.service->add(src).ok);   // and it works again afterwards
}

PB_TEST(a_hostile_file_name_is_sanitized_and_never_used_as_a_path) {
    Env e;
    const std::string weird = std::string("a\x01") + "b\x7f" + std::string(240, 'x') + ".png";   // a file system limits names to 255 bytes
    auto r = e.service->add(e.make(weird, "DATA"));
    PB_CHECK(r.ok);
    for (unsigned char c : r.display_name) PB_CHECK(c >= 0x20 && c != 0x7f);
    PB_CHECK(r.display_name.size() <= 255);
    auto row = e.store->get_attachment(r.id);
    PB_CHECK_EQ(row->local_file, std::to_string(r.id) + ".png");
    PB_CHECK((e.files() == std::vector<std::string>{row->local_file}));

    auto bare = e.service->add(e.make("README", "text"));   // no extension at all
    PB_CHECK(bare.ok);
    PB_CHECK_EQ(e.store->get_attachment(bare.id)->local_file, std::to_string(bare.id));
    auto odd = e.service->add(e.make("x.p ng", "text"));    // an extension that is not safe
    PB_CHECK_EQ(e.store->get_attachment(odd.id)->local_file, std::to_string(odd.id));
}

PB_TEST(the_token_of_an_added_file_survives_in_note_text_and_binds_on_save) {
    Env e;
    auto r = e.service->add(e.make("a.png", "DATA"));
    auto note = e.store->create_local(pbnotes::derive_title("Title\n" + r.token + "\n"), "Title\n" + r.token + "\n", "", false, 100);
    PB_CHECK(note.error == EditError::None);
    PB_CHECK(e.store->get_attachment(r.id)->state == AttachmentState::Pending);
}

PB_TEST_MAIN()
