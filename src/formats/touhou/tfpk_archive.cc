// TFPK archive
//
// Company:   Team Shanghai Alice
// Engine:    -
// Extension: .pak
//
// Known games:
// - Touhou 13.5 - Hopeless Masquerade

#include <boost/filesystem.hpp>
#include <iomanip>
#include <map>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <sstream>
#include <vector>
#include "buffered_io.h"
#include "formats/touhou/tfpk_archive.h"
#include "formats/touhou/tfpk_dir_names.h"
#include "formats/touhou/tfbm_converter.h"
#include "util/colors.h"
#include "util/encoding.h"
#include "util/zlib.h"
using namespace Formats::Touhou;

namespace
{
    const std::vector<unsigned char> modulus({
            0xC7, 0x9A, 0x9E, 0x9B, 0xFB, 0xC2, 0x0C, 0xB0,
            0xC3, 0xE7, 0xAE, 0x27, 0x49, 0x67, 0x62, 0x8A,
            0x78, 0xBB, 0xD1, 0x2C, 0xB2, 0x4D, 0xF4, 0x87,
            0xC7, 0x09, 0x35, 0xF7, 0x01, 0xF8, 0x2E, 0xE5,
            0x49, 0x3B, 0x83, 0x6B, 0x84, 0x26, 0xAA, 0x42,
            0x9A, 0xE1, 0xCC, 0xEE, 0x08, 0xA2, 0x15, 0x1C,
            0x42, 0xE7, 0x48, 0xB1, 0x9C, 0xCE, 0x7A, 0xD9,
            0x40, 0x1A, 0x4D, 0xD4, 0x36, 0x37, 0x5C, 0x89
        });
    const unsigned int exponent = 65537;

    class RsaReader
    {
    public:
        RsaReader(IO &io);
        ~RsaReader();
        std::unique_ptr<BufferedIO> read_block();
        size_t tell() const;
    private:
        std::unique_ptr<BufferedIO> decrypt(
            std::basic_string<unsigned char> input);
        RSA *create_public_key();
        RSA *public_key;
        IO &io;
    };

    RsaReader::RsaReader(IO &io) : io(io)
    {
        public_key = create_public_key();
    }

    RsaReader::~RsaReader()
    {
        if (public_key != nullptr)
            RSA_free(public_key);
    }

    size_t RsaReader::tell() const
    {
        return io.tell();
    }

    RSA *RsaReader::create_public_key()
    {
        BIGNUM *bn_modulus = BN_new();
        BIGNUM *bn_exponent = BN_new();
        BN_set_word(bn_exponent, exponent);
        BN_bin2bn(modulus.data(), modulus.size(), bn_modulus);

        RSA *rsa = RSA_new();
        rsa->e = bn_exponent;
        rsa->n = bn_modulus;
        return rsa;
    }

    std::unique_ptr<BufferedIO> RsaReader::decrypt(
        std::basic_string<unsigned char> input)
    {
        size_t output_size = RSA_size(public_key);
        std::unique_ptr<unsigned char[]> output(new unsigned char[output_size]);
        int result = RSA_public_decrypt(
            input.size(),
            input.data(),
            output.get(),
            public_key,
            RSA_PKCS1_PADDING);

        if (result == -1)
        {
            std::unique_ptr<char[]> err(new char[130]);
            ERR_load_crypto_strings();
            ERR_error_string(ERR_get_error(), err.get());
            throw std::runtime_error(std::string(err.get()));
        }

        return std::unique_ptr<BufferedIO>(
            new BufferedIO(
                reinterpret_cast<const char*>(output.get()), output_size));
    }

    std::unique_ptr<BufferedIO> RsaReader::read_block()
    {
        auto s = io.read(0x40);
        auto su = std::basic_string<unsigned char>(s.begin(), s.end());
        auto block_io = decrypt(su);
        block_io->truncate(0x20);
        return block_io;
    }
}

namespace
{
    const std::string magic("TFPK\x00", 5);

    typedef struct
    {
        size_t size;
        size_t offset;
        std::string name;
        std::string key;
    } TableEntry;

    typedef std::vector<std::unique_ptr<TableEntry>> Table;

    typedef struct
    {
        uint32_t initial_hash;
        size_t file_count;
    } DirEntry;

