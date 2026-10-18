// oscpp library
//
// Copyright (c) 2004-2018 Stefan Kersten <kaoskorobase@gmail.com>
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


#ifndef OSCPP_CLIENT_HPP_INCLUDED
#define OSCPP_CLIENT_HPP_INCLUDED

#include <oscpp/detail/host.hpp>
#include <oscpp/detail/stream.hpp>
#include <oscpp/util.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace OSCPP { namespace Client {

//! OSC packet construction.
/*!
 * Construct a valid OSC packet for transmitting over a transport
 * medium.
 *
 * Every writing function returns an OSCPP::Result. The error is
 * Error::Overflow when the buffer is too small and
 * Error::InvalidArgument when the call does not fit the packet's state.
 * After an error the contents of the packet are unspecified, so call
 * reset() before building the next packet.
 */
class Packet
{
    Result<int32_t> ptrDiff(const char* a, const char* b)
    {
        // Make sure pointer difference fits into int32_t
        const intptr_t diff = a - b;
        if (diff < std::numeric_limits<int32_t>::min() ||
            diff > std::numeric_limits<int32_t>::max())
        {
            return fail(Error::InvalidArgument);
        }
        return static_cast<int32_t>(diff);
    }

    Result<int32_t> calcSize(const char* begin, const char* end)
    {
        return ptrDiff(end, begin).and_then([](int32_t diff) -> Result<int32_t> {
            const int32_t size = diff - 4;
            if (size < 0)
                return fail(Error::InvalidArgument);
            return size;
        });
    }

public:
    //! Constructor.
    /*!
     */
    Packet()
    {
        reset(0, 0);
    }

    //! Constructor.
    /*!
     * The buffer has to be aligned to 4 bytes, otherwise writing fails
     * with Error::Unaligned.
     */
    Packet(void* buffer, size_t size)
    {
        reset(buffer, size);
    }

    //! A packet points into its buffer, so a copy would write to the
    //! buffer of the original.
    Packet(const Packet&) = delete;
    Packet& operator=(const Packet&) = delete;

    //! Destructor.
    virtual ~Packet()
    {}

    //! Get packet buffer address.
    /*!
     * Return the start address of the packet currently under
     * construction.
     */
    void* data() const
    {
        return m_buffer;
    }

    size_t capacity() const
    {
        return m_capacity;
    }

    //! Get packet content size.
    /*!
     * Return the size of the packet currently under construction.
     */
    size_t size() const
    {
        return m_args.consumed();
    }

    //! Reset packet state.
    void reset(void* buffer, size_t size)
    {
        m_buffer = buffer;
        m_capacity = size;
        m_args = WriteStream(m_buffer, m_capacity);
        m_tags = WriteStream();
        m_sizePosM = m_sizePosB = nullptr;
        m_inBundle = 0;
    }

    void reset()
    {
        reset(m_buffer, m_capacity);
    }

    Result<void> openBundle(uint64_t time)
    {
        if (m_inBundle > 0)
        {
            assert(m_sizePosB != nullptr || m_inBundle == 1);
            // Remember previous size pos offset
            int32_t offset = 0;
            if (m_sizePosB != nullptr)
            {
                auto diff = ptrDiff(m_sizePosB, m_args.begin());
                if (!diff)
                    return std::unexpected(diff.error());
                offset = *diff;
            }
            char* curPos = m_args.pos();
            if (auto skipped = m_args.skip(4); !skipped)
                return skipped;
            // Record size pos
            std::memcpy(curPos, &offset, 4);
            m_sizePosB = curPos;
        }
        else if (m_args.pos() != m_args.begin())
        {
            // Cannot open toplevel bundle in non-empty packet
            return fail(Error::InvalidArgument);
        }

        m_inBundle++;
        return m_args.putString("#bundle").and_then(
            [&] { return m_args.putUInt64(time); });
    }

    Result<void> closeBundle()
    {
        if (m_inBundle == 0)
        {
            // closeBundle() without matching openBundle()
            return fail(Error::InvalidArgument);
        }
        if (m_inBundle > 1)
        {
            // Get current stream pos
            char* curPos = m_args.pos();

            // Get previous bundle size stream pos
            int32_t offset;
            std::memcpy(&offset, m_sizePosB, 4);
            // Get previous size pos
            char* prevPos = m_args.begin() + offset;

            auto bundleSize = calcSize(m_sizePosB, curPos);
            if (!bundleSize)
                return std::unexpected(bundleSize.error());
            assert(*bundleSize >= 0 &&
                   (size_t)*bundleSize >= Size::bundle(0));
            // Write bundle size
            m_args.setPos(m_sizePosB);
            if (auto written = m_args.putInt32(*bundleSize); !written)
                return written;
            m_args.setPos(curPos);

            // record outer bundle size pos
            m_sizePosB = prevPos;
        }
        m_inBundle--;
        return {};
    }

