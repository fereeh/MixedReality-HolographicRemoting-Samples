//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Standard definition of guid type, interchangable with other definitions including from Windows.h.
// Includes constexpr routines for parsing and formatting the common string representation:
//   {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
//
// Windows consumers reference the native Windows types via typedef in lieu of defining separate types
// in this library. This enables composition with other Windows-only libraries that use the GUID type.
//
// In order to ensure portability and ABI stability, no dependecies are taken outside the CRT and STL.
// STL usage is limited to code compiled into consumers, and is not used on ABI boundaries. It may
// be conditionally disabled by defining MRFOUNDATION_NO_STL.
//
// Guid values are represented in a structured format with 32-bit and 16-bit fields for the first three
// sections of the string representation. The encoding of those fields is machine-dependent and varies
// based on the bitness of the machine. However, this means that all uses of the fields in code are
// abstracted from machine-specific details - except when the raw bytes in memory are accessed.
// On little-endian machines (including x86/x64) the in-memory encoding happens to conform to the
// "Variant-2" UUID definition, regardless of which Variant the value actually belongs to:
//
//      String: {00112233-4455-6677-8899-aabbccddeeff}
//   As UInt32: [00112233]   66774455    bbaa9988    ffeeddcc
//   As UInt16:  2233  0011 [4455][6677] 9988  bbaa  ddcc  ffee
//    As Bytes:  33 22 11 00 55 44 77 66[88 99 aa bb cc dd ee ff]
//
// By contrast a "Variant-1" UUID is encoded as a big-endian array of bytes:
//
//      String: {00112233-4455-6677-8899-aabbccddeeff}
//    As Bytes: [00 11 22 33][44 55][66 77][88 99 aa bb cc dd ee ff]
//
// That representation is used by both libuuid and boost-uuid, so converting a guid to/from those types
// requires serializing/deserializing to account for the endianness of the current machine.
//
// Note that in theory, the first half of the byte at index 8 encodes which Variant the value conforms
// to, with 8..b indicating Variant-1, and c..d indicating Variant-2. All modern uuid generators create
// Variant-1 values. Variant-2 values are designated as "reserved, Microsoft Corporation backwards
// compatibility" but do include some very common values including the IIDs of IUnknown, IWeakReference,
// and IWeakReferenceSource. By the letter of the specification, Variant-2 values *ought* to be
// conditionally serialized with the first three fields encoded as little-endian, rather than big-endian.
// Likewise, parsing a Variant-2 string representation into a byte-array representation *ought* to yield
// the following byte-array representations:
//
//      String: {00000000-0000-0000-c000-000000000046} // IUnknown
//    As Bytes: [00 00 00 00][00 00][00 00][c0 00 00 00 00 00 00 46]
//
//      String: {00000037-0000-0000-c000-000000000046} // IWeakReference
//    As Bytes: [37 00 00 00][00 00][00 00][c0 00 00 00 00 00 00 46]
//
//      String: {00000038-0000-0000-c000-000000000046} // IWeakReferenceSource
//    As Bytes: [38 00 00 00][00 00][00 00][c0 00 00 00 00 00 00 46]
//
// "Proper" deserialization of these byte-array representations would detect the c value in the first
// half of the 8th byte, and parse the first 8 bytes as little-endian fields rather than big-endian.
//
// However -- in practice, both libuuid and boost-uuid simply presume that all values are Variant-1
// (that is, big-endian) and do not detect or account for the actual encoded Variant in the 8th byte
// either on parsing or deserialization.
//
// This library primarily operates on the structured representation, which is agnostic to any of these
// byte-ordering concerns, and therefore no special logic or hard-coded assumptions are necessary for
// any of the parsing or formatting routines. Serialization and deserialization routines are provided
// which specifically produce/consume the Variant-1 representation, which are sufficient and appropriate
// for use with both libuuid and boost-uuid given their presumption of Variant-1.
//
// However, if some other library *does* account for varying representation based on the encoded
// Variant specified in the value, additional Variant-2 serialization/deserialization routines
// are also provided, and must be used conditionally based on the encoded Variant in the value.
// These may also be useful in cases where Variant-2 encoding has historically been used implicitly
// by directly copying the bytes of a structured guid on a little-endian machine, and that convention
// must be maintained for backwards-compabitibility as new support for big-endian machines is added.
//
// Note that simply blitting a structured guid value to storage or network will be subject to endianness
// considerations if the consuming machine has a different endianness. Therefore explicitly serializaing
// and deserializing to a machine-independent format is recommended to avoid any potential problems.
//
// See also: https://en.wikipedia.org/wiki/Universally_unique_identifier#Encoding
//

