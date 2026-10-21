#include "core/vision/png.h"

#include <array>
#include <cstring>

namespace st {

namespace {

std::uint32_t ReadU32(const unsigned char* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

// ------------------------------------------------------------------ inflate (RFC 1951)

class Inflator {
public:
    Inflator(const std::vector<unsigned char>& data, std::size_t offset) : d_(data), pos_(offset) {}

    bool Run(std::vector<unsigned char>& out) {
        unsigned char hlit = 0;
        unsigned char hdist = 0;
        unsigned long blockFinal = 0;
        while (true) {
            if (!ReadBits(1, blockFinal)) return false;
            final_ = (blockFinal != 0);
            unsigned long type = 0;
            if (!ReadBits(2, type)) return false;
            if (type == 0) {
                if (!Stored(out)) return false;
            } else if (type == 1) {
                FixedTables();
            } else if (type == 2) {
                if (!DynamicTables(hlit, hdist)) return false;
            } else {
                return false;
            }
            if (type != 0 && !Blocks(out, hlit, hdist)) return false;
            if (final_) break;
        }
        return true;
    }

private:
    bool ReadBits(int n, unsigned long& v) {
        v = 0;
        for (int i = 0; i < n; ++i) {
            unsigned long bit = 0;
            if (!GetBit(bit)) return false;
            v |= bit << i;  // deflate lê bits do menos significativo para o mais
        }
        return true;
    }

    bool GetBit(unsigned long& bit) {
        if (bitCount_ == 0) {
            if (pos_ >= d_.size()) return false;
            byteBuf_ = d_[pos_++];
            bitCount_ = 8;
        }
        bit = byteBuf_ & 1u;
        byteBuf_ >>= 1;
        --bitCount_;
        return true;
    }

    bool Stored(std::vector<unsigned char>& out) {
        // RFC 1951: LEN/NLEN começam no byte inteiro SEGUINTE ao header do bloco
        // (bits residuais são descartados, mas o buffer de bytes já consumido não).
        bitCount_ = 0;
        if (pos_ + 4 > d_.size()) return false;
        const unsigned len = static_cast<unsigned>(d_[pos_]) | (static_cast<unsigned>(d_[pos_ + 1]) << 8);
        const unsigned nlen = static_cast<unsigned>(d_[pos_ + 2]) | (static_cast<unsigned>(d_[pos_ + 3]) << 8);
        pos_ += 4;
        if ((len ^ 0xFFFFu) != nlen || pos_ + len > d_.size()) return false;
        out.insert(out.end(), d_.begin() + static_cast<std::ptrdiff_t>(pos_),
                   d_.begin() + static_cast<std::ptrdiff_t>(pos_ + len));
        pos_ += len;
        return true;
    }

    // Huffman canônico a partir das lengths.
    struct Huff {
        std::array<int, 16> count{};
        std::array<int, 16> nextCode{};
        std::vector<int> symbol;

        void Build(const int* lengths, int n) {
            count.fill(0);
            for (int i = 0; i < n; ++i) {
                if (lengths[i] > 0) ++count[static_cast<std::size_t>(lengths[i])];
            }
            int code = 0;
            for (int b = 1; b < 16; ++b) {
                code = (code + count[static_cast<std::size_t>(b - 1)]) << 1;
                nextCode[static_cast<std::size_t>(b)] = code;
            }
            symbol.assign(n, 0);
            for (int i = 0; i < n; ++i) {
                if (lengths[i] > 0) {
                    { const int posn = nextCode[static_cast<std::size_t>(lengths[i])]++; symbol[static_cast<std::size_t>(posn)] = i; }
                }
            }
        }

        bool Decode(Inflator& in, int& outSymbol) {
            int code = 0;
            int first = 0;
            int index = -1;
            for (int len = 1; len < 16; ++len) {
                unsigned long bit = 0;
                if (!in.GetBit(bit)) return false;
                code |= static_cast<int>(bit);
                const int countLen = count[static_cast<std::size_t>(len)];
                if (code - first < countLen) {
                    outSymbol = symbol[static_cast<std::size_t>(index + (code - first))];
                    return true;
                }
                index += countLen;
                first = (first + countLen) << 1;
                code <<= 1;
            }
            return false;
        }
    };

    void FixedTables() {
        int litDist[288];
        for (int i = 0; i < 144; ++i) litDist[i] = 8;
        for (int i = 144; i < 256; ++i) litDist[i] = 9;
        for (int i = 256; i < 280; ++i) litDist[i] = 7;
        for (int i = 280; i < 288; ++i) litDist[i] = 8;
        litLen_.Build(litDist, 288);
        int dist[32];
        for (int i = 0; i < 32; ++i) dist[i] = 5;
        distLen_.Build(dist, 32);
    }

    bool DynamicTables(unsigned char& hlit, unsigned char& hdist) {
        unsigned long v = 0;
        if (!ReadBits(5, v)) return false;
        hlit = static_cast<unsigned char>(257 + v);
        if (!ReadBits(5, v)) return false;
        hdist = static_cast<unsigned char>(1 + v);
        unsigned long hclen = 0;
        if (!ReadBits(4, v)) return false;
        hclen = 4 + v;
        static constexpr unsigned char kOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
                                                     11, 4, 12, 3, 13, 2, 14, 1, 15};
        int codeLen[19] = {0};
        for (unsigned long i = 0; i < hclen; ++i) {
            if (!ReadBits(3, v)) return false;
            codeLen[kOrder[i]] = static_cast<int>(v);
        }
        Huff codeHuff;
        codeHuff.Build(codeLen, 19);
        std::vector<int> lengths(static_cast<std::size_t>(hlit + hdist), 0);
        int i = 0;
        while (i < hlit + hdist) {
            int sym = 0;
            if (!codeHuff.Decode(*this, sym)) return false;
            if (sym < 16) {
                lengths[static_cast<std::size_t>(i++)] = sym;
            } else if (sym == 16) {
                if (i == 0) return false;
                if (!ReadBits(2, v)) return false;
                const int rep = 3 + static_cast<int>(v);
                const int prev = lengths[static_cast<std::size_t>(i - 1)];
                for (int r = 0; r < rep && i < hlit + hdist; ++r) lengths[static_cast<std::size_t>(i++)] = prev;
            } else if (sym == 17) {
                if (!ReadBits(3, v)) return false;
                for (int r = 0; r < 3 + static_cast<int>(v) && i < hlit + hdist; ++r)
                    lengths[static_cast<std::size_t>(i++)] = 0;
            } else {
                if (!ReadBits(7, v)) return false;
                for (int r = 0; r < 11 + static_cast<int>(v) && i < hlit + hdist; ++r)
                    lengths[static_cast<std::size_t>(i++)] = 0;
            }
        }
        litLen_.Build(lengths.data(), hlit);
        distLen_.Build(lengths.data() + hlit, hdist);
        return true;
    }

    bool Blocks(std::vector<unsigned char>& out, unsigned char /*hlit*/, unsigned char /*hdist*/) {
        static constexpr int kLenBase[29] = {3,  4,  5,  6,  7,  8,  9,  10,  11,  13,  15,  17,  19,  23, 27,
                                             31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
        static constexpr int kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                              2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        static constexpr int kDistBase[30] = {1,    2,    3,    4,    5,    7,     9,     13,    17,    25,
                                              33,   49,   65,   97,   129,  193,   257,   385,   513,   769,
                                              1025, 1537, 2049, 3073, 4097, 6145,  8193,  12289, 16385, 24577};
        static constexpr int kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                               6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
        while (true) {
            int sym = 0;
            if (!litLen_.Decode(*this, sym)) return false;
            if (sym < 256) {
                out.push_back(static_cast<unsigned char>(sym));
            } else if (sym == 256) {
                return true;
            } else {
                sym -= 257;
                if (sym >= 29) return false;
                unsigned long v = 0;
                if (!ReadBits(kLenExtra[sym], v)) return false;
                const int length = kLenBase[sym] + static_cast<int>(v);
                int dsym = 0;
                if (!distLen_.Decode(*this, dsym)) return false;
                if (dsym >= 30) return false;
                if (!ReadBits(kDistExtra[dsym], v)) return false;
                const int dist = kDistBase[dsym] + static_cast<int>(v);
                if (dist > static_cast<int>(out.size())) return false;
                for (int r = 0; r < length; ++r) {
                    out.push_back(out[out.size() - static_cast<std::size_t>(dist)]);
                }
            }
        }
    }

    const std::vector<unsigned char>& d_;
    std::size_t pos_;
    unsigned byteBuf_ = 0;
    int bitCount_ = 0;
    bool final_ = false;
    Huff litLen_;
    Huff distLen_;
};

// ------------------------------------------------------------------ filtros de scanline

inline unsigned Paeth(unsigned a, unsigned b, unsigned c) {
    const int p = static_cast<int>(a + b - c);
    const int pa = p > static_cast<int>(a) ? p - static_cast<int>(a) : static_cast<int>(a) - p;
    const int pb = p > static_cast<int>(b) ? p - static_cast<int>(b) : static_cast<int>(b) - p;
    const int pc = p > static_cast<int>(c) ? p - static_cast<int>(c) : static_cast<int>(c) - p;
    if (pa <= pb && pa <= pc) return a;
    return pb <= pc ? b : c;
}

bool Unfilter(std::vector<unsigned char>& raw, int height, std::size_t stride, int bpp) {
    std::vector<unsigned char> prev(stride, 0);
    for (int y = 0; y < height; ++y) {
        if (static_cast<std::size_t>(y) * (stride + 1u) >= raw.size()) return false;
        const unsigned filter = raw[static_cast<std::size_t>(y) * (stride + 1u)];
        unsigned char* row = raw.data() + static_cast<std::size_t>(y) * (stride + 1u) + 1u;
        for (std::size_t x = 0; x < stride; ++x) {
            const unsigned a = x >= static_cast<std::size_t>(bpp) ? row[x - static_cast<std::size_t>(bpp)] : 0u;
            const unsigned b = prev[x];
            const unsigned c = x >= static_cast<std::size_t>(bpp) ? prev[x - static_cast<std::size_t>(bpp)] : 0u;
            unsigned v = row[x];
            switch (filter) {
                case 0: break;
                case 1: v = (v + a) & 0xFFu; break;
                case 2: v = (v + b) & 0xFFu; break;
                case 3: v = (v + ((a + b) >> 1)) & 0xFFu; break;
                case 4: v = (v + Paeth(a, b, c)) & 0xFFu; break;
                default: return false;
            }
            row[x] = static_cast<unsigned char>(v);
        }
        std::memcpy(prev.data(), row, stride);
    }
    // Compacta: remove o byte de filtro de cada linha.
    std::vector<unsigned char> pixels((stride) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        std::memcpy(pixels.data() + static_cast<std::size_t>(y) * stride,
                    raw.data() + static_cast<std::size_t>(y) * (stride + 1u) + 1u, stride);
    }
    raw = std::move(pixels);
    return true;
}

}  // namespace

bool DecodePng(const std::vector<unsigned char>& bytes, ImageRgb& out) {
    out = ImageRgb{};
    static constexpr unsigned char kSig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (bytes.size() < 8 || std::memcmp(bytes.data(), kSig, 8) != 0) {
        return false;
    }
    std::size_t pos = 8;
    int width = 0;
    int height = 0;
    int bitDepth = 0;
    int colorType = -1;
    int interlace = 0;
    std::vector<unsigned char> idat;
    std::vector<unsigned char> palette;
    std::vector<unsigned char> trns;
    while (pos + 12 <= bytes.size()) {
        const std::uint32_t len = ReadU32(&bytes[pos]);
        const char* type = reinterpret_cast<const char*>(&bytes[pos + 4]);
        if (pos + 12 + len > bytes.size()) {
            return false;
        }
        const unsigned char* data = &bytes[pos + 8];
        if (std::memcmp(type, "IHDR", 4) == 0 && len == 13) {
            width = static_cast<int>(ReadU32(data));
            height = static_cast<int>(ReadU32(data + 4));
            bitDepth = data[8];
            colorType = data[9];
            interlace = data[12];
            if (data[10] != 0 || data[11] != 0 || interlace != 0 || width <= 0 || height <= 0 ||
                width > 16384 || height > 16384) {
                return false;  // só compression=0 / filter=0, não progressivo, limites de sanidade
            }
        } else if (std::memcmp(type, "PLTE", 4) == 0) {
            palette.assign(data, data + len);
        } else if (std::memcmp(type, "tRNS", 4) == 0) {
            trns.assign(data, data + len);
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), data, data + len);
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        pos += 12 + len;
    }
    if (width <= 0 || height <= 0 || colorType < 0 || idat.empty()) {
        return false;
    }

