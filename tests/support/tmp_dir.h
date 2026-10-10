#pragma once
// Temporary directory for tests, removed on destruction.

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace pbtest {

class TmpDir {
public:
    TmpDir() {
        char tmpl[] = "/tmp/pbnotes-test-XXXXXX";
        if (!mkdtemp(tmpl)) throw std::runtime_error("mkdtemp failed");
        path_ = tmpl;
    }
    ~TmpDir() { remove_tree(path_); }
    TmpDir(const TmpDir&) = delete;
    TmpDir& operator=(const TmpDir&) = delete;

    const std::string& path() const { return path_; }
    std::string file(const std::string& name) const { return path_ + "/" + name; }

    void write(const std::string& name, const std::string& data) const {
        std::ofstream f(file(name), std::ios::binary);
        f << data;
    }
    std::string read(const std::string& name) const {
        std::ifstream f(file(name), std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
    bool exists(const std::string& name) const {
        struct stat st;
        return ::stat(file(name).c_str(), &st) == 0;
    }

private:
    static void remove_tree(const std::string& dir) {
        if (DIR* d = opendir(dir.c_str())) {
            while (dirent* e = readdir(d)) {
                std::string n = e->d_name;
                if (n == "." || n == "..") continue;
                std::string p = dir + "/" + n;
                struct stat st;
                if (::lstat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) remove_tree(p);
                else ::unlink(p.c_str());
            }
            closedir(d);
        }
        ::rmdir(dir.c_str());
    }
    std::string path_;
};

}  // namespace pbtest