    std::string lower_ascii_only(std::string name_sjis)
    {
        //while SJIS can use ASCII for encoding multibyte characters.
        //UTF-8 uses the codes 0–127 only for the ASCII characters,
        std::string name_utf8 = convert_encoding(name_sjis, "cp932", "utf-8");
        for (size_t i = 0; i < name_utf8.size(); i ++)
            if (name_utf8[i] >= 'A' && name_utf8[i] <= 'Z')
                name_utf8[i] += 'a' - 'A';
        return convert_encoding(name_utf8, "utf-8", "cp932");
    }

    std::string replace_slash_with_backslash(std::string s)
    {
        std::string s_copy = s;
        std::replace(s_copy.begin(), s_copy.end(), '/', '\\');
        return s_copy;
    }

    uint32_t get_file_name_hash(
        const std::string name,
        uint32_t initial_hash = 0x811C9DC5)
    {
        std::string name_processed
            = replace_slash_with_backslash(
                lower_ascii_only(
                    convert_encoding(name, "utf-8", "cp932")));

        uint32_t result = initial_hash;
        for (size_t i = 0; i < name_processed.size(); i ++)
            result = name_processed[i] ^ 0x1000193 * result;
        return result;
    }

    std::string get_dir_name(DirEntry &dir_entry)
    {
        static std::map<uint32_t, std::string> hash_to_dir_name_map;
        if (hash_to_dir_name_map.size() == 0)
        {
            for (auto &dir_name : tfpk_known_dir_names)
                hash_to_dir_name_map[get_file_name_hash(dir_name)] = dir_name;
        }
        auto it = hash_to_dir_name_map.find(dir_entry.initial_hash);
        if (it != hash_to_dir_name_map.end())
            return it->second;
        std::stringstream stream;
        stream
            << std::setfill('0')
            << std::setw(8)
            << std::hex
            << dir_entry.initial_hash;
        return stream.str();
    }

    std::vector<std::unique_ptr<DirEntry>> read_dir_entries(
        RsaReader &rsa_reader)
    {
        std::unique_ptr<BufferedIO> tmp_io(rsa_reader.read_block());
        std::vector<std::unique_ptr<DirEntry>> dir_entries;
        size_t dir_count = tmp_io->read_u32_le();
        for (size_t i = 0; i < dir_count; i ++)
        {
            tmp_io = rsa_reader.read_block();
            std::unique_ptr<DirEntry> entry(new DirEntry);
            entry->initial_hash = tmp_io->read_u32_le();
            entry->file_count = tmp_io->read_u32_le();
            dir_entries.push_back(std::move(entry));
        }
        return dir_entries;
    }

    std::map<uint32_t, std::string> read_file_names_map(RsaReader &rsa_reader)
    {
        auto dir_entries = read_dir_entries(rsa_reader);

        std::unique_ptr<BufferedIO> tmp_io(rsa_reader.read_block());
        size_t table_size_compressed = tmp_io->read_u32_le();
        size_t table_size_original = tmp_io->read_u32_le();
        size_t block_count = tmp_io->read_u32_le();

        tmp_io.reset(new BufferedIO);
        for (size_t i = 0; i < block_count; i ++)
            tmp_io->write_from_io(*rsa_reader.read_block());
        if (tmp_io->size() < table_size_compressed)
            throw std::runtime_error("Invalid file table size");

        tmp_io->seek(0);
        tmp_io.reset(
            new BufferedIO(zlib_inflate(tmp_io->read(table_size_compressed))));

        std::map<uint32_t, std::string> map;
        for (auto &dir_entry : dir_entries)
        {
            std::string dir_name = get_dir_name(*dir_entry);
            if (dir_name.size() > 0 && dir_name[dir_name.size() - 1] != '/')
                dir_name += "/";

            for (size_t j = 0; j < dir_entry->file_count; j ++)
            {
                std::string name = convert_encoding(
                    tmp_io->read_until_zero(), "cp932", "utf-8");

                uint32_t hash = get_file_name_hash(
                    name, dir_entry->initial_hash);

                map[hash] = dir_name + name;
            }
        }
        return map;
    }

