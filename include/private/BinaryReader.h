#ifndef BINARYREADER_H_
#define BINARYREADER_H_

/*!
 * @file BinaryReader.h
 * Read raw data like a stream.
 */

#include <cstdint>
#include <cstring>
#include <span>

#include "common.h"

class BinaryReader {
public:
    explicit BinaryReader(std::span<const u8> _span)
        : m_span(_span)
    {
    }

    template <typename T>
    T read()
    {
        T obj;
        memcpy(&obj, m_span.data() + m_seek, sizeof(T));
        m_seek += sizeof(T);
        return obj;
    }

    template <typename T>
    T peek()
    {
        T obj;
        memcpy(&obj, m_span.data() + m_seek, sizeof(T));
        return obj;
    }

    void ffwd(int amount)
    {
        m_seek += amount;
    }

    uint32_t bytes_left() const { return m_span.size() - m_seek; }
    const uint8_t* here() { return m_span.data() + m_seek; }
    BinaryReader at(u32 pos) { return BinaryReader(m_span.subspan(pos)); }
    uint32_t get_seek() const { return m_seek; }
    void set_seek(u32 seek) { m_seek = seek; }

private:
    std::span<const u8> m_span;
    uint32_t m_seek = 0;
};

#endif // BINARYREADER_H_
