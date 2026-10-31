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
