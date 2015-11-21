#pragma once

#include "fmt/archive_decoder.h"

namespace au {
namespace fmt {
namespace nekopack {

    class Nekopack4ArchiveDecoder final : public ArchiveDecoder
    {
    protected:
        bool is_recognized_impl(io::File &input_file) const override;

        std::unique_ptr<ArchiveMeta> read_meta_impl(
            io::File &input_file) const override;

        std::unique_ptr<io::File> read_file_impl(
            io::File &input_file,
            const ArchiveMeta &m,
            const ArchiveEntry &e) const override;

        void preprocess(
            io::File &input_file,
            ArchiveMeta &m,
            const FileSaver &file_saver) const override;
    };

} } }
