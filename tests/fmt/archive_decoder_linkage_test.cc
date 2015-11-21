#include "test_support/catch.hh"
#include "fmt/archive_decoder.h"
#include "fmt/registry.h"

using namespace au::fmt;

TEST_CASE("Archives reference only valid decoders", "[fmt_core]")
{
    const auto &registry = Registry::instance();
    for (const auto &format_name : registry.get_decoder_names())
    {
        const auto decoder = registry.create_decoder(format_name);
        const auto arc_decoder
            = dynamic_cast<const ArchiveDecoder*>(decoder.get());
        if (!arc_decoder)
            continue;
        for (const auto &linked_format_name : arc_decoder->get_linked_formats())
        {
            INFO(format_name
                << " links to invalid format "
                << linked_format_name);
            REQUIRE(registry.has_decoder(linked_format_name));
        }
    }
}
