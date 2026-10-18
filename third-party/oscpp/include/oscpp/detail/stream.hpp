// oscpp library
//
// Copyright (c) 2004-2026 Stefan Kersten <kaoskorobase@gmail.com>
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
//
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.


#ifndef OSCPP_STREAM_HPP_INCLUDED
#define OSCPP_STREAM_HPP_INCLUDED

#include <oscpp/detail/host.hpp>
#include <oscpp/error.hpp>
#include <oscpp/types.hpp>
#include <oscpp/util.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>

namespace OSCPP {

class Stream
{
public:
    Stream()
    {
        m_begin = m_end = m_pos = 0;
    }

    Stream(void* data, size_t size)
    {
        m_begin = static_cast<char*>(data);
        m_end = m_begin + size;
        m_pos = m_begin;
    }

    Stream(const Stream& stream)
    {
        m_begin = m_pos = stream.m_pos;
        m_end = stream.m_end;
    }

    Stream& operator=(Stream other)
    {
        swap(other);
        return *this;
    }

    void swap(Stream& other)
    {
        std::swap(m_begin, other.m_begin);
        std::swap(m_end, other.m_end);
        std::swap(m_pos, other.m_pos);
    }

    void reset()
    {
        m_pos = m_begin;
    }

    const char* begin() const
    {
        return m_begin;
    }

    char* begin()
    {
        return m_begin;
    }

    const char* end() const
    {
        return m_end;
    }

    size_t capacity() const
    {
        return end() - begin();
    }

    const char* pos() const
    {
        return m_pos;
    }

    char* pos()
    {
        return m_pos;
    }

    void setPos(char* pos)
    {
        assert((pos >= m_begin) && (pos <= m_end));
        m_pos = pos;
    }

    void advance(size_t n)
    {
        m_pos += n;
    }

    bool atEnd() const
    {
        return pos() == end();
    }

    size_t consumed() const
    {
        return pos() - begin();
    }

    size_t consumable() const
    {
        return end() - pos();
    }

