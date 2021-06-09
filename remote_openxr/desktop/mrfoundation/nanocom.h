//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Standard definition of nano-COM ABI conventions, compatible and interchangable with the Windows
// definition from unknwn.h and weakreference.h. Windows consumers reference the native Windows types
// via typedef in lieu of defining separate types in this library. This enables using libraries like
// cppwinrt, WIL and WRL in Windows-only codebases.
//
// In order to ensure portability and ABI stability, no dependecies are taken outside the CRT and STL.
// STL usage is limited to code compiled into consumers, and is not used on ABI boundaries. It may
// be conditionally disabled by defining MRFOUNDATION_NO_STL.
//
// Nano-COM is a convention for defining an ABI between components which may be built and/or deployed
// separately. It has been distilled from the broader definition of COM on Windows, discarding most of
// the runtime aspects of that system including type activation, apartments, marshaling, etc.
// Nano-COM comprises:
//
//   - HRESULT: Venerable convention for expressing "exceptions" on an ABI via return-value
//   - IID: Association of guid values with interface types, used for dynamic casting
//   - IUnknown: Intrusive ref-counting, and dynamic casting
//   - Memory allocation: Alloc/free functions for managing pointers to memory returned by methods.
//
// In addition, this definition of nano-COM incorporates a convention established by WinRT for supporting
// "weak references" to objects (ala std::shared_ptr/std::weak_ptr).
//
//   - IWeakReference[Source]: Convention for maintaining access to an object without keeping it alive
//
// A nano-COM interface is a pure-virtual C++ class which derives from IUnknown. Its methods may return
// any ABI-stable type including void. If the method is required to express "exceptions" (e.g. for invalid
// parameters or other usage errors), it must express these by returning an HRESULT. Typical usage should
// translate failed HRESULTs into exceptions, which are not expected to be handled. Non-exceptional failure
// modes should typically be expressed with a bool/enum/pointer result which must be inspected by the caller.
//
// Each interface must have a unique IID value associated with it. If the signature of an interface ever
// changes, a new IID must be assigned to avoid obscure runtime failures due to mis-matched vtables between
// the consumer of the interface and its implementation. (Note that out-of-sync consumers will still encounter
// failures from IUnknown::QueryInterface, so changing existing interfaces should be done with care.)
//
// All interfaces derive from IUnknown which is used to manage the lifetime of the object. A newly created
// object will typically have a reference count of one, and the consumer is responsible to release that
// reference by calling IUnknown::Release. Additional references may be taken by calling IUnknown::AddRef,
// and later released by calling IUnknown::Release. This pattern is amenable to creating RAII wrappers that
// automatically add and release references upon construction/destruction.
//
// An object may implement one or more interfaces, and can be "cast" to a given interface via the
// IUnknown::QueryInterface method. This is morally equivalent to a C++ dynamic_cast, and is typically
// implemented by performing a static_cast to the interface type corresponding to the specified IID.
// The caller reinterpret_casts the resulting void* to the requested interface type. Interfaces may derive
// from other interfaces, ultimately terminating on IUnknown. In this case, an implementation class must
// only derive from the most-derived interface in an inheritance chain, but its implementation of
// IUnknown::QueryInterface must return that interface's vtable offset for each of the IIDs of the types
// in the chain.
//
// When a method needs to return a pointer to raw memory (versus plain-old-data or an object reference),
// it may either directly return a const pointer to memory owned by the object, in which case the pointer
// only remains valid as long as the consumer holds a reference to the object, or it may allocate a
// buffer which the caller is responsible to free. This requires that the calling component and the
// implementation agree on a common set of alloc/free functions in a separate shared library. On Windows,
// this is provided by CoTaskMemAlloc/CoTaskMemFree from ole32.dll. On Linux-based platforms, it is
// provided by malloc/free from libc.so.
//
// Weak references are supported via the IWeakReference and IWeakReferenceSource interfaces. This permits a
// consumer of an object to maintain access to the object without keeping it alive. When the weak reference
// is resolved, the result may be null if the object has been destroyed in the meantime. The weak reference
// is implemented as a separate "tear-off" object with its own lifetime apart from the source object. As a
// historical note, earlier versions of IWeakReference used IInspectable** for the outparam, but this turned
// out to be an artificial limitation. Current versions use void** as QueryInterface does to permit any
// interface to be used.

#ifndef MRFOUNDATION_NANOCOM_H
#define MRFOUNDATION_NANOCOM_H

#include <sal.h>
#include <mrfoundation/guid.h>

#ifdef _WIN32
#define MRFOUNDATION_USING_WINDOWS_IUNKNOWN
// IUnknown declaration comes in via mrfoundation/guid.h which includes combaseapi.h
#endif

