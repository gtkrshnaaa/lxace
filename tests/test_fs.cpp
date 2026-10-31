#include "test_runner.hpp"
#include "sys/fs.hpp"

using namespace lxace::sys;

TEST_CASE(fs_file_write_and_read) {
    std::filesystem::path test_file = "/tmp/lxace_test_fs.txt";
    std::string content = "LXACE Native Filesystem Test Content\nLine 2";

    bool written = FsEngine::write_file(test_file, content);
    ASSERT_TRUE(written);
    ASSERT_TRUE(FsEngine::exists(test_file));

    auto read_back = FsEngine::read_file(test_file);
    ASSERT_TRUE(read_back.has_value());
    ASSERT_EQ(*read_back, content);

    bool removed = FsEngine::remove_path(test_file);
    ASSERT_TRUE(removed);
    ASSERT_FALSE(FsEngine::exists(test_file));
}

TEST_CASE(fs_directory_listing) {
    std::filesystem::path test_dir = "/tmp/lxace_test_dir";
    FsEngine::create_directories(test_dir);
    FsEngine::write_file(test_dir / "file1.txt", "one");
    FsEngine::write_file(test_dir / "file2.txt", "two");

    auto entries = FsEngine::list_directory(test_dir, false);
    ASSERT_EQ(entries.size(), 2);

    FsEngine::remove_path(test_dir);
    ASSERT_FALSE(FsEngine::exists(test_dir));
}

TEST_CASE(fs_safe_path_traversal_protection) {
    std::filesystem::path project_root = "/tmp/lxace_project";
    FsEngine::create_directories(project_root / "subdir");

    // Allowed relative path
    auto safe1 = FsEngine::resolve_safe_path(project_root, "subdir/file.txt");
    ASSERT_TRUE(safe1.has_value());

    // Disallowed path traversal escaping project root
    auto unsafe1 = FsEngine::resolve_safe_path(project_root, "../../etc/passwd");
    ASSERT_FALSE(unsafe1.has_value());

    // Disallowed absolute path outside project root
    auto unsafe2 = FsEngine::resolve_safe_path(project_root, "/etc/shadow");
    ASSERT_FALSE(unsafe2.has_value());

    FsEngine::remove_path(project_root);
}
