#include "formats/arc/rgssad_archive.h"
#include "test_support/archive_support.h"

void test_rgssad_archive()
{
    std::vector<File*> expected_files;
    std::unique_ptr<File> file1(new File);
    std::unique_ptr<File> file2(new File);
    file1->name = "abc.txt";
    file2->name = "another.txt";
    file1->io.write("123", 3);
    file2->io.write("abcdefghij", 10);
    expected_files.push_back(file1.get());
    expected_files.push_back(file2.get());

    std::string path = "tests/test_files/arc/rgssad/test.rgssad";
    std::unique_ptr<Archive> archive(new RgssadArchive);
    compare_files(expected_files, unpack_to_memory(path, *archive));
}

int main(void)
{
    test_rgssad_archive();
    return 0;
}