#ifndef MRFOUNDATION_NO_STL
#include <exception>
#endif

namespace mrf {
    //
    // HRESULT support
    //

#ifdef MRFOUNDATION_USING_WINDOWS_IUNKNOWN
    using hresult = ::HRESULT;
#else
    using hresult = _Return_type_success_(return >= 0) int32_t;
#endif

    // Validate fundamental type semantics as 32-bit signed integer
    static_assert(sizeof(hresult) == 4);
    static_assert(hresult(-1) < hresult(0));
    static_assert(hresult(0.5f) != 0.5f);

    namespace details {
        struct invalid_win32_error_code {};

        // Many common failure codes are defined to correspond to Win32 error codes.
        // These are transformed to HRESULT-compatible failure codes in the same way
        // as the Windows HRESULT_FROM_WIN32 macro to produce interchangable codes.
        // By definition, these codes can be at most 16-bits in size since they can
        // only occupy the lower 16-bits of a corresponding HRESULT.
        constexpr inline hresult failure_from_win32(uint16_t code) {
            if (code == 0) {
                throw invalid_win32_error_code();
            }                         // 0 is ERROR_SUCCESS, i.e. S_OK, which is not a failure
            return 0x80070000 | code; // HRESULT_FROM_WIN32
        }
    } // namespace details

    // Success codes start with `success_` and have positive values (high-order bit clear).
    // There should be only one! If you think you need a second success code,
    // you should probably have a domain-specific enum outparam instead.
    constexpr hresult success_ok = 0; // S_OK

    // Failure codes start with `failure_` and have negative values (high-order bit set).
    // Commonly used codes are defined below using the values of corresponding Windows HRESULTs.
    // Specific failure codes should generally not be used for control flow, but only for
    // diagnostic purposes. Instead, prefer expressing programmatically relevant failure modes
    // through a domain-specific enum outparam instead, or simplifying to a boolean result.
    constexpr hresult failure_pending = 0x8000000a;                                          // E_PENDING
    constexpr hresult failure_not_implemented = 0x80004001;                                  // E_NOTIMPL
    constexpr hresult failure_no_interface = 0x80004002;                                     // E_NOINTERFACE
    constexpr hresult failure_abort = 0x80004004;                                            // E_ABORT
    constexpr hresult failure_unspecified = 0x80004005;                                      // E_FAIL
    constexpr hresult failure_unexpected = 0x8000ffff;                                       // E_UNEXPECTED
    constexpr hresult failure_access_denied = details::failure_from_win32(5);                // ERROR_ACCESS_DENIED / E_ACCESSDENIED
    constexpr hresult failure_invalid_handle = details::failure_from_win32(6);               // ERROR_INVALID_HANDLE
    constexpr hresult failure_invalid_data = details::failure_from_win32(13);                // ERROR_INVALID_DATA
    constexpr hresult failure_out_of_memory = details::failure_from_win32(14);               // ERROR_OUTOFMEMORY / E_OUTOFMEMORY
    constexpr hresult failure_not_ready = details::failure_from_win32(21);                   // ERROR_NOT_READY
    constexpr hresult failure_bad_command = details::failure_from_win32(22);                 // ERROR_BAD_COMMAND
    constexpr hresult failure_not_supported = details::failure_from_win32(50);               // ERROR_NOT_SUPPORTED
    constexpr hresult failure_invalid_argument = details::failure_from_win32(87);            // ERROR_INVALID_PARAMETER / E_INVALIDARG
    constexpr hresult failure_insufficient_buffer = details::failure_from_win32(122);        // ERROR_INSUFFICIENT_BUFFER
    constexpr hresult failure_more_data = details::failure_from_win32(234);                  // ERROR_MORE_DATA
    constexpr hresult failure_no_more_items = details::failure_from_win32(259);              // ERROR_NO_MORE_ITEMS
    constexpr hresult failure_operation_aborted = details::failure_from_win32(995);          // ERROR_OPERATION_ABORTED
    constexpr hresult failure_io_pending = details::failure_from_win32(997);                 // ERROR_IO_PENDING
    constexpr hresult failure_not_found = details::failure_from_win32(1168);                 // ERROR_NOT_FOUND / E_NOT_SET
    constexpr hresult failure_cancelled = details::failure_from_win32(1223);                 // ERROR_CANCELLED
    constexpr hresult failure_driver_process_terminated = details::failure_from_win32(1291); // ERROR_DRIVER_PROCESS_TERMINATED
    constexpr hresult failure_device_removed = details::failure_from_win32(1617);            // ERROR_DEVICE_REMOVED
    constexpr hresult failure_not_connected = details::failure_from_win32(2250);             // ERROR_NOT_CONNECTED