    Table read_table(RsaReader &rsa_reader)
    {
        auto hash_to_file_name_map = read_file_names_map(rsa_reader);

        std::unique_ptr<BufferedIO> tmp_io(rsa_reader.read_block());
        size_t file_count = tmp_io->read_u32_le();

        Table table;
        for (size_t i = 0; i < file_count; i ++)
        {
            std::unique_ptr<TableEntry> entry(new TableEntry);

            tmp_io = rsa_reader.read_block();
            entry->size = tmp_io->read_u32_le();
            entry->offset = tmp_io->read_u32_le();

            tmp_io = rsa_reader.read_block();
            uint32_t file_name_hash = tmp_io->read_u32_le();
            auto it = hash_to_file_name_map.find(file_name_hash);
            if (it == hash_to_file_name_map.end())
                throw std::runtime_error("Could not find file name hash");
            entry->name = it->second;

            entry->key = rsa_reader.read_block()->read(16);
            table.push_back(std::move(entry));
        }

        auto file_names_start = rsa_reader.tell();
        for (auto &entry : table)
            entry->offset += file_names_start;

        return table;
    }

    std::unique_ptr<File> read_file(IO &arc_io, const TableEntry &entry)
    {
        std::unique_ptr<File> file(new File);
        file->name = entry.name;

        arc_io.seek(entry.offset);
        BufferedIO tmp_io(arc_io, entry.size);
        size_t key_size = entry.key.size();
        for (size_t i = 0; i < entry.size; i ++)
            tmp_io.buffer()[i] ^= entry.key[i % key_size];
        file->io.write_from_io(tmp_io);

        return file;
    }

    TfbmPalette read_palette_file(IO &arc_io, TableEntry &entry)
    {
        const std::string pal_magic("TFPA\x00", 5);
        auto pal_file = read_file(arc_io, entry);
        pal_file->io.seek(0);
        if (pal_file->io.read(pal_magic.size()) != pal_magic)
            throw std::runtime_error("Not a TFPA palette file");
        size_t pal_size = pal_file->io.read_u32_le();
        BufferedIO pal_io(zlib_inflate(pal_file->io.read(pal_size)));
        TfbmPalette palette;
        for (size_t i = 0; i < 256; i ++)
            palette[i] = rgba5551(pal_io.read_u16_le());
        return palette;
    }

    TfbmPaletteMap find_all_palettes(const boost::filesystem::path &arc_path)
    {
        TfbmPaletteMap pals;

        auto dir = boost::filesystem::path(arc_path).parent_path();
        for (boost::filesystem::directory_iterator it(dir);
            it != boost::filesystem::directory_iterator();
            it ++)
        {
            if (!boost::filesystem::is_regular_file(it->path()))
                continue;
            if (it->path().string().find(".pak") == std::string::npos)
                continue;

            try
            {
                FileIO sub_arc_io(it->path(), FileIOMode::Read);
                if (sub_arc_io.read(magic.size()) != magic)
                    continue;
                RsaReader rsa_reader(sub_arc_io);
                for (auto &entry : read_table(rsa_reader))
                {
                    if (entry->name.find("palette") == std::string::npos)
                        continue;
                    pals[entry->name] = read_palette_file(sub_arc_io, *entry);
                }
            }
            catch (...)
            {
                continue;
            }
        }

        return pals;
    }
}

struct TfpkArchive::Internals
{
    std::unique_ptr<TfbmConverter> tfbm_converter;

    Internals() : tfbm_converter(new TfbmConverter)
    {
    }

    ~Internals()
    {
    }
};

TfpkArchive::TfpkArchive() : internals(new Internals)
{
}

TfpkArchive::~TfpkArchive()
{
}

void TfpkArchive::unpack_internal(File &arc_file, FileSaver &file_saver) const
{
    if (arc_file.io.read(magic.size()) != magic)
        throw std::runtime_error("Not a TFPK archive");

    TfbmPaletteMap palette_map = find_all_palettes(arc_file.name);
    internals->tfbm_converter->set_palette_map(palette_map);

    RsaReader rsa_reader(arc_file.io);
    Table table = read_table(rsa_reader);

    for (auto &entry : table)
    {
        auto file = read_file(arc_file.io, *entry);
        internals->tfbm_converter->try_decode(*file);
        run_converters(*file);
        file_saver.save(std::move(file));
    }
}
