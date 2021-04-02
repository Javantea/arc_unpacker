// Copyright (C) 2016 by rr-
//
// This file is part of arc_unpacker.
//
// arc_unpacker is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at
// your option) any later version.
//
// arc_unpacker is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with arc_unpacker. If not, see <http://www.gnu.org/licenses/>.

#include "dec/mebius/koe_audio_decoder.h"
#include "algo/binary.h"
#include "dec/microsoft/wav_audio_decoder.h"

using namespace au;
using namespace au::dec::mebius;

static const bstr magic = "RIFF"_b;

bool KoeAudioDecoder::is_recognized_impl(io::File &input_file) const
{
    return input_file.stream.read(magic.size()) == magic
        && (input_file.path.has_extension("bgm")
        || input_file.path.has_extension("mse")
        || input_file.path.has_extension("koe"));
}

res::Audio KoeAudioDecoder::decode_impl(
    const Logger &logger, io::File &input_file) const
{
    auto audio = dec::microsoft::WavAudioDecoder().decode(logger, input_file);

    const auto plugin = plugin_manager.get();
    if (input_file.path.has_extension("bgm"))
        audio.samples = algo::unxor(audio.samples, plugin.bgm_key);
    else if (input_file.path.has_extension("koe"))
        audio.samples = algo::unxor(audio.samples, plugin.koe_key);
    else if (input_file.path.has_extension("mse"))
        audio.samples = algo::unxor(audio.samples, plugin.mse_key);

    return audio;
}

static auto _ = dec::register_decoder<KoeAudioDecoder>("mebius/koe");