    std::vector<unsigned char> raw;
    Inflator infl(idat, 2);  // pula o header de 2 bytes do zlib
    if (!infl.Run(raw)) {
        return false;
    }

    // Canais por pixel e tamanho do pixel em bits.
    int channels = 0;
    switch (colorType) {
        case 0: channels = 1; break;  // cinza
        case 2: channels = 3; break;  // RGB
        case 3: channels = 1; break;  // paleta
        case 4: channels = 2; break;  // cinza+alfa
        case 6: channels = 4; break;  // RGBA
        default: return false;
    }
    const bool okDepth = (colorType == 3) ? (bitDepth == 1 || bitDepth == 2 || bitDepth == 4 || bitDepth == 8)
                                          : (bitDepth == 8 || (colorType == 0 && bitDepth == 16) ||
                                             (colorType == 0 && (bitDepth == 1 || bitDepth == 2 || bitDepth == 4)));
    if (!okDepth) {
        return false;
    }
    const int pixelBits = channels * bitDepth;
    const std::size_t stride = (static_cast<std::size_t>(width) * static_cast<std::size_t>(pixelBits) + 7u) / 8u;
    const int bpp = pixelBits >= 8 ? pixelBits / 8 : 1;  // bytes por pixel p/ filtros (sub-pixel: 1)
    if (raw.size() < stride * static_cast<std::size_t>(height) + static_cast<std::size_t>(height)) {
        return false;
    }
    if (!Unfilter(raw, height, stride, bpp)) {
        return false;
    }