#ifndef MRFOUNDATION_GUID_H
#define MRFOUNDATION_GUID_H

#include <sal.h>

#ifdef _WIN32
#define MRFOUNDATION_USING_WINDOWS_GUID

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define MRFOUNDATION_DEFINED_WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#define MRFOUNDATION_DEFINED_NOMINMAX
#endif

#include <combaseapi.h>

#ifdef MRFOUNDATION_DEFINED_NOMINMAX
#undef MRFOUNDATION_DEFINED_NOMINMAX
#undef NOMINMAX
#endif

#ifdef MRFOUNDATION_DEFINED_WIN32_LEAN_AND_MEAN
#undef MRFOUNDATION_DEFINED_WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#else
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include <stdint.h>
#include <string.h>
#include <wchar.h>

#ifndef MRFOUNDATION_NO_STL
#include <string>
#include <iostream>
#endif

// Mixed Reality Foundation types
namespace mrf {
#ifdef MRFOUNDATION_USING_WINDOWS_GUID
    using guid = ::GUID;
#else
    struct guid {
        uint32_t Data1;
        uint16_t Data2;
        uint16_t Data3;
        uint8_t Data4[8];
    };

    inline bool operator==(guid const& left, guid const& right) noexcept {
        return memcmp(&left, &right, sizeof(left)) == 0;
    }

    inline bool operator!=(guid const& left, guid const& right) noexcept {
        return !(left == right);
    }
#endif

    static_assert(sizeof(guid) == 16);
    static_assert(offsetof(guid, Data1) == 0);
    static_assert(offsetof(guid, Data2) == 4);
    static_assert(offsetof(guid, Data3) == 6);
    static_assert(offsetof(guid, Data4) == 8);
    static_assert(guid{0xffffffff, 0, 0, {}}.Data1 > 0);
    static_assert(guid{0, 0xffff, 0, {}}.Data2 > 0);
    static_assert(guid{0, 0, 0xffff, {}}.Data3 > 0);
    static_assert(guid{0, 0, 0, {0xff}}.Data4[0] > 0);

    constexpr guid guid_null{};
} // namespace mrf

// The C++ Argument Dependent Lookup (ADL) rules will find operators for a type that are declared in the same
// namespace as the type itself - however, if the type is an alias, this applies to the target of the alias,
// not the alias itself. Therefore, when mrf::guid is aliased to ::GUID, its operators must live in
// the global namespace, not in mrf.
#ifdef MRFOUNDATION_USING_WINDOWS_GUID
#define MRFOUNDATION_OPERATOR_NS_BEGIN(NS)
#define MRFOUNDATION_OPERATOR_NS_END
#else
#define MRFOUNDATION_OPERATOR_NS_BEGIN(NS) namespace NS {
#define MRFOUNDATION_OPERATOR_NS_END }
#endif

MRFOUNDATION_OPERATOR_NS_BEGIN(mrf)
inline bool operator<(mrf::guid const& left, mrf::guid const& right) noexcept {
    // Compare each field in order to ensure proper behavior regardless of endianness of the current machine.
    if (left.Data1 < right.Data1) {
        return true;
    } else if (left.Data1 == right.Data1) {
        if (left.Data2 < right.Data2) {
            return true;
        } else if (left.Data2 == right.Data2) {
            if (left.Data3 < right.Data3) {
                return true;
            } else if (left.Data3 == right.Data3) {
                return memcmp(&left.Data4, &right.Data4, sizeof(left.Data4)) < 0;
            }
        }
    }
    return false;
}
MRFOUNDATION_OPERATOR_NS_END

