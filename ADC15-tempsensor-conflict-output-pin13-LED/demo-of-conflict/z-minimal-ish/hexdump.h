// MSVC:   /std:c++14 /Zc:__cplusplus /Zc:preprocessor    (without the /Zc the language version reporting + macro work will fail!)
// clang:  -std=c++11
// gcc:    -std=c++11
//
// Note: https://learn.microsoft.com/en-us/cpp/build/reference/std-specify-language-standard-version?view=msvc-170
// MSVC2026 doesn't support going back to any C++ version before C++/14.
//
// Thanks to godbolt.org (Compiler Explorer) we've been able to speed up the ARM/SAM3=Arduino Due gcc compiler
// issues, as that compiler is an antique: GCC 4.8.3, which says it has C++/11 support, but still has bugs
// that killed our template code below!
//

#include <initializer_list>
#include <string>
#include <cstdarg>
//#include <cassert>
#include <cstring>    // memcpy et al
#include <type_traits>
#include <stdint.h>
#include <inttypes.h>
#include <utility>




//
// large-ish debug helper code chunk, ripped from another project verbatim, then tweaked.
//

// --------------------------------------------------------------------------------------------


// SAM3 compiler is stuck at the C++/11 age, so no `%b` for me.

static const char * const nibble_2_bitpattern[16] = {
    "0000", "0001", "0010", "0011",
    "0100", "0101", "0110", "0111",
    "1000", "1001", "1010", "1011",
    "1100", "1101", "1110", "1111",
};