    Result<void> openMessage(const char* addr, size_t numTags)
    {
        if (m_inBundle > 0)
        {
            // record message size pos
            m_sizePosM = m_args.pos();
            // advance arg stream
            if (auto skipped = m_args.skip(4); !skipped)
                return skipped;
        }
        if (auto written = m_args.putString(addr); !written)
            return written;
        size_t sigLen = numTags + 2;
        // the tag stream excludes the terminating NUL, so that writing
        // more tags than declared overflows instead of removing it
        auto tags = m_args.slice(sigLen - 1);
        if (!tags)
            return std::unexpected(tags.error());
        m_tags = *tags;
        return m_args.zero(align(sigLen)).and_then(
            [&] { return m_tags.putChar(','); });
    }

    Result<void> closeMessage()
    {
        if (m_inBundle > 0)
        {
            // closeMessage() without matching openMessage()
            if (m_sizePosM == nullptr)
                return fail(Error::InvalidArgument);
            // Get current stream pos
            char* curPos = m_args.pos();
            auto  size = calcSize(m_sizePosM, curPos);
            if (!size)
                return std::unexpected(size.error());
            // write message size
            m_args.setPos(m_sizePosM);
            if (auto written = m_args.putInt32(*size); !written)
                return written;
            // restore stream pos
            m_args.setPos(curPos);
            // reset tag stream
            m_tags = WriteStream();
            m_sizePosM = nullptr;
        }
        return {};
    }

    //! Write integer message argument.
    /*!
     * Write a 32 bit integer message argument.
     *
     * \param arg 32 bit integer argument.
     *
     * \pre openMessage must have been called before with no intervening
     * closeMessage.
     */
    Result<void> int32(int32_t arg)
    {
        return m_tags.putChar('i').and_then(
            [&] { return m_args.putInt32(arg); });
    }

    Result<void> float32(float arg)
    {
        return m_tags.putChar('f').and_then(
            [&] { return m_args.putFloat32(arg); });
    }

    Result<void> string(const char* arg)
    {
        return m_tags.putChar('s').and_then(
            [&] { return m_args.putString(arg); });
    }

    // Error::InvalidArgument if blob size is greater than
    // std::numeric_limits<int32_t>::max()
    Result<void> blob(const Blob& arg)
    {
        if (arg.size() > (size_t)std::numeric_limits<int32_t>::max())
            return fail(Error::InvalidArgument);
        return m_tags.putChar('b')
            .and_then([&] {
                return m_args.putInt32(static_cast<int32_t>(arg.size()));
            })
            .and_then([&] { return m_args.putData(arg.data(), arg.size()); });
    }

    Result<void> openArray()
    {
        return m_tags.putChar('[');
    }

    Result<void> closeArray()
    {
        return m_tags.putChar(']');
    }

    template <typename T> Result<void> put(T)
    {
        T::OSC_Client_Packet_put_unimplemented;
        return fail(Error::InvalidArgument);
    }

    template <typename InputIterator>
    Result<void> put(InputIterator begin, InputIterator end)
    {
        for (auto it = begin; it != end; it++)
        {
            if (auto written = put(*it); !written)
                return written;
        }
        return {};
    }

    template <typename InputIterator>
    Result<void> putArray(InputIterator begin, InputIterator end)
    {
        return openArray()
            .and_then([&] { return put<InputIterator>(begin, end); })
            .and_then([&] { return closeArray(); });
    }

private:
    void*       m_buffer;
    size_t      m_capacity;
    WriteStream m_args;     // packet stream
    WriteStream m_tags;     // current tag stream
    char*       m_sizePosM; // last message size position
    char*       m_sizePosB; // last bundle size position
    size_t      m_inBundle; // bundle nesting depth
};

template <> inline Result<void> Packet::put<int32_t>(int32_t x)
{
    return int32(x);
}
template <> inline Result<void> Packet::put<float>(float x)
{
    return float32(x);
}
template <> inline Result<void> Packet::put<const char*>(const char* x)
{
    return string(x);
}
template <> inline Result<void> Packet::put<Blob>(Blob x)
{
    return blob(x);
}

template <size_t buffer_size> class StaticPacket : public Packet
{
public:
    StaticPacket()
    : Packet(m_buffer, buffer_size)
    {}

private:
    alignas(kAlignment) char m_buffer[buffer_size];
};

class DynamicPacket : public Packet
{
public:
    DynamicPacket(size_t buffer_size)
    : Packet(static_cast<char*>(new char[buffer_size]), buffer_size)
    {}

    ~DynamicPacket()
    {
        delete[] static_cast<char*>(data());
    }
};

}} // namespace OSCPP::Client

#endif // OSCPP_CLIENT_HPP_INCLUDED