namespace mrf {
    // Converts from structured representation (guid) to Variant-1 UUID byte-array encoding
    // by extracting the bytes out of their corresponding unsigned integer representations.
    constexpr inline void encode_guid_as_variant1(const guid& g, uint8_t (&u)[16]) {
        // Data1, Data2, and Data3 are encoded as big-endian for Variant-1
        u[0] = uint8_t(g.Data1 >> 24);
        u[1] = uint8_t(g.Data1 >> 16);
        u[2] = uint8_t(g.Data1 >> 8);
        u[3] = uint8_t(g.Data1);

        u[4] = uint8_t(g.Data2 >> 8);
        u[5] = uint8_t(g.Data2);

        u[6] = uint8_t(g.Data3 >> 8);
        u[7] = uint8_t(g.Data3);

        u[8] = g.Data4[0];
        u[9] = g.Data4[1];
        u[10] = g.Data4[2];
        u[11] = g.Data4[3];
        u[12] = g.Data4[4];
        u[13] = g.Data4[5];
        u[14] = g.Data4[6];
        u[15] = g.Data4[7];
    }

    // Converts from Variant-1 UUID byte-array encoding to structured representation (guid)
    // by shifting the bytes into their corresponding unsigned integer representations.
    constexpr inline guid decode_guid_from_variant1(const uint8_t (&u)[16]) {
        return {// Data1, Data2, and Data3 are encoded as big-endian for Variant-1
                u[0] * 0x01000000u + u[1] * 0x00010000u + u[2] * 0x00000100u + u[3],

                uint16_t(u[4] * 0x0100u + u[5]),

                uint16_t(u[6] * 0x0100u + u[7]),

                {
                    u[8],
                    u[9],
                    u[10],
                    u[11],
                    u[12],
                    u[13],
                    u[14],
                    u[15],
                }};
    }

    // Converts from structured representation (guid) to Variant-2 UUID byte-array encoding
    // by extracting the bytes out of their corresponding unsigned integer representations.
    constexpr inline void encode_guid_as_variant2(const guid& g, uint8_t (&u)[16]) {
        // Data1, Data2, and Data3 are encoded as little-endian for Variant-2
        u[0] = uint8_t(g.Data1);
        u[1] = uint8_t(g.Data1 >> 8);
        u[2] = uint8_t(g.Data1 >> 16);
        u[3] = uint8_t(g.Data1 >> 24);

        u[4] = uint8_t(g.Data2);
        u[5] = uint8_t(g.Data2 >> 8);

        u[6] = uint8_t(g.Data3);
        u[7] = uint8_t(g.Data3 >> 8);

        u[8] = g.Data4[0];
        u[9] = g.Data4[1];
        u[10] = g.Data4[2];
        u[11] = g.Data4[3];
        u[12] = g.Data4[4];
        u[13] = g.Data4[5];
        u[14] = g.Data4[6];
        u[15] = g.Data4[7];
    }

    // Converts from Variant-2 UUID byte-array encoding to structured representation (guid)
    // by shifting the bytes into their corresponding unsigned integer representations.
    constexpr inline guid decode_guid_from_variant2(const uint8_t (&u)[16]) {
        return {// Data1, Data2, and Data3 are encoded as little-endian for Variant-2
                u[0] + u[1] * 0x00000100u + u[2] * 0x00010000u + u[3] * 0x01000000u,

                uint16_t(u[4] + u[5] * 0x0100u),

                uint16_t(u[6] + u[7] * 0x0100u),

                {
                    u[8],
                    u[9],
                    u[10],
                    u[11],
                    u[12],
                    u[13],
                    u[14],
                    u[15],
                }};
    }

    namespace details {
        template <typename T>
        class guid_parser {
        public:
            struct parse_error {
                const char* const message; // The reason that parsing failed; must be a string literal
                const T* parsePoint;       // A pointer to the point in the string that failed to parse
            };