static inline void to_bitstring_raw(char dst[9], const uint8_t byte)
{
  memcpy(dst, nibble_2_bitpattern[byte >> 4], 4);
  dst[4] = '.';
  memcpy(dst + 5, nibble_2_bitpattern[byte & 0x0F], 4);
}
static inline char * to_bitstring(char dst[9 + 1], size_t dstsize, const uint8_t byte)
{
  if (dstsize < 9 + 1) {
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  to_bitstring_raw(dst, byte);
  dst[9] = 0;
  return dst + 9;
}
static inline void to_bitstring_raw(char dst[9 + 10 + 1], const uint16_t word)
{
  to_bitstring_raw(dst, uint8_t(word >> 8));
  dst[9] = '.';
  to_bitstring_raw(dst + 10, uint8_t(word));
}
static inline char * to_bitstring(char dst[2 * 9 + 1 + 1], size_t dstsize, const uint16_t word)
{
  if (dstsize < 2 * 9 + 1 + 1) {
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  to_bitstring_raw(dst, word);
  dst[9 + 1 + 9] = 0;
  return dst + 19;
}
static inline void to_bitstring_raw(char dst[4 * 9 + 3 + 1], const uint32_t word)
{
  to_bitstring_raw(dst,  uint16_t(word >> 16));
  dst[9 + 1 + 9] = ':';
  to_bitstring_raw(dst + 9 + 1 + 9 + 1, uint16_t(word));
}
static inline char * to_bitstring(char dst[4 * 9 + 3 + 1], size_t dstsize, const uint32_t word)
{
  if (dstsize < 4 * 9 + 3 + 1) {
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  to_bitstring_raw(dst, word);
  dst[9 + 1 + 9 + 1 + 9 + 1 + 9] = 0;
  return dst + 39;
}
static inline void to_bitstring_raw(char dst[8 * 9 + 7 + 1], const uint64_t word)
{
  to_bitstring_raw(dst, uint32_t(word >> 32));
  dst[4 * 9 + 3] = ':';
  to_bitstring_raw(dst + 4 * 9 + 3 + 1, uint32_t(word));
}
static inline char * to_bitstring(char dst[8 * 9 + 7 + 1], size_t dstsize, const uint64_t word)
{
  if (dstsize < 8 * 9 + 7 + 1) {
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  to_bitstring_raw(dst, word);
  dst[8 * 9 + 7] = 0;
  return dst + 8 * 9 + 7;
}


// ---------------------------------------------------------------------------

static const char * const nibble_2_hex = "0123456789ABCDEF";

static inline void to_hexstring_raw(char dst[2 + 1], const uint8_t byte)
{
  dst[0] = nibble_2_hex[byte >> 4];
  dst[1] = nibble_2_hex[byte & 0x0F];
}
static inline char * to_hexstring(char dst[2 + 1], size_t dstsize, const uint8_t byte)
{
  if (dstsize < 2 + 1) {
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  to_hexstring_raw(dst, byte);
  dst[2] = 0;
  return dst + 2;
}
static inline void to_hexstring_raw(char dst[2 * 2 + 1], const uint16_t word)
{
  to_hexstring_raw(dst, uint8_t(word >> 8));
  to_hexstring_raw(dst + 2, uint8_t(word));
}
static inline char * to_hexstring(char dst[2 * 2 + 1], size_t dstsize, const uint16_t word)
{
  if (dstsize < 2 * 2 + 1) {
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  to_hexstring_raw(dst, word);
  dst[4] = 0;
  return dst + 4;
}
static inline void to_hexstring_raw(char dst[4 * 2 + 1], const uint32_t word)
{
  to_hexstring_raw(dst, uint16_t(word >> 16));
  to_hexstring_raw(dst + 4, uint16_t(word));
}
static inline char * to_hexstring(char dst[4 * 2 + 1], size_t dstsize, const uint32_t word)
{
  if (dstsize < 4 * 2 + 1) {
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  to_hexstring_raw(dst, word);
  dst[8] = 0;
  return dst + 8;
}
static inline void to_hexstring_raw(char dst[8 * 2 + 1], const uint64_t word)
{
  to_hexstring_raw(dst, uint32_t(word >> 32));
  to_hexstring_raw(dst + 4, uint32_t(word));
}
static inline char * to_hexstring(char dst[8 * 2 + 1], size_t dstsize, const uint64_t word)
{
  if (dstsize < 8 * 2 + 1) {
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  to_hexstring_raw(dst, word);
  dst[2 * 8] = 0;
  return dst + 2 * 8;
}


// ----------------------------------------------------------------------------------------------------


template <typename T,
          typename std::enable_if<
            std::is_floating_point<T>::value,
            bool
          >::type = true
        >
const char *display(char *dst, size_t dstsize, T v) {
  if (dstsize < 12) {
overflow:    
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  else {
    auto wl = snprintf(dst, dstsize, "%lg", double(v));
    if (wl >= dstsize || wl < 1) {
      goto overflow;
    }
    return dst + wl;
  }
}


template <typename T,
          typename std::enable_if<
            std::is_same<T, bool>::value,
            bool
          >::type = true
        >
const char *display(char *dst, size_t dstsize, T v) {
  if (dstsize < 8 + 1) {
overflow:    
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  else {
    if (v) {
      strcpy(dst, "TRUE(1)");
      return dst + 7;
    }
    else {
      strcpy(dst, "FALSE(0)");
      return dst + 8;
    }
  }
}


template <typename T,
          typename std::enable_if<
            ((std::is_arithmetic<T>::value && std::is_signed<T>::value) || std::is_enum<T>::value)
            && !std::is_same<T, bool>::value
            && !std::is_floating_point<T>::value,
            bool
          >::type = true
        >
const char *display(char *dst, size_t dstsize, T v) {
  if (dstsize < sizeof(T) * 3 + 2) {
overflow:    
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  else {
    constexpr const unsigned int w = sizeof(T);
    if (w == 8) {
      int64_t v64 = int64_t(v);  
      auto wl = snprintf(dst, dstsize, "%" PRId64, v64);
      if (wl >= dstsize || wl < 1) {
        goto overflow;
      }
      return dst + wl;
    }
    else {
      auto wl = snprintf(dst, dstsize, "%d", int(v));
      if (wl >= dstsize || wl < 1) {
        goto overflow;
      }
      return dst + wl;
    }
  }
}


template <typename T,
          typename std::enable_if<
            (std::is_arithmetic<T>::value && std::is_unsigned<T>::value)
            && !std::is_enum<T>::value        // we assume all enums are SIGNED (gcc 4 for SAM3/Due failed to match either is_signed or is_unsigned for an enum type)
            && !std::is_same<T, bool>::value
            && !std::is_floating_point<T>::value,
            bool
          >::type = true
        >
const char *display(char *dst, size_t dstsize, T v) {
  if (dstsize < sizeof(T) * 3 + 2) {
overflow:    
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  else {
    constexpr const unsigned int w = sizeof(T);
    if (w == 8) {
      uint64_t v64 = uint64_t(v);  
      auto wl = snprintf(dst, dstsize, "%" PRIu64, v64);
      if (wl >= dstsize || wl < 1) {
        goto overflow;
      }
      return dst + wl;
    }
    else {
      auto wl = snprintf(dst, dstsize, "%u", (unsigned int)(v));
      if (wl >= dstsize || wl < 1) {
        goto overflow;
      }
      return dst + wl;
    }
  }
}


// helpers:
static char * raw_display_as_hexbin_uint(char *dst, uint8_t v, const bool show_as_binary = false) {
  dst[0] = '$';
  // --> '$XX(B1111.1111)'
  to_hexstring_raw(dst + 1, v);
  if (!show_as_binary) {
    dst[3] = 0;
    return dst + 3;
  }
  else {
    dst[3] = '(';
    dst[4] = 'B';
    to_bitstring_raw(dst + 5, v);
    dst[5 + 9] = ')';
    dst[5 + 9 + 1] = 0;
    return dst + 5 + 9 + 1;
  }
}
static inline char * raw_display_as_hexbin_uint(char *dst, uint16_t v, const bool show_as_binary = false) {
  dst[0] = '$';
  // --> '$XXXX(B1111.1111.1111.1111)'
  to_hexstring_raw(dst + 1, v);
  if (!show_as_binary) {
    dst[5] = 0;
    return dst + 5;
  }
  else {
    dst[5] = '(';
    dst[6] = 'B';
    to_bitstring_raw(dst + 7, v);
    dst[7 + 16 + 3] = ')';
    dst[7 + 16 + 3 + 1] = 0;
    return dst + 7 + 16 + 3 + 1;
  }
}
static inline char * raw_display_as_hexbin_uint(char *dst, uint32_t v, const bool show_as_binary = false) {
  dst[0] = '$';
  // --> '$XXXX:XXXX(B1111.1111.1111.1111:1111.1111.1111.1111)'
  to_hexstring_raw(dst + 1, uint16_t(v >> 16));
  dst[5] = ':';
  to_hexstring_raw(dst + 6, uint16_t(v));
  if (!show_as_binary) {
    dst[10] = 0;
    return dst + 10;
  }
  else {
    dst[10] = '(';
    dst[11] = 'B';
    to_bitstring_raw(dst + 12, v);
    dst[12 + 32 + 7] = ')';
    dst[12 + 32 + 8] = 0;
    return dst + 12 + 32 + 8;
  }
}
static inline char * raw_display_as_hexbin_uint(char *dst, uint64_t v, const bool show_as_binary = false) {
  dst[0] = '$';
  // --> '$XXXX:XXXX:XXXX:XXXX(B1111.1111.1111.1111:1111.1111.1111.1111:1111.1111.1111.1111:1111.1111.1111.1111)'
  to_hexstring_raw(dst + 1, uint16_t(v >> 48));
  dst[5] = ':';
  to_hexstring_raw(dst + 6, uint16_t(v >> 32));
  dst[10] = ':';
  to_hexstring_raw(dst + 11, uint16_t(v >> 16));
  dst[15] = ':';
  to_hexstring_raw(dst + 16, uint16_t(v));
  if (!show_as_binary) {
    dst[20] = 0;
    return dst + 20;
  }
  else {
    dst[20] = '(';
    dst[21] = 'B';
    to_bitstring_raw(dst + 22, v);
    dst[22 + 64 + 15] = ')';
    dst[22 + 64 + 16] = 0;
    return dst + 22 + 64 + 16;
  }
}


template <typename T,
          typename std::enable_if<
            (std::is_arithmetic<T>::value || std::is_enum<T>::value)
            && std::is_unsigned<T>::value
            && !std::is_same<T, bool>::value
            && !std::is_floating_point<T>::value,
            bool
          >::type = true
        >
const char *displayHex(char *dst, size_t dstsize, T v, const bool show_as_binary = false) {
  if (dstsize < 1 + sizeof(T) * 3 + (show_as_binary ? sizeof(T) * 9 + 3 : 0) + 1) {
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  else {
    constexpr const unsigned int w = sizeof(T);
    if (w == 1) {
      dst = raw_display_as_hexbin_uint(dst, uint8_t(v), show_as_binary);
    }
    else if (w == 2) {
      dst = raw_display_as_hexbin_uint(dst, uint16_t(v), show_as_binary);
    }
    else if (w == 8) {
      dst = raw_display_as_hexbin_uint(dst, uint64_t(v), show_as_binary);
    }
    else /* if (w == 4) */ {
      dst = raw_display_as_hexbin_uint(dst, uint32_t(v), show_as_binary);
    }
    return dst;
  }
}



#if !defined(__cpp_lib_is_null_pointer)
// https://en.cppreference.com/cpp/types/is_null_pointer

// DO NOT define std::is_null_pointer for GCC 4.9.1 / 4.9.0 as just those two versions have it, yet do not define the feature macro! :-((
#if !defined(__GNUC__) || (!(__GNUC__ == 4 && __GNUC_MINOR__ == 9 && __GNUC_PATCHLEVEL__ >= 0 && __GNUC_PATCHLEVEL__ <= 1))

namespace std {

  template<typename>
    struct gho__is_null_pointer_helper
    : public false_type { };

  template<>
    struct gho__is_null_pointer_helper<std::nullptr_t>
    : public true_type { };

  // is_null_pointer (LWG 2247).
  template<typename _Tp>
    struct is_null_pointer
    : public gho__is_null_pointer_helper<typename remove_cv<_Tp>::type>::type
    { };

}

#endif // GCC check

// In libc++, std::is_null_pointer is not available in C++11 mode.
#endif



static_assert(std::is_null_pointer<decltype(nullptr)>::value                         , "uh-oh!");
static_assert(!std::is_null_pointer<int*>::value                                     , "uh-oh!");
static_assert(!std::is_pointer<decltype(nullptr)>::value                             , "uh-oh!");
static_assert(std::is_pointer<int*>::value                                           , "uh-oh!");

template <typename T = void,
          typename std::enable_if<
            std::is_null_pointer<T>::value,
            bool
          >::type = true
        >
const char *display(char *dst, size_t dstsize, T v) {
  if (dstsize < 7 + 1) {
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  else {
    strcpy(dst, "(null)");
    return dst + 6;
  }
}


// helper:
static inline unsigned int str_quotes_count(const char *str, const char * const quotes = "\"") {
  unsigned int cnt = 0;
  //assert(str != nullptr);
  do {
    str = strpbrk(str, quotes);           // find separator
    if (str) {
      auto l = strspn(str, quotes);       // skip separator(s)
      cnt += l;
      str += l;
    }
  } while(str && *str);
  return cnt;    
}

const char *raw_display_string(char *dst, size_t dstsize, const char *s, bool quoted, bool allow_clipping) {
  unsigned int qcnt = 0;
  if (!s) {
    s = "(null)";
    quoted = false;
  } else {
    qcnt = str_quotes_count(s);
  }
  auto len = strnlen(s, dstsize /* don't care; we'll crop the text if we have to... */ );

  if ((allow_clipping ? dstsize < 80 : true) && dstsize < len + qcnt + 2 + 1) {
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  else {
    if (!quoted) {
      if (len < dstsize) {
        strcpy(dst, s);
        return dst + len;
      }
#ifdef DEBUG
      assert(dstsize > 20 + sizeof("(...continued)") + 1);
#endif
      auto cl = dstsize - (sizeof("(...continued)") + 1);
      memcpy(dst, s, cl);
      dst += cl;
      strcpy(dst, "(...continued)");
      return dst + sizeof("(...continued)") - 1;
    }
    else {
      if (len + qcnt + 2 < dstsize) {
        *dst++ = '"';
        if (qcnt == 0) {
          memcpy(dst, s, len);
          dst += len;
          *dst++ = '"';
          *dst = 0;
          return dst;
        }
        else {
          for (;;) {
            char c = *s++;
            if (c != '"') {
              *dst++ = c;
            }
            else if (c == 0) {
              *dst++ = '"';
              *dst = 0;
              return dst;
            }
            else {
              // duplicate quote to 'escape':
              *dst++ = c;
              *dst++ = c;
            }
          }
        }
      }
      else {
#ifdef DEBUG
        assert(dstsize > 20 + sizeof("(...continued)") + 2);
#endif
        auto cl = dstsize - (sizeof("(...continued)") + 2);

        *dst++ = '"';
        dstsize--;
        dstsize -= cl;
        for (decltype(cl) i = 0; i < cl && *s != 0; i++) {
          char c = *s++;
          if (c != '"') {
            *dst++ = c;
          }
          else {
            // duplicate quote to 'escape':
            *dst++ = c;
            *dst++ = c;
            dstsize--;
            cl--;
          }
        }
        *dst++ = '"';
        if (*s == 0) {
          *dst = 0;
        }
        else {
          strcpy(dst, "(...continued)");
          dst += sizeof("(...continued)") - 1;
        }
        return dst;
      }
    }
  }
}


template <typename T,
          typename std::enable_if<
            (
                std::is_assignable<const char *, T>::value
            ),
            bool
          >::type = true
        >
const char *display(char *dst, size_t dstsize, const T &v, bool quoted = false, bool allow_clipping = true) {
  const char *s = v;
  return raw_display_string(dst, dstsize, s, quoted, allow_clipping);
}


template <typename T,
          typename std::enable_if<
            (
                std::is_assignable<char *, T>::value
            ),
            bool
          >::type = true
        >
const char *display(char *dst, size_t dstsize, T &v, bool quoted = false, bool allow_clipping = true) {
  const char *s = v;
  return raw_display_string(dst, dstsize, s, quoted, allow_clipping);
}


template <typename T,
          typename std::enable_if<
            (
              std::is_base_of<const char *, T>::value
            ),
            bool
          >::type = true
        >
const char *display(char *dst, size_t dstsize, const T &v, bool quoted = false, bool allow_clipping = true) {
  const char *s = v;
  return raw_display_string(dst, dstsize, s, quoted, allow_clipping);
}


template <typename T,
          typename std::enable_if<
            (
              std::is_base_of<char *, T>::value
            ),
            bool
          >::type = true
        >
const char *display(char *dst, size_t dstsize, T &v, bool quoted = false, bool allow_clipping = true) {
  const char *s = v;
  return raw_display_string(dst, dstsize, s, quoted, allow_clipping);
}


// the flavours above DO NOT match the `const char *` or `{const} char[]` literal types...    :-S

template <typename T,
          typename std::enable_if<
            (
              std::is_same<const char *, T>::value
            ),
            bool
          >::type = true
        >
const char *display(char *dst, size_t dstsize, const T &v, bool quoted = false, bool allow_clipping = true) {
  const char *s = v;
  return raw_display_string(dst, dstsize, s, quoted, allow_clipping);
}


template <typename T,
          typename std::enable_if<
            (
              std::is_same<char *, T>::value
            ),
            bool
          >::type = true
        >
const char *display(char *dst, size_t dstsize, T &v, bool quoted = false, bool allow_clipping = true) {
  const char *s = v;
  return raw_display_string(dst, dstsize, s, quoted, allow_clipping);
}


#if 01 // the `const char *` argument types won't match otherwise!   :-S

template <typename T,
          typename std::enable_if<
            (
              std::is_convertible<T, const char *>::value
              && ! std::is_same<char *, T>::value
              && ! std::is_same<const char *, T>::value
              && ! std::is_null_pointer<T>::value
            ),
            bool
          >::type = true
        >
const char *display(char *dst, size_t dstsize, const T &v, bool quoted = false, bool allow_clipping = true) {
  const char *s = v;
  return raw_display_string(dst, dstsize, s, quoted, allow_clipping);
}

#endif








static_assert(decltype(std::true_type())()                                                                           , "uh-oh!");
            //
            // NOTE: std::enable_if<T> only has a conditionally available boolean ::type member,
            // so we turn that into a value by using `::type{true}`
            //
static_assert(std::enable_if<std::true_type{}, bool>::type{true}                                                     , "uh-oh!");
static_assert(std::enable_if<true, bool>::type{true}                                                                 , "uh-oh!");





// Primary template (default: false)
//template <typename T>
//struct has_cstr_member_function : std::false_type {};

#if !defined(__GNUC__) || (__GNUC__ > 5 || (__GNUC__ == 5 && __GNUC_MINOR__ >= 5))

// Specialization: Check for member function "c_str" with signature `const char *c_str(void)`
template <typename T>
struct has_cstr_member_function {
private:
    // Helper 1: Substitute T::func() and check return type
    template <typename U>
    static auto test(int) -> decltype(
            std::is_function<decltype(std::declval<U>().c_str)>::value,
            // Attempt to call U::func
            // Validate return type matches `const char *`
            std::is_convertible<decltype(std::declval<U>().c_str()), const char *>::value,
            std::true_type{}
        ) ;

    // Helper 2: Fallback
    template <typename U>
    static auto test(...) -> std::false_type ;

public:
    static constexpr bool value = decltype(test<T>(0)){};
};

#else

// Special implementation for GCC 4.9.x / GCC <= 5.4, all of which b0rk on the C++/11 code above...  :-(

// Define a template class 'check' to test for the existence of 'T::func'
template <typename T> 
class has_cstr_member_function {
    // Define two types of character arrays, 'yes' and 'no',
    // for SFINAE test
    typedef char yes[1];
    typedef char no[2];

    // Test if class T has a member function named 'func'
    // If T has 'func', this version of test() is chosen
    template <typename C>
    static yes& test(decltype(&C::c_str));

    // Fallback test() function used if T does not have
    // 'func'
    template <typename> static no& test(...);

public:
    // Static constant 'value' becomes true if T has 'func',
    // false otherwise The comparison is based on the size
    // of the return type from test()
    static const bool value
        = sizeof(test<T>(0)) == sizeof(yes);
};

#endif






template <typename T,
          typename std::enable_if<
            !std::is_convertible<T, const char *>::value &&
            has_cstr_member_function<T>::value,
            bool
          >::type = false
        >
const char *display(char *dst, size_t dstsize, const T &v, bool quoted = false, bool allow_clipping = true) {
  return raw_display_string(dst, dstsize, v.c_str(), quoted, allow_clipping);
}


#if 01

template <typename T,
          typename std::enable_if<
            !(std::is_arithmetic<T>::value ||
              std::is_enum<T>::value ||
              std::is_assignable<const char *, T>::value ||
              std::is_convertible<T, const char *>::value ||
              std::is_same<const char *, T>::value ||
              std::is_assignable<char *, T>::value ||
              std::is_convertible<T, char *>::value ||
              std::is_same<char *, T>::value ||
              has_cstr_member_function<T>::value ||
              std::is_null_pointer<T>::value
             ),
            bool
          >::type = true
        >
const char *display(char *dst, size_t dstsize, const T *v) {
  const void *p = (const void *)v;
  if (dstsize < 10 + 1) {
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
      return dst + 1;
    }
    return dst;
  }
  else {
    if (!p) {
      strcpy(dst, "(null)");
      return dst + 6;
    }
    dst[0] = '0';
    dst[1] = 'x';
    // --> '0xDEADBEAF'
    to_hexstring_raw(dst + 2, uint32_t(intptr_t(p)));
    dst[2 + 8] = 0;
    return dst + 2 + 8;
  }
}

#endif





























template <unsigned int SIZE>
class StringBuffer {
  char buf[SIZE]            {};
  unsigned int fill         {0};
  
public:
  StringBuffer() {}
  ~StringBuffer() = default;

  void append_separator() {
    if (fill > 0 && fill < SIZE - 1) {
       buf[fill++] = ' ';
       buf[fill] = 0; 
    }
  }

  void append_sentinel() {
    if (fill > 0 && fill < SIZE - 1) {
       buf[fill++] = '\n';
       buf[fill] = 0; 
    }
  }

  template <typename T>
  StringBuffer& operator | (T v) {
    append_separator();
    const char *dst = displayHex(buf + fill, SIZE - fill, v, true /* show_as_binary */ );
    fill = dst - buf;
    return *this;
  }

  template <typename T>
  StringBuffer& operator << (T v) {
    append_separator();
    const char *dst = display(buf + fill, SIZE - fill, v);
    fill = dst - buf;
    return *this;
  }

  template <typename T,
          typename std::enable_if<
            std::is_reference<T>::value,
            bool
          >::type = true
      >
  StringBuffer& operator << (const T &v) {
    append_separator();
    const char *dst = display(buf + fill, SIZE - fill, v);
    fill = dst - buf;
    return *this;
  }

  template <typename T,
          typename std::enable_if<
            std::is_pointer<T>::value,
            bool
          >::type = true
      >
  StringBuffer& operator << (const T *v) {
    append_separator();
    const char *dst = display(buf + fill, SIZE - fill, v);
    fill = dst - buf;
    return *this;
  }

  /* (const char *) */ operator const char *() {
    // ... and once fetched from here, RESET the fill:
    append_sentinel();
    fill = 0;
    return buf;
  }
};









/*
Sketch uses 39660 bytes (7%) of program storage space. Maximum is 524288 bytes.
Global variables use 3076 bytes (3%) of dynamic memory, leaving 95228 bytes for local variables. Maximum is 98304 bytes.
*/

