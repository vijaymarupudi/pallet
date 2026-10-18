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


#ifndef OSCPP_SERVER_HPP_INCLUDED
#define OSCPP_SERVER_HPP_INCLUDED

#include <oscpp/detail/stream.hpp>
#include <oscpp/util.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <tuple>

namespace OSCPP { namespace Server {

//! OSC Message Argument Iterator.
/*!
 * Retrieve typed arguments from an incoming message.
 *
 * Supported tags and their correspondong types are:
 *
 *  i       -- 32 bit signed integer number<br>
 *  f       -- 32 bit floating point number<br>
 *  s       -- NULL-terminated string padded to 4-byte boundary<br>
 *  b       -- 32-bit integer size followed by 4-byte aligned data
 *
 * Every accessor returns an OSCPP::Result. The error is
 * Error::Underrun when the buffer ends too early and Error::Parse when
 * the argument has a different type than requested. After an error the
 * stream position is unspecified, so the message should be dropped.
 *
 * \sa getArgInt32
 * \sa getArgFloat32
 * \sa getArgString
 */
class ArgStream
{
public:
    //* Empty argument stream.
    ArgStream() = default;

    //* Construct argument stream from tag and value streams.
    ArgStream(const ReadStream& tags, const ReadStream& args)
    : m_tags(tags)
    , m_args(args)
    {}

    //! Read arguments from stream.
    /*!
     * The stream has to point to the start of a message type
     * signature.
     */
    static Result<ArgStream> parse(const ReadStream& stream)
    {
        ReadStream args(stream);
        return args.getString().and_then(
            [&](const char* tags) -> Result<ArgStream> {
                if (tags[0] != ',')
                    return fail(Error::Parse);
                return ArgStream(ReadStream(tags + 1, strlen(tags) - 1),
                                 args);
            });
    }

    //* Return the number of arguments that can be read from the stream.
    size_t size() const
    {
        return m_tags.capacity();
    }

    //* Return true if no more arguments can be read from the stream.
    bool atEnd() const
    {
        return m_tags.atEnd();
    }

    //* Return tag and argument streams.
    std::tuple<ReadStream, ReadStream> state() const
    {
        return std::make_tuple(m_tags, m_args);
    }

    //* Return the type tag corresponding to the next message argument.
    Result<char> tag() const
    {
        return m_tags.peekChar();
    }

    //* Drop next argument.
    Result<void> drop()
    {
        return m_tags.getChar().and_then([&](char t) { return drop(t); });
    }

    //! Get next integer argument.
    /*!
     * Read next numerical argument from the input stream and convert it
     * to an integer.
     */
    Result<int32_t> int32()
    {
        return m_tags.getChar().and_then([&](char t) -> Result<int32_t> {
            if (t == 'i')
                return m_args.getInt32();
            if (t == 'f')
                return m_args.getFloat32().and_then([](float x) -> Result<int32_t> {
                    // converting NaN or an out of range value is undefined
                    if (!(x >= -2147483648.0f && x < 2147483648.0f))
                        return fail(Error::Parse);
                    return (int32_t)x;
                });
            return fail(Error::Parse);
        });
    }

    //! Get next float argument.
    /*!
     * Read next numerical argument from the input stream and convert it
     * to a float.
     */
    Result<float> float32()
    {
        return m_tags.getChar().and_then([&](char t) -> Result<float> {
            if (t == 'f')
                return m_args.getFloat32();
            if (t == 'i')
                return m_args.getInt32().transform(
                    [](int32_t x) { return (float)x; });
            return fail(Error::Parse);
        });
    }

    //! Get next string argument.
    /*!
     * Read next string argument and return it as a NULL-terminated
     * string.
     */
    Result<const char*> string()
    {
        return m_tags.getChar().and_then([&](char t) -> Result<const char*> {
            if (t == 's')
                return m_args.getString();
            return fail(Error::Parse);
        });
    }

    //* Get next blob argument.
    Result<Blob> blob()
    {
        return m_tags.getChar().and_then([&](char t) -> Result<Blob> {
            if (t == 'b')
                return parseBlob();
            return fail(Error::Parse);
        });
    }

    //* Return a stream corresponding to an array argument.
    Result<ArgStream> array()
    {
        return m_tags.getChar().and_then([&](char t) -> Result<ArgStream> {
            if (t != '[')
                return fail(Error::Parse);
            const char* tags = m_tags.pos();
            const char* args = m_args.pos();
            return dropArray().transform([&] {
                // m_tags.pos() points right after the closing ']'.
                return ArgStream(ReadStream(tags, m_tags.pos() - tags - 1),
                                 ReadStream(args, m_args.pos() - args));
            });
        });
    }