        private:
            typedef _Inout_                                 // Pointer to buffer (cursor itself) is read, and incremented
                _Deref_pre_z_                               // Buffer itself is initially null terminated
                    _When_(**(_Curr_) != 0, _Deref_post_z_) // Buffer remains null terminated unless it matches the null terminator
                const T** BufferCursor;

            static constexpr bool TryParseChar(BufferCursor str, T c) {
                if (**str == c) {
                    ++(*str);
                    return true;
                }
                return false;
            }

            static constexpr void ParseChar(BufferCursor str, T c) {
                if (!TryParseChar(str, c)) {
                    throw parse_error{"Improperly formatted guid!", *str};
                }
            }

            static constexpr uint8_t ParseHalfByte(BufferCursor str) {
                const T c = *(*str)++;
                return ((c >= '0' && c <= '9')
                            ? (c - '0')
                            : ((c >= 'a' && c <= 'f')
                                   ? (c - 'a' + 10)
                                   : ((c >= 'A' && c <= 'F') ? (c - 'A' + 10)
                                                             : throw parse_error{"Invalid hexadecimal character!", *str - 1})));
            }

            static constexpr uint8_t ParseByte(BufferCursor str) {
                const uint8_t first = ParseHalfByte(str);
                const uint8_t second = ParseHalfByte(str);
                return (first << 4) | second;
            }

            static constexpr uint16_t ParseUInt16(BufferCursor str) {
                const uint8_t upper = ParseByte(str);
                const uint8_t lower = ParseByte(str);
                return (upper << 8) | lower;
            }

            static constexpr uint32_t ParseUInt32(BufferCursor str) {
                const uint16_t high = ParseUInt16(str);
                const uint16_t low = ParseUInt16(str);
                return (high << 16) | low;
            }

        public:
            static constexpr guid parse(_In_z_ const T* str) {
                const bool hasBraces = TryParseChar(&str, '{');
                const guid parsed{ParseUInt32(&str),
                                  (ParseChar(&str, '-'), ParseUInt16(&str)),
                                  (ParseChar(&str, '-'), ParseUInt16(&str)),
                                  {
                                      (ParseChar(&str, '-'), ParseByte(&str)),
                                      ParseByte(&str),
                                      (ParseChar(&str, '-'), ParseByte(&str)),
                                      ParseByte(&str),
                                      ParseByte(&str),
                                      ParseByte(&str),
                                      ParseByte(&str),
                                      ParseByte(&str),
                                  }};

                if (hasBraces) {
                    ParseChar(&str, '}');
                }
                ParseChar(&str, '\0');

                return parsed;
            }
        };
    } // namespace details

    // Formats a guid value into a string of the form {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}. Used like:
    //
    // printf("%s\n", formatted_guid(g).value);
    //
    template <typename T = char>
    class formatted_guid {
    public:
        const T value[39];

        constexpr formatted_guid(const guid& g)
            : value{'{',
                    First(Upper(High(g.Data1))),
                    Second(Upper(High(g.Data1))),
                    First(Lower(High(g.Data1))),
                    Second(Lower(High(g.Data1))),
                    First(Upper(Low(g.Data1))),
                    Second(Upper(Low(g.Data1))),
                    First(Lower(Low(g.Data1))),
                    Second(Lower(Low(g.Data1))),
                    '-',
                    First(Upper(g.Data2)),
                    Second(Upper(g.Data2)),
                    First(Lower(g.Data2)),
                    Second(Lower(g.Data2)),
                    '-',
                    First(Upper(g.Data3)),
                    Second(Upper(g.Data3)),
                    First(Lower(g.Data3)),
                    Second(Lower(g.Data3)),
                    '-',
                    First(g.Data4[0]),
                    Second(g.Data4[0]),
                    First(g.Data4[1]),
                    Second(g.Data4[1]),
                    '-',
                    First(g.Data4[2]),
                    Second(g.Data4[2]),
                    First(g.Data4[3]),
                    Second(g.Data4[3]),
                    First(g.Data4[4]),
                    Second(g.Data4[4]),
                    First(g.Data4[5]),
                    Second(g.Data4[5]),
                    First(g.Data4[6]),
                    Second(g.Data4[6]),
                    First(g.Data4[7]),
                    Second(g.Data4[7]),
                    '}',
                    '\0'} {
        }

