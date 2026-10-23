#ifndef TINYSPRITE_IO_BYTE_READER_HPP
#define TINYSPRITE_IO_BYTE_READER_HPP

#include <cstddef>
#include <cstdint>
#include <stdexcept>

/* Reads little-endian values from a byte range; running past its end throws std::out_of_range,
 * which the decoders (pxa_decode, aseprite_decode) report as an invalid file. */
class Reader
{
  public:
    Reader(const uint8_t *begin, const uint8_t *end) : p_(begin), end_(end)
    {
    }

    bool atEnd() const
    {
        return p_ == end_;
    }
    size_t remaining() const
    {
        return static_cast<size_t>(end_ - p_);
    }
    const uint8_t *take(size_t count)
    {
        if (count > static_cast<size_t>(end_ - p_)) {
            throw std::out_of_range("truncated");
        }
        const uint8_t *start = p_;
        p_ += count;
        return start;
    }
    uint32_t u8()
    {
        return *take(1);
    }
    uint32_t u16()
    {
        const uint8_t *b = take(2);
        return static_cast<uint32_t>(b[0]) | static_cast<uint32_t>(b[1]) << 8;
    }
    uint32_t u32()
    {
        uint32_t low = u16();
        return low | u16() << 16;
    }

  private:
    const uint8_t *p_;
    const uint8_t *end_;
};

#endif
