/* utils.hpp
Misc utilities
(C) 2015 Niall Douglas http://www.nedprod.com/
File Created: Dec 2015


Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef BOOST_AFIO_UTILS_H
#define BOOST_AFIO_UTILS_H

#include "config.hpp"

BOOST_AFIO_V2_NAMESPACE_BEGIN

//! Utility routines often useful when using AFIO
namespace utils
{
  /*! \brief Returns the page sizes of this architecture which is useful for calculating direct i/o multiples.

  \param only_actually_available Only return page sizes actually available to the user running this process
  \return The page sizes of this architecture.
  \ingroup utils
  \complexity{Whatever the system API takes (one would hope constant time).}
  \exceptionmodel{Any error from the operating system or std::bad_alloc.}
  */
  BOOST_AFIO_HEADERS_ONLY_FUNC_SPEC std::vector<size_t> page_sizes(bool only_actually_available = true) noexcept;

  /*! \brief Returns a reasonable default size for page_allocator, typically the closest page size from
  page_sizes() to 1Mb.

  \return A value of a TLB large page size close to 1Mb.
  \ingroup utils
  \complexity{Whatever the system API takes (one would hope constant time).}
  \exceptionmodel{Any error from the operating system or std::bad_alloc.}
  */
  inline size_t file_buffer_default_size() noexcept
  {
    static size_t size;
    if (!size)
    {
      std::vector<size_t> sizes(page_sizes(true));
      for (auto &i : sizes)
        if (i >= 1024 * 1024)
        {
          size = i;
          break;
        }
      if (!size)
        size = 1024 * 1024;
    }
    return size;
  }

  /*! \brief Fills the buffer supplied with cryptographically strong randomness. Uses the OS kernel API.

  \param buffer A buffer to fill
  \param bytes How many bytes to fill
  \ingroup utils
  \complexity{Whatever the system API takes.}
  \exceptionmodel{Any error from the operating system.}
  */
  BOOST_AFIO_HEADERS_ONLY_FUNC_SPEC void random_fill(char *buffer, size_t bytes);

  /*! \brief Converts a number to a hex string. Out buffer can be same as in buffer.

  Note that the character range used is a 16 item table of:

  0123456789abcdef

  This lets one pack one byte of input into two bytes of output.

  \ingroup utils
  \complexity{O(N) where N is the length of the number.}
  \exceptionmodel{Throws exception if output buffer is too small for input.}
  */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 6293) // MSVC sanitiser warns that we wrap n in the for loop