        struct format_error {
            const char* const message; // must be a string literal
        };

    private:
        static constexpr T HalfByteToChar(_In_range_(0, 15) uint8_t hb) {
            if (hb >= 16) {
                throw format_error{"Half-byte out of range!"};
            }
            return hb + ((hb < 10) ? '0' : ('a' - 10));
        }

        static constexpr uint16_t High(uint32_t ui32) {
            return ui32 >> 16;
        }
        static constexpr uint16_t Low(uint32_t ui32) {
            return ui32 & 0xffff;
        }
        static constexpr uint8_t Upper(uint16_t ui16) {
            return ui16 >> 8;
        }
        static constexpr uint8_t Lower(uint16_t ui16) {
            return ui16 & 0xff;
        }
        static constexpr T First(uint8_t ui8) {
            return HalfByteToChar(ui8 >> 4);
        }
        static constexpr T Second(uint8_t ui8) {
            return HalfByteToChar(ui8 & 0xf);
        }
    };

    using wformatted_guid = formatted_guid<wchar_t>;

    // Parses a string of the form {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} into a guid value (with or without braces).
    // Supports both compile-time and runtime parsing.
    template <typename T>
    inline constexpr guid parse_guid(_In_z_ const T* str) {
        return details::guid_parser<T>::parse(str);
    }

    inline guid create_guid() noexcept {
#ifdef _WIN32
        guid result;
        if (FAILED(CoCreateGuid(&result))) {
            abort();
        }
        return result;
#else
        const int fd = open("/proc/sys/kernel/random/uuid", O_RDONLY);
        if (fd < 0) {
            abort();
        }

        char guidString[37];
        if (read(fd, guidString, sizeof(guidString)) != sizeof(guidString)) {
            abort();
        }
        if (guidString[sizeof(guidString) - 1] != '\n') {
            abort();
        }
        guidString[sizeof(guidString) - 1] = '\0';

        if (close(fd) != 0) {
            abort();
        }

        return parse_guid(guidString);
#endif
    }
} // namespace mrf

#ifndef MRFOUNDATION_NO_STL
namespace mrf {
    template <typename T = std::string>
    inline T to_string(const guid& g) {
        return formatted_guid<typename T::value_type>(g).value;
    }

    inline std::wstring to_wstring(const guid& g) {
        return to_string<std::wstring>(g);
    }
} // namespace mrf

MRFOUNDATION_OPERATOR_NS_BEGIN(mrf)
inline std::ostream& operator<<(std::ostream& out, const mrf::guid& g) {
    return out << mrf::formatted_guid(g).value;
}

inline std::wostream& operator<<(std::wostream& out, const mrf::guid& g) {
    return out << mrf::wformatted_guid(g).value;
}
MRFOUNDATION_OPERATOR_NS_END

namespace mrf::details {
    template <size_t sizeof_size_t>
    struct guid_hash_traits_impl;

    template <>
    struct guid_hash_traits_impl<8> {
        static inline const uint64_t fnv_offset_basis = 14695981039346656037ULL;
        static inline const uint64_t fnv_prime = 1099511628211ULL;
    };

    template <>
    struct guid_hash_traits_impl<4> {
        static inline const uint32_t fnv_offset_basis = 2166136261U;
        static inline const uint32_t fnv_prime = 16777619U;
    };

    using guid_hash_traits = guid_hash_traits_impl<sizeof(size_t)>;
} // namespace mrf::details

namespace std {
    // Implementation adapted from cppwinrt
    template <>
    struct hash<mrf::guid> {
        size_t operator()(mrf::guid const& value) const noexcept {
            size_t result = mrf::details::guid_hash_traits::fnv_offset_basis;

            uint8_t const* const buffer = reinterpret_cast<uint8_t const*>(&value);
            for (size_t next = 0; next < sizeof(value); ++next) {
                result ^= buffer[next];
                result *= mrf::details::guid_hash_traits::fnv_prime;
            }

            return result;
        }
    };
} // namespace std
#endif

#endif
