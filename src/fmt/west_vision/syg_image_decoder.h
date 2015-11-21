#pragma once

#include "fmt/image_decoder.h"

namespace au {
namespace fmt {
namespace west_vision {

    class SygImageDecoder final : public ImageDecoder
    {
    protected:
        bool is_recognized_impl(io::File &input_file) const override;
        pix::Grid decode_impl(io::File &input_file) const override;
    };

} } }
