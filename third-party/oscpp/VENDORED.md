# Vendored oscpp

Upstream: https://github.com/kaoskorobase/oscpp, commit `a62fe7690ce3563c997d7c9915d8cb54ff8f79b0`
(MIT licence, see `LICENSE`).

Local changes:

1. **No exceptions.** Every function that threw now returns `OSCPP::Result<T>`, which is
   `std::expected<T, std::error_condition>`. The conditions come from `OSCPP::Error`
   (`Underrun`, `Overflow`, `Parse`, `Unaligned`, `InvalidArgument`) in `error.hpp`, and compare
   with `==`, for example `result.error() == OSCPP::Error::Parse`.
2. **API changes that follow from 1.**
   - Constructors that could fail became functions: `ArgStream::parse(stream)`,
     `Packet::message()` and `Packet::bundle()` replace the converting constructors and
     conversion operators, and `slice(size)` on the streams replaces the two-argument constructor.
   - The packet writers return `Result<void>` instead of `Packet&`, so calls can no longer be chained
     with `.`. Use `.and_then`.
   - `OverflowError` carried the number of missing bytes. The condition does not.
3. **Fixes.**
   - `PacketStream::next` rejects a negative packet size instead of converting it to a huge
     `size_t`.
   - `Client::Packet::reset` checked the alignment of the address of its own member. It no
     longer checks, because the stream writes check the alignment of the write position.
   - `ArgStream::drop` consumed no data for the tags `h t d c r m S`, so the arguments after them
     were read from the wrong offset. It now skips them, and fails with `Error::Parse` on a tag whose
     size is unknown. (Only `i f s b` and arrays can be read; the other types can only be dropped.)
   - `ReadStream::getString` advanced a pointer up to 3 bytes past the end of the buffer (undefined
     behaviour); it indexes instead.
   - `ArgStream::int32` converted an `f` argument that is NaN or out of the `int32_t` range, which is
     undefined. It fails with `Error::Parse`.
   - `Client::Packet::closeMessage` inside a bundle without `openMessage` (or twice) subtracted from a null
     pointer. It fails with `Error::InvalidArgument`.
   - `Client::Packet` (and so `StaticPacket` and `DynamicPacket`) is no longer copyable, because a copy
     pointed into the buffer of the original.
   - `Client::Packet::openMessage` let the tag stream write over the terminating NUL of the tag
     string, so a message with more arguments than declared could produce a corrupt packet.
     The extra tag now fails with `Error::Overflow`.
   - `StaticPacket` uses `alignas` instead of the deprecated `std::aligned_storage`.
   - `Client::Packet::reset` also clears the tag stream, `Server::Message` is constructed from an
     address and an `ArgStream`, and the primary templates of `convert32` and `convert64` are deleted
     instead of throwing.
4. **Removed.** `print.hpp` (a debug printer built on the throwing API), the tests, the install
   rules and the CMake presets. The `CMakeLists.txt` only defines the header-only target and
   requires C++23. The upstream `README.md` is kept as it was and still describes the exceptions
   and the `.`-chaining.