    out.width = width;
    out.height = height;
    out.rgb.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3u);
    const std::size_t palEntries = palette.size() / 3u;
    for (int y = 0; y < height; ++y) {
        const unsigned char* row = raw.data() + static_cast<std::size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            std::uint8_t r = 0;
            std::uint8_t g = 0;
            std::uint8_t b = 0;
            auto sample = [&](int ch) -> unsigned {
                const std::size_t bitIdx = static_cast<std::size_t>(x) * static_cast<std::size_t>(pixelBits) +
                                          static_cast<std::size_t>(ch) * static_cast<std::size_t>(bitDepth);
                if (bitDepth == 8) {
                    return row[bitIdx / 8u];
                }
                if (bitDepth == 16) {
                    return row[bitIdx / 8u];  // mantém o byte alto
                }
                // sub-byte (1/2/4): extrai do byte
                const unsigned perByte = 8u / static_cast<unsigned>(bitDepth);
                const unsigned byteVal = row[bitIdx / 8u];
                const unsigned shift = static_cast<unsigned>((perByte - 1u - (bitIdx % perByte)) * bitDepth);
                const unsigned mask = (1u << bitDepth) - 1u;
                const unsigned v = (byteVal >> shift) & mask;
                // expande para 8 bits (proporção simples, suficiente para paleta/cinza)
                return v * 255u / mask;
            };
            switch (colorType) {
                case 0: r = g = b = static_cast<std::uint8_t>(sample(0)); break;
                case 2:
                    r = static_cast<std::uint8_t>(sample(0));
                    g = static_cast<std::uint8_t>(sample(1));
                    b = static_cast<std::uint8_t>(sample(2));
                    break;
                case 3: {
                    const unsigned idx = sample(0);
                    if (idx < palEntries) {
                        r = palette[idx * 3u];
                        g = palette[idx * 3u + 1u];
                        b = palette[idx * 3u + 2u];
                    }
                    break;
                }
                case 4: r = g = b = static_cast<std::uint8_t>(sample(0)); break;
                case 6:
                    r = static_cast<std::uint8_t>(sample(0));
                    g = static_cast<std::uint8_t>(sample(1));
                    b = static_cast<std::uint8_t>(sample(2));
                    break;
            }
            const std::size_t d = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                   static_cast<std::size_t>(x)) * 3u;
            out.rgb[d] = r;
            out.rgb[d + 1u] = g;
            out.rgb[d + 2u] = b;
        }
    }
    return true;
}

}  // namespace st
