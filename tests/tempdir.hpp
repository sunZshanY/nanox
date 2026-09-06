#pragma once

// Shared helper for tests that need a scratch directory on disk (file tree,
// project analysis). Each TempDir creates a unique directory under the system
// temp path and removes it recursively on destruction.

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace nanox_test {

struct TempDir {
    std::filesystem::path path;

    explicit TempDir(const std::string& tag) : path(make_unique(tag)) {}

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = default;
    TempDir& operator=(TempDir&&) = default;

    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    std::string str() const { return path.string(); }

    // Writes `content` to `<root>/<relative>`, creating parent directories.
    void write_file(const std::string& relative, const std::string& content) {
        const std::filesystem::path target = path / relative;
        std::filesystem::create_directories(target.parent_path());
        std::ofstream out(target, std::ios::binary);
        out << content;
    }

    // Creates an (empty) directory `<root>/<relative>`.
    void make_dir(const std::string& relative) {
        std::filesystem::create_directories(path / relative);
    }

private:
    static std::filesystem::path make_unique(const std::string& tag) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        std::filesystem::path p =
            std::filesystem::temp_directory_path() / (tag + "_" + std::to_string(stamp));
        std::filesystem::create_directories(p);
        return p;
    }
};

}  // namespace nanox_test