    template <typename T> Result<T> next()
    {
        T::OSC_Server_ArgStream_next_unimplemented;
        return fail(Error::InvalidArgument);
    }

private:
    // Parse a blob (type tag already consumed).
    Result<Blob> parseBlob()
    {
        return m_args.getInt32().and_then([&](int32_t size) -> Result<Blob> {
            if (size < 0)
                return fail(Error::Parse);
            static_assert(
                sizeof(size_t) >= sizeof(int32_t),
                "Size of size_t must be greater than size of int32_t");
            const void* data = m_args.pos();
            return m_args.skip(align(size)).transform([&] {
                return Blob(data, static_cast<size_t>(size));
            });
        });
    }
    // Drop an atomic value of type t (type tag already consumed).
    Result<void> dropAtom(char t)
    {
        switch (t)
        {
            case 'i':
            case 'f':
            case 'c':
            case 'r':
            case 'm':
                return m_args.skip(4);
            case 'h':
            case 't':
            case 'd':
                return m_args.skip(8);
            case 's':
            case 'S':
                return m_args.getString().transform([](const char*) {});
            case 'b':
                return parseBlob().transform([](Blob) {});
            case 'T':
            case 'F':
            case 'N':
            case 'I':
                return {};
        }
        // the size of an unknown type is not known, so the arguments
        // after it cannot be found
        return fail(Error::Parse);
    }
    // Drop a possibly nested array.
    Result<void> dropArray()
    {
        unsigned int level = 0;
        for (;;)
        {
            auto t = m_tags.getChar();
            if (!t)
                return std::unexpected(t.error());
            if (*t == ']')
            {
                if (level == 0)
                    break;
                else
                    level--;
            }
            else if (*t == '[')
            {
                level++;
            }
            else if (auto dropped = dropAtom(*t); !dropped)
            {
                return dropped;
            }
        }
        return {};
    }
    // Drop the next argument of type t (type tag already consumed).
    Result<void> drop(char t)
    {
        switch (t)
        {
            case '[':
                return dropArray();
            default:
                return dropAtom(t);
        }
    }

private:
    ReadStream m_tags;
    ReadStream m_args;
};

class Message
{
public:
    Message(const char* address, const ArgStream& args)
    : m_address(address)
    , m_args(args)
    {}

    const char* address() const
    {
        return m_address;
    }

    ArgStream args() const
    {
        return m_args;
    }

private:
    const char* m_address;
    ArgStream   m_args;
};

class PacketStream;

class Bundle
{
public:
    Bundle(uint64_t time, const ReadStream& stream)
    : m_time(time)
    , m_stream(stream)
    {}

    uint64_t time() const
    {
        return m_time;
    }

    inline PacketStream packets() const;

private:
    uint64_t   m_time;
    ReadStream m_stream;
};

class Packet
{
public:
    Packet()
    : m_isBundle(false)
    {}

    Packet(const ReadStream& stream)
    : m_stream(stream)
    , m_isBundle(isBundle(stream))
    {
        // Skip over #bundle header, which isBundle() guarantees to be
        // present.
        if (m_isBundle)
            m_stream.advance(8);
    }

    Packet(const void* data, size_t size)
    : Packet(ReadStream(data, size))
    {}

    const void* data() const
    {
        return m_stream.begin();
    }

    size_t size() const
    {
        return m_stream.capacity();
    }

    bool isBundle() const
    {
        return m_isBundle;
    }

    bool isMessage() const
    {
        return !isBundle();
    }

    //* Error::Parse if the packet is not a bundle.
    Result<Bundle> bundle() const
    {
        if (!isBundle())
            return fail(Error::Parse);
        ReadStream stream(m_stream);
        return stream.getUInt64().transform(
            [&](uint64_t time) { return Bundle(time, stream); });
    }

    //* Error::Parse if the packet is not a message.
    Result<Message> message() const
    {
        if (!isMessage())
            return fail(Error::Parse);
        ReadStream stream(m_stream);
        return stream.getString().and_then(
            [&](const char* address) -> Result<Message> {
                return ArgStream::parse(stream).transform(
                    [&](ArgStream args) { return Message(address, args); });
            });
    }

    static bool isMessage(const void* data, size_t size)
    {
        return (size > 3) && (static_cast<const char*>(data)[0] != '#');
    }

    static bool isMessage(const ReadStream& stream)
    {
        return isMessage(stream.pos(), stream.consumable());
    }

    static bool isBundle(const void* data, size_t size)
    {
        return (size > 15) && (std::memcmp(data, "#bundle", 8) == 0);
    }

    static bool isBundle(const ReadStream& stream)
    {
        return isBundle(stream.pos(), stream.consumable());
    }

private:
    ReadStream m_stream;
    bool       m_isBundle;
};

class PacketStream
{
public:
    PacketStream(const ReadStream& stream)
    : m_stream(stream)
    {}

    bool atEnd() const
    {
        return m_stream.atEnd();
    }

    Result<Packet> next()
    {
        return m_stream.getInt32().and_then([&](int32_t size) -> Result<Packet> {
            if (size < 0)
                return fail(Error::Parse);
            return m_stream.slice(size).and_then(
                [&](ReadStream stream) -> Result<Packet> {
                    return m_stream.skip(size).transform(
                        [&] { return Packet(stream); });
                });
        });
    }

private:
    ReadStream m_stream;
};

template <> inline Result<int32_t> ArgStream::next<int32_t>()
{
    return int32();
}

template <> inline Result<float> ArgStream::next<float>()
{
    return float32();
}

template <> inline Result<const char*> ArgStream::next<const char*>()
{
    return string();
}

template <> inline Result<Blob> ArgStream::next<Blob>()
{
    return blob();
}

template <> inline Result<ArgStream> ArgStream::next<ArgStream>()
{
    return array();
}

PacketStream Bundle::packets() const
{
    return PacketStream(m_stream);
}

}} // namespace OSCPP::Server

static inline bool operator==(const OSCPP::Server::Message& msg,
                              const char*                   str)
{
    return strcmp(msg.address(), str) == 0;
}

static inline bool operator==(const char*                   str,
                              const OSCPP::Server::Message& msg)
{
    return msg == str;
}

static inline bool operator!=(const OSCPP::Server::Message& msg,
                              const char*                   str)
{
    return !(msg == str);
}

static inline bool operator!=(const char*                   str,
                              const OSCPP::Server::Message& msg)
{
    return msg != str;
}

#endif // OSCPP_SERVER_HPP_INCLUDED