    namespace details {
        template <typename T>
        inline constexpr bool is_hresult_v = false;

        template <>
        inline constexpr bool is_hresult_v<hresult> = true;

        struct type_is_not_hresult {};
    } // namespace details

    template <typename T>
    [[nodiscard]] constexpr inline bool is_success(const T&) {
        static_assert(details::is_hresult_v<T>, "Wrong type: hresult expected");
        throw details::type_is_not_hresult();
        return false;
    }

    [[nodiscard]] constexpr inline bool is_success(hresult result) {
        // Since high-order bit encodes failure, all positive values indicate success.
        return result >= 0;
    }

    template <typename T>
    [[nodiscard]] constexpr inline bool is_failure(const T&) {
        static_assert(details::is_hresult_v<T>, "Wrong type: hresult expected");
        throw details::type_is_not_hresult();
        return true;
    }

    [[nodiscard]] constexpr inline bool is_failure(hresult result) {
        return !is_success(result);
    }

    struct hresult_failure {
        template <typename T>
        constexpr hresult_failure(const T& result) noexcept
            : code(result) {
            if (!is_failure(result)) {
#ifdef MRFOUNDATION_NO_STL
                abort();
#else
                std::abort();
#endif
            }
        }

        const hresult code;
    };

    template <typename T>
    constexpr inline void throw_if_failure(const T&) {
        static_assert(details::is_hresult_v<T>, "Wrong type: hresult expected");
        throw details::type_is_not_hresult();
    }

    constexpr inline void throw_if_failure(hresult result) {
        if (is_failure(result)) {
            throw hresult_failure{result};
        }
    }

    namespace details {
        constexpr inline mrf::hresult unexpected_failure_fallback() noexcept {
            return failure_unexpected;
        }
    } // namespace details

    // The optional fallback parameter allows callers to do additional processing of exception types not known
    // to this header (or otherwise override the failure returned for unhandled exceptions).
    template <typename T = decltype(details::unexpected_failure_fallback)>
    [[nodiscard]] inline hresult current_exception_to_hresult(const T& fallback = details::unexpected_failure_fallback) noexcept {
        try {
            throw;
        } catch (const hresult_failure& ex) {
            return ex.code;
        }
#ifndef MRFOUNDATION_NO_STL
        catch (const std::bad_alloc&) {
            return failure_out_of_memory;
        } catch (const std::invalid_argument&) {
            return failure_invalid_argument;
        } catch (const std::out_of_range&) {
            return failure_not_found;
        } catch (const std::bad_function_call&) {
            return failure_not_implemented;
        } catch (const std::bad_cast&) {
            return failure_no_interface;
        } catch (const std::bad_exception&) {
            return failure_unexpected;
        }
#endif
        catch (...) {
            return fallback();
        }
    }

    //
    // IID support
    //

    namespace details {
        template <typename T, bool hasIid = false>
        struct missing_iid : guid {
            missing_iid() {
                static_assert(hasIid, "No IID has been associated with this type!");
            }
        };

        template <typename T>
        inline constexpr guid iid_v{missing_iid<T>()};
    } // namespace details

    template <typename T>
    static inline constexpr const guid& iid_of() noexcept {
        return details::iid_v<T>;
    }

    template <typename T>
    static inline constexpr const guid& iid_of(const T*) noexcept {
        return iid_of<T>();
    }

    // Generic adapter for any RAII wrapper
    template <typename T, template <typename> typename U>
    static inline constexpr const guid& iid_of(const U<T>&) noexcept {
        return iid_of<T>();
    }
} // namespace mrf

// interface declaration macros
#ifdef _WIN32
#define MRFOUNDATION_EXPORT extern "C" __declspec(dllexport)
#else
#define MRFOUNDATION_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#ifdef _WIN32
#define MRFOUNDATION_CALL __stdcall
#else
#define MRFOUNDATION_CALL
#endif

#ifdef MIDL_INTERFACE
#define MRFOUNDATION_DEFINE_INTERFACE(iid, T) MIDL_INTERFACE(iid) T
#else
#define MRFOUNDATION_DEFINE_INTERFACE(iid, T) struct T
#endif

#define MRFOUNDATION_ASSOCIATE_IID(iid, T)              \
    template <>                                         \
    inline constexpr mrf::guid mrf::details::iid_v<T> { \
        mrf::parse_guid(iid)                            \
    }

