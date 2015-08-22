#include "fmt/touhou/pak2_sound_converter.h"
#include "test_support/catch.hh"
#include "test_support/file_support.h"

using namespace au;
using namespace au::fmt::touhou;

TEST_CASE("Decoding CV3 sound files works")
{
    Pak2SoundConverter converter;
    auto input_file = tests::file_from_path(
        "tests/fmt/touhou/files/pak2/049.cv3");
    auto expected_file = tests::file_from_path(
        "tests/fmt/touhou/files/pak2/049-out.wav");
    auto actual_file = converter.decode(*input_file);
    tests::compare_files(*expected_file, *actual_file, false);
}