    inline Result<void> checkAlignment(size_t n) const
    {
        return OSCPP::checkAlignment(pos(), n);
    }

protected:
    char* m_begin;
    char* m_end;
    char* m_pos;
};

template <ByteOrder B> class BasicWriteStream : public Stream
{
public:
    BasicWriteStream()
    : Stream()
    {}

    BasicWriteStream(void* data, size_t size)
    : Stream(data, size)
    {}

    BasicWriteStream(const BasicWriteStream& stream)
    : Stream(stream)
    {}

    BasicWriteStream& operator=(BasicWriteStream other)
    {
        Stream::swap(other);
        return *this;
    }

    // Return a stream over the next size bytes, or Error::Overflow.
    Result<BasicWriteStream> slice(size_t size) const
    {
        return checkWritable(size).transform([&] {
            BasicWriteStream stream(*this);
            stream.m_end = stream.m_pos + size;
            return stream;
        });
    }

    inline Result<void> checkWritable(size_t n) const
    {
        if (consumable() < n)
            return fail(Error::Overflow);
        return {};
    }

    Result<void> skip(size_t n)
    {
        return checkWritable(n).transform([&] { advance(n); });
    }

    Result<void> zero(size_t n)
    {
        return checkWritable(n).transform([&] {
            std::memset(m_pos, 0, n);
            advance(n);
        });
    }

    Result<void> putChar(char c)
    {
        return checkWritable(1).transform([&] {
            *pos() = c;
            advance(1);
        });
    }

    Result<void> putInt32(int32_t x)
    {
        return checkWritable(4)
            .and_then([&] { return checkAlignment(4); })
            .transform([&] {
                uint32_t uh;
                memcpy(&uh, &x, 4);
                const uint32_t un = convert32<B>(uh);
                std::memcpy(pos(), &un, 4);
                advance(4);
            });
    }

    Result<void> putUInt64(uint64_t x)
    {
        return checkWritable(8).transform([&] {
            const uint64_t un = convert64<B>(x);
            std::memcpy(pos(), &un, 8);
            advance(8);
        });
    }

    Result<void> putFloat32(float f)
    {
        return checkWritable(4)
            .and_then([&] { return checkAlignment(4); })
            .transform([&] {
                uint32_t uh;
                std::memcpy(&uh, &f, 4);
                const uint32_t un = convert32<B>(uh);
                std::memcpy(pos(), &un, 4);
                advance(4);
            });
    }

    Result<void> putFloat64(double f)
    {
        return checkWritable(8)
            .and_then([&] { return checkAlignment(4); })
            .transform([&] {
                uint64_t uh;
                std::memcpy(&uh, &f, 8);
                const uint64_t un = convert64<B>(uh);
                std::memcpy(pos(), &un, 8);
                advance(8);
            });
    }

    Result<void> putData(const void* data, size_t size)
    {
        const size_t padding = OSCPP::padding(size);
        const size_t n = size + padding;
        return checkWritable(n).transform([&] {
            std::memcpy(pos(), data, size);
            std::memset(pos() + size, 0, padding);
            advance(n);
        });
    }

    Result<void> putString(const char* s)
    {
        return putData(s, strlen(s) + 1);
    }
};

typedef BasicWriteStream<NetworkByteOrder> WriteStream;

template <ByteOrder B> class BasicReadStream : public Stream
{
public:
    BasicReadStream()
    {}

    BasicReadStream(const void* data, size_t size)
    : Stream(const_cast<void*>(data), size)
    {}

    BasicReadStream(const BasicReadStream& stream)
    : Stream(stream)
    {}

    BasicReadStream& operator=(BasicReadStream other)
    {
        Stream::swap(other);
        return *this;
    }

    // Return a stream over the next size bytes, or Error::Underrun.
    Result<BasicReadStream> slice(size_t size) const
    {
        return checkReadable(size).transform([&] {
            BasicReadStream stream(*this);
            stream.m_end = stream.m_pos + size;
            return stream;
        });
    }

    Result<void> checkReadable(size_t n) const
    {
        if (consumable() < n)
            return fail(Error::Underrun);
        return {};
    }

    Result<void> skip(size_t n)
    {
        return checkReadable(n).transform([&] { advance(n); });
    }

    inline Result<char> peekChar() const
    {
        return checkReadable(1).transform([&] { return *pos(); });
    }

    inline Result<char> getChar()
    {
        return peekChar().transform([&](char x) {
            advance(1);
            return x;
        });
    }

    inline Result<int32_t> peekInt32() const
    {
        return checkReadable(4)
            .and_then([&] { return checkAlignment(4); })
            .transform([&] {
                uint32_t un;
                std::memcpy(&un, pos(), 4);
                const uint32_t uh = convert32<B>(un);
                int32_t        x;
                std::memcpy(&x, &uh, 4);
                return x;
            });
    }

    inline Result<int32_t> getInt32()
    {
        return peekInt32().transform([&](int32_t x) {
            advance(4);
            return x;
        });
    }

    inline Result<uint64_t> getUInt64()
    {
        return checkReadable(8).transform([&] {
            uint64_t un;
            std::memcpy(&un, pos(), 8);
            advance(8);
            return convert64<B>(un);
        });
    }

    inline Result<float> getFloat32()
    {
        return checkReadable(4)
            .and_then([&] { return checkAlignment(4); })
            .transform([&] {
                uint32_t un;
                std::memcpy(&un, pos(), 4);
                advance(4);
                const uint32_t uh = convert32<B>(un);
                float          f;
                std::memcpy(&f, &uh, 4);
                return f;
            });
    }

    inline Result<double> getFloat64()
    {
        return checkReadable(8)
            .and_then([&] { return checkAlignment(4); })
            .transform([&] {
                uint64_t un;
                std::memcpy(&un, pos(), 8);
                advance(8);
                const uint64_t uh = convert64<B>(un);
                double         f;
                std::memcpy(&f, &uh, 8);
                return f;
            });
    }

    // The string must be NULL-terminated and padded to a 4-byte
    // boundary, otherwise the result is Error::Underrun.
    Result<const char*> getString()
    {
        return checkReadable(4).and_then([&]() -> Result<const char*> {
            // The terminator is in the last byte of a 4-byte group. Index
            // instead of advancing a pointer, which could point past the
            // end of the buffer.
            const size_t available = consumable();
            for (size_t i = 3; i < available; i += 4)
            {
                if (pos()[i] == '\0')
                {
                    const char* x = pos();
                    advance(i + 1);
                    return x;
                }
            }
            return fail(Error::Underrun);
        });
    }
};

typedef BasicReadStream<NetworkByteOrder> ReadStream;
} // namespace OSCPP

#endif // OSCPP_STREAM_HPP_INCLUDED
