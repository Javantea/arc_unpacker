#include "fmt/cronus/pak_archive_decoder.h"
#include "test_support/decoder_support.h"
#include "test_support/catch.hh"
#include "test_support/file_support.h"

using namespace au;
using namespace au::fmt::cronus;

static const std::string dir = "tests/fmt/cronus/files/pak/";

static void do_test(const std::string &input_path)
{
    const std::vector<std::shared_ptr<io::File>> expected_files
    {
        tests::stub_file("123.txt", "1234567890"_b),
        tests::stub_file("abc.txt", "abcdefghijklmnopqrstuvwxyz"_b),
    };
    const PakArchiveDecoder decoder;
    const auto input_file = tests::file_from_path(dir + input_path);
    const auto actual_files = tests::unpack(decoder, *input_file);
    tests::compare_files(expected_files, actual_files, true);
}

TEST_CASE("Cronus PAK archives", "[fmt]")
{
    SECTION("Plain")
    {
        do_test("unencrypted-uncompressed.pak");
    }

    SECTION("Encrypted")
    {
        do_test("encrypted-uncompressed.pak");
    }

    SECTION("Encrypted + compressed")
    {
        do_test("encrypted-compressed.pak");
    }
}