#endif
  inline size_t to_hex_string(char *out, size_t outlen, const char *_in, size_t inlen)
  {
    unsigned const char *in = (unsigned const char *)_in;
    static constexpr char table[] = "0123456789abcdef";
    if (outlen<inlen * 2)
      throw std::invalid_argument("Output buffer too small.");
    for (size_t n = inlen - 2; n <= inlen - 2; n -= 2)
    {
      out[n * 2 + 3] = table[(in[n + 1] >> 4) & 0xf];
      out[n * 2 + 2] = table[in[n + 1] & 0xf];
      out[n * 2 + 1] = table[(in[n] >> 4) & 0xf];
      out[n * 2 + 0] = table[in[n] & 0xf];
    }
    if (inlen & 1)
    {
      out[1] = table[(in[0] >> 4) & 0xf];
      out[0] = table[in[0] & 0xf];
    }
    return inlen * 2;
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
  //! \overload
  inline std::string to_hex_string(std::string in)
  {
    std::string out(in.size() * 2, ' ');
    to_hex_string(const_cast<char *>(out.data()), out.size(), in.data(), in.size());
    return out;
  }

  /*! \brief Converts a hex string to a number. Out buffer can be same as in buffer.

  Note that this routine is about 43% slower than to_hex_string(), half of which is due to input validation.

  \ingroup utils
  \complexity{O(N) where N is the length of the string.}
  \exceptionmodel{Throws exception if output buffer is too small for input or input size is not multiple of two.}
  */
  inline size_t from_hex_string(char *out, size_t outlen, const char *in, size_t inlen)
  {
    if (inlen % 2)
      throw std::invalid_argument("Input buffer not multiple of two.");
    if (outlen<inlen / 2)
      throw std::invalid_argument("Output buffer too small.");
    bool is_invalid = false;
    auto fromhex = [&is_invalid](char c) -> unsigned char
    {
#if 1
      // ASCII starting from 48 is 0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~
      //                           48               65                              97
      static constexpr unsigned char table[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,                                                    // +10 = 58
        255, 255, 255, 255, 255, 255, 255,                                                                                                      // +7  = 65
        10, 11, 12, 13, 14, 15,                                                                                                                 // +6  = 71
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,       // +26 = 97
        10, 11, 12, 13, 14, 15
      };
      unsigned char r = 255;
      if (c >= 48 && c <= 102)
        r = table[c - 48];
      if (r == 255)
        is_invalid = true;
      return r;
#else
      if (c >= '0' && c <= '9')
        return c - '0';
      if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
      if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
      BOOST_AFIO_THROW(std::invalid_argument("Input is not hexadecimal."));
#endif
    };
    for (size_t n = 0; n<inlen / 2; n += 4)
    {
      unsigned char c[8];
      c[0] = fromhex(in[n * 2]);
      c[1] = fromhex(in[n * 2 + 1]);
      c[2] = fromhex(in[n * 2 + 2]);
      c[3] = fromhex(in[n * 2 + 3]);
      out[n] = (c[1] << 4) | c[0];
      c[4] = fromhex(in[n * 2 + 4]);
      c[5] = fromhex(in[n * 2 + 5]);
      out[n + 1] = (c[3] << 4) | c[2];
      c[6] = fromhex(in[n * 2 + 6]);
      c[7] = fromhex(in[n * 2 + 7]);
      out[n + 2] = (c[5] << 4) | c[4];
      out[n + 3] = (c[7] << 4) | c[6];
    }
    for (size_t n = inlen / 2 - (inlen / 2) % 4; n<inlen / 2; n++)
    {
      unsigned char c1 = fromhex(in[n * 2]), c2 = fromhex(in[n * 2 + 1]);
      out[n] = (c2 << 4) | c1;
    }
    if (is_invalid)
      throw std::invalid_argument("Input is not hexadecimal.");
    return inlen / 2;
  }

  /*! \brief Returns a cryptographically random string capable of being used as a filename. Essentially random_fill() + to_hex_string().

  \param randomlen The number of bytes of randomness to use for the string.
  \return A string representing the randomness at a 2x ratio, so if 32 bytes were requested, this string would be 64 bytes long.
  \ingroup utils
  \complexity{Whatever the system API takes.}
  \exceptionmodel{Any error from the operating system.}
  */
  inline std::string random_string(size_t randomlen)
  {
    size_t outlen = randomlen * 2;
    std::string ret(outlen, 0);
    random_fill(const_cast<char *>(ret.data()), randomlen);
    to_hex_string(const_cast<char *>(ret.data()), outlen, ret.data(), randomlen);
    return ret;
  }

#ifndef BOOST_AFIO_SECDEC_INTRINSICS
# if defined(__GCC__) || defined(__clang__)
#  define BOOST_AFIO_SECDEC_INTRINSICS 1
# elif defined(_MSC_VER) && (defined(_M_X64) || _M_IX86_FP==1)
#  define BOOST_AFIO_SECDEC_INTRINSICS 1
# endif
#endif
#ifndef BOOST_AFIO_SECDEC_INTRINSICS
# define BOOST_AFIO_SECDEC_INTRINSICS 0
#endif
  /*! \class secded_ecc
  \brief Calculates the single error correcting double error detecting (SECDED) Hamming Error Correcting Code for a \em blocksize block of bytes. For example, a secdec_ecc<8> would be the very common 72,64 Hamming code used in ECC RAM, or secdec_ecc<4096> would be for a 32784,32768 Hamming code.

  Did you know that some non-ECC RAM systems can see 1e-12 flips/bit/hour, which is 3.3 bits flipped in a 16Gb RAM system
  per 24 hours). See Schroeder, Pinheiro and Weber (2009) 'DRAM Errors in the Wild: A Large-Scale Field Study'.

  After construction during which lookup tables are built, no state is modified and therefore this class is safe for static
  storage (indeed if C++ 14 is available, the constructor is constexpr). The maximum number of bits in a code is a good four
  billion, I did try limiting it to 65536 for performance but it wasn't worth it, and one might want > 8Kb blocks maybe.
  As with all SECDED ECC, undefined behaviour occurs when more than two bits of error are present or the ECC supplied
  is incorrect. You should combine this SECDED with a robust hash which can tell you definitively if a buffer is error
  free or not rather than relying on this to correctly do so.

  The main intended use case for this routine is calculating the ECC on data being written to disc, and hence that is
  where performance has been maximised. It is not expected that this routine will be frequently called on data being read
  from disc i.e. only when its hash doesn't match its contents which should be very rare, and then a single bit heal using this routine is attempted
  before trying again with the hash. Care was taken that really enormous SECDEDs are fast, in fact tuning was mostly
  done for the 32784,32768 code which can heal one bad bit per 4Kb page as the main thing we have in mind is achieving
  reliable filing system code on computers without ECC RAM and in which sustained large quantities of random disc i/o produce
  a worrying number of flipped bits in a 24 hour period (anywhere between 0 and 3 on my hardware here, average is about 0.8).

  Performance of the fixed block size routine where you supply whole chunks of \em blocksize is therefore \b particularly excellent
  as I spent a lot of time tuning it for Ivy Bridge and later out of order architectures: an
  amazing 22 cycles per byte for the 32784,32768 code, which is a testament to modern out of order CPUs (remember SECDED inherently must work a bit
  at a time, so that's just 2.75 amortised CPU cycles per bit which includes a table load, a bit test, and a conditional XOR)
  i.e. it's pushing about 1.5 ops per clock cycle. On my 3.9Ghz i7-3770K here, I see about 170Mb/sec per CPU core.

  The variable length routine is necessarily much slower as it must work in single bytes, and achieves 72 cycles per byte,
  or 9 cycles per bit (64Mb/sec per CPU core).

  \ingroup utils
  \complexity{O(N) where N is the blocksize}
  \exceptionmodel{Throws constexpr exceptions in constructor only, otherwise entirely noexcept.}
  */
  template<size_t blocksize> class secded_ecc
  {
  public:
    typedef unsigned int result_type; //!< The largest ECC which can be calculated
  private:
    static constexpr size_t bits_per_byte = 8;
    typedef unsigned char unit_type;  // The batch unit of processing
    result_type bitsvalid;
    // Many CPUs (x86) are slow doing variable bit shifts, so keep a table
    result_type ecc_twospowers[sizeof(result_type)*bits_per_byte];
    unsigned short ecc_table[blocksize*bits_per_byte];
    static bool _is_single_bit_set(result_type x)
    {
#ifndef _MSC_VER
#if defined(__i386__) || defined(__x86_64__)
#ifndef __SSE4_2__
      // Do a once off runtime check
      static int have_popcnt = [] {
        size_t cx, dx;
#if defined(__x86_64__)
        asm("cpuid": "=c" (cx), "=d" (dx) : "a" (1), "b" (0), "c" (0), "d" (0));
#else
        asm("pushl %%ebx\n\tcpuid\n\tpopl %%ebx\n\t": "=c" (cx), "=d" (dx) : "a" (1), "c" (0), "d" (0));
#endif
        return (dx&(1 << 26)) != 0/*SSE2*/ && (cx&(1 << 23)) != 0/*POPCNT*/;
      }();
      if (have_popcnt)
#endif
      {
        unsigned count;
        asm("popcnt %1,%0" : "=r"(count) : "rm"(x) : "cc");
        return count == 1;
      }
#endif
      return __builtin_popcount(x) == 1;
#else
      x -= (x >> 1) & 0x55555555;
      x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
      x = (x + (x >> 4)) & 0x0f0f0f0f;
      unsigned int count = (x * 0x01010101) >> 24;
      return count == 1;
#if 0
      x -= (x >> 1) & 0x5555555555555555ULL;
      x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
      x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0fULL;
      unsigned long long count = (x * 0x0101010101010101ULL) >> 56;
      return count == 1;
#endif
#endif
    }
  public:
    //! Constructs an instance, configuring the necessary lookup tables
    BOOST_CXX14_CONSTEXPR secded_ecc()
    {
      for (size_t n = 0; n<sizeof(result_type)*bits_per_byte; n++)
        ecc_twospowers[n] = ((result_type)1 << n);
      result_type length = blocksize*bits_per_byte;
      // This is (data bits + parity bits + 1) <= 2^(parity bits)
      for (result_type p = 1; p<sizeof(result_type)*bits_per_byte; p++)
        if ((length + p + 1) <= ecc_twospowers[p])
        {
          bitsvalid = p;
          break;
        }
      if ((bits_per_byte - 1 + bitsvalid) / bits_per_byte>sizeof(result_type))
        throw std::runtime_error("secdec_ecc: ECC would exceed the size of result_type!");
      for (result_type i = 0; i<blocksize*bits_per_byte; i++)
      {
        // Make a code bit
        result_type b = i + 1;
#if BOOST_AFIO_SECDEC_INTRINSICS && 0 // let constexpr do its thing
#ifdef _MSC_VER
        unsigned long _topbit;
        _BitScanReverse(&_topbit, b);
        result_type topbit = _topbit;
#else
        result_type topbit = bits_per_byte*sizeof(result_type) - __builtin_clz(b);
#endif
        b += topbit;
        if (b >= ecc_twospowers[topbit]) b++;
        //while(b>ecc_twospowers(_topbit+1)) _topbit++;
        //b+=_topbit;
        //if(b>=ecc_twospowers(_topbit)) b++;
#else
        for (size_t p = 0; ecc_twospowers[p]<(b + 1); p++)
          b++;
#endif
        ecc_table[i] = (unsigned short)b;
        if (b>(unsigned short)-1)
          throw std::runtime_error("secdec_ecc: Precalculated table has exceeded its bounds");
      }
    }
    //! The number of bits valid in result_type
    constexpr result_type result_bits_valid() const noexcept
    {
      return bitsvalid;
    }
    //! Accumulate ECC from fixed size buffer
    result_type operator()(result_type ecc, const char *buffer) const noexcept
    {
      if (blocksize<sizeof(unit_type) * 8)
        return (*this)(ecc, buffer, blocksize);
      // Process in lumps of eight
      const unit_type *_buffer = (const unit_type *)buffer;
      //#pragma omp parallel for reduction(^:ecc)
      for (size_t i = 0; i<blocksize; i += sizeof(unit_type) * 8)
      {
        union { unsigned long long v; unit_type c[8]; };
        result_type prefetch[8];
        v = *(unsigned long long *)(&_buffer[0 + i / sizeof(unit_type)]); // min 1 cycle
#define BOOST_AFIO_ROUND(n) \
          prefetch[0]=ecc_table[(i+0)*8+n]; \
          prefetch[1]=ecc_table[(i+1)*8+n]; \
          prefetch[2]=ecc_table[(i+2)*8+n]; \
          prefetch[3]=ecc_table[(i+3)*8+n]; \
          prefetch[4]=ecc_table[(i+4)*8+n]; \
          prefetch[5]=ecc_table[(i+5)*8+n]; \
          prefetch[6]=ecc_table[(i+6)*8+n]; \
          prefetch[7]=ecc_table[(i+7)*8+n]; \
          if(c[0]&((unit_type)1<<n))\
            ecc^=prefetch[0];\
          if(c[1]&((unit_type)1<<n))\
            ecc^=prefetch[1];\
          if(c[2]&((unit_type)1<<n))\
            ecc^=prefetch[2];\
          if(c[3]&((unit_type)1<<n))\
            ecc^=prefetch[3];\
          if(c[4]&((unit_type)1<<n))\
            ecc^=prefetch[4];\
          if(c[5]&((unit_type)1<<n))\
            ecc^=prefetch[5];\
          if(c[6]&((unit_type)1<<n))\
            ecc^=prefetch[6];\
          if(c[7]&((unit_type)1<<n))\
            ecc^=prefetch[7];
        BOOST_AFIO_ROUND(0)                                                    // prefetch = min 8, bit test and xor = min 16, total = 24
          BOOST_AFIO_ROUND(1)
          BOOST_AFIO_ROUND(2)
          BOOST_AFIO_ROUND(3)
          BOOST_AFIO_ROUND(4)
          BOOST_AFIO_ROUND(5)
          BOOST_AFIO_ROUND(6)
          BOOST_AFIO_ROUND(7)
#undef BOOST_AFIO_ROUND                                                      // total should be 1+(8*24/3)=65
      }
      return ecc;
    }
    result_type operator()(const char *buffer) const noexcept { return (*this)(0, buffer); }
    //! Accumulate ECC from partial buffer where \em length <= \em blocksize
    result_type operator()(result_type ecc, const char *buffer, size_t length) const noexcept
    {
      const unit_type *_buffer = (const unit_type *)buffer;
      //#pragma omp parallel for reduction(^:ecc)
      for (size_t i = 0; i<length; i += sizeof(unit_type))
      {
        unit_type c = _buffer[i / sizeof(unit_type)];                 // min 1 cycle
        if (!c)                                                    // min 1 cycle
          continue;
        char bitset[bits_per_byte*sizeof(unit_type)];
        result_type prefetch[bits_per_byte*sizeof(unit_type)];
        // Most compilers will roll this out
        for (size_t n = 0; n<bits_per_byte*sizeof(unit_type); n++)   // min 16 cycles
        {
          bitset[n] = !!(c&((unit_type)1 << n));
          prefetch[n] = ecc_table[i*bits_per_byte + n];               // min 8 cycles
        }
        result_type localecc = 0;
        for (size_t n = 0; n<bits_per_byte*sizeof(unit_type); n++)
        {
          if (bitset[n])                                           // min 8 cycles
            localecc ^= prefetch[n];                                // min 8 cycles
        }
        ecc ^= localecc;                                            // min 1 cycle. Total cycles = min 43 cycles/byte
      }
      return ecc;
    }
    result_type operator()(const char *buffer, size_t length) const noexcept { return (*this)(0, buffer, length); }
    //! Given the original ECC and the new ECC for a buffer, find the bad bit. Return (result_type)-1 if not found (e.g. ECC corrupt)
    result_type find_bad_bit(result_type good_ecc, result_type bad_ecc) const noexcept
    {
      result_type length = blocksize*bits_per_byte, eccdiff = good_ecc^bad_ecc;
      if (_is_single_bit_set(eccdiff))
        return (result_type)-1;
      for (result_type i = 0, b = 1; i<length; i++, b++)
      {
        // Skip parity bits
        while (_is_single_bit_set(b))
          b++;
        if (b == eccdiff)
          return i;
      }
      return (result_type)-1;
    }
    //! The outcomes from verify()
    enum verify_status
    {
      corrupt = 0,  //!< The buffer had more than a single bit corrupted or the ECC was invalid
      okay = 1,     //!< The buffer had no errors
      healed = 2    //!< The buffer was healed
    };
    //! Verifies and heals when possible a buffer, returning non zero if the buffer is error free
    verify_status verify(char *buffer, result_type good_ecc) const noexcept
    {
      result_type this_ecc = (*this)(0, buffer);
      if (this_ecc == good_ecc)
        return verify_status::okay; // no errors
      result_type badbit = find_bad_bit(good_ecc, this_ecc);
      if ((result_type)-1 == badbit)
        return verify_status::corrupt; // parity corrupt?
      buffer[badbit / bits_per_byte] ^= (unsigned char)ecc_twospowers[badbit%bits_per_byte];
      this_ecc = (*this)(0, buffer);
      if (this_ecc == good_ecc)
        return healed; // error healed
                       // Put the bit back
      buffer[badbit / bits_per_byte] ^= (unsigned char)ecc_twospowers[badbit%bits_per_byte];
      return verify_status::corrupt; // more than one bit was corrupt
    }
  };

  namespace detail
  {
    struct large_page_allocation
    {
      void *p;
      size_t page_size_used;
      size_t actual_size;
      large_page_allocation() : p(nullptr), page_size_used(0), actual_size(0) { }
      large_page_allocation(void *_p, size_t pagesize, size_t actual) : p(_p), page_size_used(pagesize), actual_size(actual) { }
    };
    inline large_page_allocation calculate_large_page_allocation(size_t bytes)
    {
      large_page_allocation ret;
      auto pagesizes(page_sizes());
      do
      {
        ret.page_size_used = pagesizes.back();
        pagesizes.pop_back();
      } while (!pagesizes.empty() && !(bytes / ret.page_size_used));
      ret.actual_size = (bytes + ret.page_size_used - 1)&~(ret.page_size_used - 1);
      return ret;
    }
    BOOST_AFIO_HEADERS_ONLY_FUNC_SPEC large_page_allocation allocate_large_pages(size_t bytes);
    BOOST_AFIO_HEADERS_ONLY_FUNC_SPEC void deallocate_large_pages(void *p, size_t bytes);
  }
  /*! \class page_allocator
  \brief An STL allocator which allocates large TLB page memory.
  \ingroup utils

  If the operating system is configured to allow it, this type of memory is particularly efficient for doing
  large scale file i/o. This is because the kernel must normally convert the scatter gather buffers you pass
  into extended scatter gather buffers as the memory you see as contiguous may not, and probably isn't, actually be
  contiguous in physical memory. Regions returned by this allocator \em may be allocated contiguously in physical
  memory and therefore the kernel can pass through your scatter gather buffers unmodified.

  A particularly useful combination with this allocator is with the page_sizes() member function of __afio_dispatcher__.
  This will return which pages sizes are possible, and which page sizes are enabled for this user. If writing a
  file copy routine for example, using this allocator with the largest page size as the copy chunk makes a great
  deal of sense.

  Be aware that as soon as the allocation exceeds a large page size, most systems allocate in multiples of the large
  page size, so if the large page size were 2Mb and you allocate 2Mb + 1 byte, 4Mb is actually consumed.
  */
  template <typename T>
  class page_allocator
  {
  public:
    typedef T         value_type;
    typedef T*        pointer;
    typedef const T*  const_pointer;
    typedef T& reference;
    typedef const T&  const_reference;
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;
    typedef std::true_type propagate_on_container_move_assignment;
    typedef std::true_type is_always_equal;

    template <class U>
    struct rebind { typedef page_allocator<U> other; };

    page_allocator() noexcept
    {}

    template <class U>
    page_allocator(const page_allocator<U>&) noexcept
    {}

    size_type
      max_size() const noexcept
    {
      return size_type(~0) / sizeof(T);
    }

    pointer
      address(reference x) const noexcept
    {
      return std::addressof(x);
    }

    const_pointer
      address(const_reference x) const noexcept
    {
      return std::addressof(x);
    }

    pointer
      allocate(size_type n, const void *hint = 0)
    {
      if (n>max_size())
        throw std::bad_alloc();
      auto mem(detail::allocate_large_pages(n * sizeof(T)));
      if (mem.p == nullptr)
        throw std::bad_alloc();
      return reinterpret_cast<pointer>(mem.p);
    }

    void
      deallocate(pointer p, size_type n)
    {
      if (n>max_size())
        throw std::bad_alloc();
      detail::deallocate_large_pages(p, n * sizeof(T));
    }

    template <class U, class ...Args>
    void
      construct(U* p, Args&&... args)
    {
      ::new(reinterpret_cast<void*>(p)) U(std::forward<Args>(args)...);
    }

    template <class U> void
      destroy(U* p)
    {
      p->~U();
    }
  };
  template <>
  class page_allocator<void>
  {
  public:
    typedef void         value_type;
    typedef void*        pointer;
    typedef const void*  const_pointer;
    typedef std::true_type propagate_on_container_move_assignment;
    typedef std::true_type is_always_equal;

    template <class U>
    struct rebind { typedef page_allocator<U> other; };
  };
  template<class T, class U> inline bool operator==(const page_allocator<T> &, const page_allocator<U> &) noexcept { return true; }
}

BOOST_AFIO_V2_NAMESPACE_END

# if BOOST_AFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#  define BOOST_AFIO_INCLUDED_BY_HEADER 1
#  ifdef WIN32
#   include "detail/impl/windows/utils.ipp"
#  else
#   include "detail/impl/posix/utils.ipp"
#  endif
#  undef BOOST_AFIO_INCLUDED_BY_HEADER
# endif


#endif