#define MRFOUNDATION_INTERFACE(iid, T)  \
    struct T;                           \
    MRFOUNDATION_ASSOCIATE_IID(iid, T); \
    MRFOUNDATION_DEFINE_INTERFACE(iid, T)

#define MRFOUNDATION_INTERFACE_NS(iid, NS, T) \
    namespace NS {                            \
        struct T;                             \
    }                                         \
    MRFOUNDATION_ASSOCIATE_IID(iid, NS::T);   \
    MRFOUNDATION_DEFINE_INTERFACE(iid, NS::T)

//
// IUnknown support
//

#ifdef MRFOUNDATION_USING_WINDOWS_IUNKNOWN
namespace mrf {
    using IUnknown = ::IUnknown;

    // Nominally, refcounts are unsigned 32-bit integers. However, Windows uses long for this rather than int,
    // and these are technically different types, so virtual method overrides won't match if they aren't consistent.
    // Therefore we define an abstracted type that always matches whatever ::IUnknown is using on Windows, and
    // is explicitly uint32_t on non-Windows.
    using refcount_type = decltype(((IUnknown*)nullptr)->AddRef());
} // namespace mrf

// Map ::IUnknown's IID into the mrf IID association system.
template <>
inline constexpr mrf::guid mrf::details::iid_v<IUnknown>{__uuidof(IUnknown)};
#else
namespace mrf {
    using refcount_type = uint32_t;
}

MRFOUNDATION_INTERFACE_NS("{00000000-0000-0000-C000-000000000046}", mrf, IUnknown) {
    virtual mrf::hresult MRFOUNDATION_CALL QueryInterface(const guid& iid, _COM_Outptr_ void** ppvObject) noexcept = 0;
    virtual mrf::refcount_type MRFOUNDATION_CALL AddRef() noexcept = 0;
    virtual mrf::refcount_type MRFOUNDATION_CALL Release(void) noexcept = 0;

    template <typename T>
    inline mrf::hresult QueryInterface(_COM_Outptr_ T * *pp) noexcept {
        return QueryInterface(iid_of(*pp), reinterpret_cast<void**>(pp));
    }
};
#endif

//
// Memory allocation support
//

namespace mrf {
    // On Windows, use CoTaskMemAlloc/CoTaskMemFree for consistency/interoperability with existing COM conventions.
    // Otherwise, use malloc/free from libc.so. Note that in both cases, it is required that the memory allocator
    // come from a shared library outside the consumer's library and the implementor's library. If it were statically
    // linked instead, memory allocated by an implementation library could not be freed by a consumer in a different
    // library since they may be referring to different heaps.

    [[nodiscard]] inline _Ret_maybenull_ void* nanocom_alloc(size_t numBytes) noexcept {
#ifdef _WIN32
        return CoTaskMemAlloc(numBytes);
#else
        return ::malloc(numBytes);
#endif
    }

    inline void nanocom_free(_In_opt_ void* p) noexcept {
#ifdef _WIN32
        CoTaskMemFree(p);
#else
        ::free(p);
#endif
    }
} // namespace mrf

//
// Weak reference support
//

// The Windows definitions from weakreference.h are are artificially constrained to require resolved interfaces to
// derive from IInspectable. In practice, this is not required, so this definition uses void** instead. The type
// defined here is ABI-compatible with ::IWeakReference and uses the same IID, but is not directly castable since
// it is a distinct type in order to remove the dependency on IInspectable. In practice, this is unlikely to cause
// problems since it is typically obtained via QueryInterface anyway, and in the event that an ::IWeakReference needs
// to be converted to/from a mrf::IWeakReference, QueryInterface or reinterpret_cast may be used.
MRFOUNDATION_INTERFACE_NS("{00000037-0000-0000-C000-000000000046}", mrf, IWeakReference)
    : mrf::IUnknown {
    // Returns success_ok with nullptr when the referenced object has been destroyed - null check is always required.
    virtual mrf::hresult MRFOUNDATION_CALL Resolve(const guid& iid, _COM_Outptr_result_maybenull_ void** objectReference) noexcept = 0;

    template <typename T>
    inline mrf::hresult Resolve(_COM_Outptr_result_maybenull_ T * *objectReference) noexcept {
        return Resolve(mrf::iid_of<T>(), reinterpret_cast<void**>(objectReference));
    }
};

MRFOUNDATION_INTERFACE_NS("{00000038-0000-0000-C000-000000000046}", mrf, IWeakReferenceSource)
    : mrf::IUnknown {
    virtual mrf::hresult MRFOUNDATION_CALL GetWeakReference(_COM_Outptr_ mrf::IWeakReference * *weakReference) noexcept = 0;
};

#endif
