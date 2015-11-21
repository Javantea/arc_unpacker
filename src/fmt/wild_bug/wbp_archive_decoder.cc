#include "fmt/wild_bug/wbp_archive_decoder.h"
#include <map>
#include "util/range.h"

using namespace au;
using namespace au::fmt::wild_bug;

static const bstr magic = "ARCFORM4\x20WBUG\x20"_b;

namespace
{
    struct ArchiveEntryImpl final : fmt::ArchiveEntry
    {
        size_t offset;
        size_t size;
    };
}

bool WbpArchiveDecoder::is_recognized_impl(io::File &input_file) const
{
    return input_file.stream.read(magic.size()) == magic;
}

std::unique_ptr<fmt::ArchiveMeta>
    WbpArchiveDecoder::read_meta_impl(io::File &input_file) const
{
    input_file.stream.seek(0x10);
    auto file_count = input_file.stream.read_u32_le();
    auto table_offset = input_file.stream.read_u32_le();
    auto table_size = input_file.stream.read_u32_le();
    input_file.stream.skip(8);

    std::vector<size_t> dir_offsets;
    for (auto i : util::range(0x100))
    {
        auto offset = input_file.stream.read_u32_le();
        if (offset)
            dir_offsets.push_back(offset);
    }

    std::vector<size_t> file_offsets;
    for (auto i : util::range(0x100))
    {
        auto offset = input_file.stream.read_u32_le();
        if (offset)
            file_offsets.push_back(offset);
    }

    std::map<u16, std::string> dir_names;
    for (auto &offset : dir_offsets)
    {
        input_file.stream.seek(offset + 1);
        auto name_size = input_file.stream.read_u8();
        auto dir_id = input_file.stream.read_u8();
        input_file.stream.skip(1);
        dir_names[dir_id] = input_file.stream.read_to_zero(name_size).str();
    }

    auto meta = std::make_unique<ArchiveMeta>();
    for (size_t i : util::range(file_offsets.size()))
    {
        // one file offset may contain multiple entries
        input_file.stream.seek(file_offsets[i]);
        do
        {
            auto old_pos = input_file.stream.tell();

            input_file.stream.skip(1);
            auto name_size = input_file.stream.read_u8();
            auto dir_id = input_file.stream.read_u8();
            input_file.stream.skip(1);

            auto entry = std::make_unique<ArchiveEntryImpl>();
            entry->offset = input_file.stream.read_u32_le();
            entry->size = input_file.stream.read_u32_le();
            input_file.stream.skip(8);
            entry->name = dir_names.at(dir_id)
                + input_file.stream.read_to_zero(name_size).str();

            meta->entries.push_back(std::move(entry));

            input_file.stream.seek(old_pos + (name_size & 0xFC) + 0x18);
        }
        while (i + 1 < file_offsets.size()
            && input_file.stream.tell() < file_offsets[i + 1]);
    }
    return meta;
}

std::unique_ptr<io::File> WbpArchiveDecoder::read_file_impl(
    io::File &input_file, const ArchiveMeta &m, const ArchiveEntry &e) const
{
    auto entry = static_cast<const ArchiveEntryImpl*>(&e);
    input_file.stream.seek(entry->offset);
    auto data = input_file.stream.read(entry->size);
    return std::make_unique<io::File>(entry->name, data);
}

std::vector<std::string> WbpArchiveDecoder::get_linked_formats() const
{
    return {"wild-bug/wbi", "wild-bug/wbm", "wild-bug/wpn", "wild-bug/wwa"};
}

static auto dummy = fmt::register_fmt<WbpArchiveDecoder>("wild-bug/wbp");
