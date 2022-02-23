// Copyright (c) 2005, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ----
//
// This is just a very thin wrapper over densehashtable.h, just
// like sgi stl's stl_hash_map is a very thin wrapper over
// stl_hashtable.  The major thing we define is operator[], because
// we have a concept of a data_type which stl_hashtable doesn't
// (it only has a key and a value).
//
// NOTE: this is exactly like sparse_hash_map.h, with the word
// "sparse" replaced by "dense", except for the addition of
// set_empty_key().
//
//   YOU MUST CALL SET_EMPTY_KEY() IMMEDIATELY AFTER CONSTRUCTION.
//
// Otherwise your program will die in mysterious ways.  (Note if you
// use the constructor that takes an InputIterator range, you pass in
// the empty key in the constructor, rather than after.  As a result,
// this constructor differs from the standard STL version.)
//
// In other respects, we adhere mostly to the STL semantics for
// hash-map.  One important exception is that insert() may invalidate
// iterators entirely -- STL semantics are that insert() may reorder
// iterators, but they all still refer to something valid in the
// hashtable.  Not so for us.  Likewise, insert() may invalidate
// pointers into the hashtable.  (Whether insert invalidates iterators
// and pointers depends on whether it results in a hashtable resize).
// On the plus side, delete() doesn't invalidate iterators or pointers
// at all, or even change the ordering of elements.
//
// Here are a few "power user" tips:
//
//    1) set_deleted_key():
//         If you want to use erase() you *must* call set_deleted_key(),
//         in addition to set_empty_key(), after construction.
//         The deleted and empty keys must differ.
//
//    2) resize(0):
//         When an item is deleted, its memory isn't freed right
//         away.  This allows you to iterate over a hashtable,
//         and call erase(), without invalidating the iterator.
//         To force the memory to be freed, call resize(0).
//         For tr1 compatibility, this can also be called as rehash(0).
//
//    3) min_load_factor(0.0)
//         Setting the minimum load factor to 0.0 guarantees that
//         the hash table will never shrink.
//
// Roughly speaking:
//   (1) dense_hash_map: fastest, uses the most memory unless entries are small
//   (2) sparse_hash_map: slowest, uses the least memory
//   (3) hash_map / unordered_map (STL): in the middle
//
// Typically I use sparse_hash_map when I care about space and/or when
// I need to save the hashtable on disk.  I use hash_map otherwise.  I
// don't personally use dense_hash_set ever; some people use it for
// small sets with lots of lookups.
//
// - dense_hash_map has, typically, about 78% memory overhead (if your
//   data takes up X bytes, the hash_map uses .78X more bytes in overhead).
// - sparse_hash_map has about 4 bits overhead per entry.
// - sparse_hash_map can be 3-7 times slower than the others for lookup and,
//   especially, inserts.  See time_hash_map.cc for details.
//
// See /usr/(local/)?doc/sparsehash-*/dense_hash_map.html
// for information about how to use this class.

#ifndef _DENSE_HASH_MAP_H_
#define _DENSE_HASH_MAP_H_

#include <algorithm>                  // needed by stl_alloc
#include <assert.h>
#include <functional>                 // for equal_to<>, select1st<>, etc
#include <iosfwd>
#include <iterator>             // For iterator tags
#include <limits>               // for numeric_limits
#include <memory>                     // for alloc
#include <new>                // for placement new
#include <stddef.h>                  // for size_t
#include <stdexcept>                 // For length_error
#include <stdio.h>              // for FILE, fwrite, fread
#include <stdlib.h>           // for malloc/realloc/free
#include <utility>                    // for pair<>
#include <xhash>

/*
 * NOTE: This file is for internal use only.
 *       Do not use these #defines in your own program!
 */

/* Namespace for Google classes */
#define GOOGLE_NAMESPACE  ::google

#if (_MSC_VER >= 1800 )

/* the location of the header defining hash functions */
#define HASH_FUN_H  <unordered_map>

#else /* Earlier than VSC++ 2013 */ 

/* the location of the header defining hash functions */
#define HASH_FUN_H  <hash_map>
 
#endif

/* the namespace of the hash<> function */
#define HASH_NAMESPACE  stdext

/* Define to 1 if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define to 1 if the system has the type `long long'. */
#define HAVE_LONG_LONG  1

/* Define to 1 if you have the `memcpy' function. */
#define HAVE_MEMCPY  1

/* Define to 1 if you have the <stdint.h> header file. */
#undef HAVE_STDINT_H

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H  1

/* Define to 1 if the system has the type `uint16_t'. */
#undef HAVE_UINT16_T

/* Define to 1 if the system has the type `u_int16_t'. */
#undef HAVE_U_INT16_T

/* Define to 1 if the system has the type `__uint16'. */
#define HAVE___UINT16  1

/* The system-provided hash function including the namespace. */
#define SPARSEHASH_HASH  HASH_NAMESPACE::hash_compare

/* The system-provided hash function, in namespace HASH_NAMESPACE. */
#define SPARSEHASH_HASH_NO_NAMESPACE  hash_compare

/* Stops putting the code inside the Google namespace */
#define _END_GOOGLE_NAMESPACE_  }

/* Puts following code inside the Google namespace */
#define _START_GOOGLE_NAMESPACE_   namespace google {

// Copyright (c) 2005, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ---
//
// A dense hashtable is a particular implementation of
// a hashtable: one that is meant to minimize memory allocation.
// It does this by using an array to store all the data.  We
// steal a value from the key space to indicate "empty" array
// elements (ie indices where no item lives) and another to indicate
// "deleted" elements.
//
// (Note it is possible to change the value of the delete key
// on the fly; you can even remove it, though after that point
// the hashtable is insert_only until you set it again.  The empty
// value however can't be changed.)
//
// To minimize allocation and pointer overhead, we use internal
// probing, in which the hashtable is a single table, and collisions
// are resolved by trying to insert again in another bucket.  The
// most cache-efficient internal probing schemes are linear probing
// (which suffers, alas, from clumping) and quadratic probing, which
// is what we implement by default.
//
// Type requirements: value_type is required to be Copy Constructible
// and Default Constructible. It is not required to be (and commonly
// isn't) Assignable.
//
// You probably shouldn't use this code directly.  Use dense_hash_map<>
// or dense_hash_set<> instead.

// You can change the following below:
// HT_OCCUPANCY_PCT      -- how full before we double size
// HT_EMPTY_PCT          -- how empty before we halve size
// HT_MIN_BUCKETS        -- default smallest bucket size
//
// You can also change enlarge_factor (which defaults to
// HT_OCCUPANCY_PCT), and shrink_factor (which defaults to
// HT_EMPTY_PCT) with set_resizing_parameters().
//
// How to decide what values to use?
// shrink_factor's default of .4 * OCCUPANCY_PCT, is probably good.
// HT_MIN_BUCKETS is probably unnecessary since you can specify
// (indirectly) the starting number of buckets at construct-time.
// For enlarge_factor, you can use this chart to try to trade-off
// expected lookup time to the space taken up.  By default, this
// code uses quadratic probing, though you can change it to linear
// via JUMP_ below if you really want to.
//
// From http://www.augustana.ca/~mohrj/courses/1999.fall/csc210/lecture_notes/hashing.html
// NUMBER OF PROBES / LOOKUP       Successful            Unsuccessful
// Quadratic collision resolution   1 - ln(1-L) - L/2    1/(1-L) - L - ln(1-L)
// Linear collision resolution     [1+1/(1-L)]/2         [1+1/(1-L)2]/2
//
// -- enlarge_factor --           0.10  0.50  0.60  0.75  0.80  0.90  0.99
// QUADRATIC COLLISION RES.
//    probes/successful lookup    1.05  1.44  1.62  2.01  2.21  2.85  5.11
//    probes/unsuccessful lookup  1.11  2.19  2.82  4.64  5.81  11.4  103.6
// LINEAR COLLISION RES.
//    probes/successful lookup    1.06  1.5   1.75  2.5   3.0   5.5   50.5
//    probes/unsuccessful lookup  1.12  2.5   3.6   8.5   13.0  50.0  5000.0

#ifndef _DENSEHASHTABLE_H_
#define _DENSEHASHTABLE_H_

// Copyright (c) 2010, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ---
//
// Provides classes shared by both sparse and dense hashtable.
//
// sh_hashtable_settings has parameters for growing and shrinking
// a hashtable.  It also packages zero-size functor (ie. hasher).
//
// Other functions and classes provide common code for serializing
// and deserializing hashtables to a stream (such as a FILE*).

#ifndef UTIL_GTL_HASHTABLE_COMMON_H_
#define UTIL_GTL_HASHTABLE_COMMON_H_

_START_GOOGLE_NAMESPACE_

template <bool> struct SparsehashCompileAssert { };
#define SPARSEHASH_COMPILE_ASSERT(expr, msg) \
  __attribute__((unused)) typedef SparsehashCompileAssert<(bool(expr))> msg[bool(expr) ? 1 : -1]

namespace sparsehash_internal {

// Adaptor methods for reading/writing data from an INPUT or OUPTUT
// variable passed to serialize() or unserialize().  For now we
// have implemented INPUT/OUTPUT for FILE*, istream*/ostream* (note
// they are pointers, unlike typical use), or else a pointer to
// something that supports a Read()/Write() method.
//
// For technical reasons, we implement read_data/write_data in two
// stages.  The actual work is done in *_data_internal, which takes
// the stream argument twice: once as a template type, and once with
// normal type information.  (We only use the second version.)  We do
// this because of how C++ picks what function overload to use.  If we
// implemented this the naive way:
//    bool read_data(istream* is, const void* data, size_t length);
//    template<typename T> read_data(T* fp,  const void* data, size_t length);
// C++ would prefer the second version for every stream type except
// istream.  However, we want C++ to prefer the first version for
// streams that are *subclasses* of istream, such as istringstream.
// This is not possible given the way template types are resolved.  So
// we split the stream argument in two, one of which is templated and
// one of which is not.  The specialized functions (like the istream
// version above) ignore the template arg and use the second, 'type'
// arg, getting subclass matching as normal.  The 'catch-all'
// functions (the second version above) use the template arg to deduce
// the type, and use a second, void* arg to achieve the desired
// 'catch-all' semantics.

// ----- low-level I/O for FILE* ----

template<typename Ignored>
inline bool read_data_internal(Ignored*, FILE* fp,
                               void* data, size_t length) {
  return fread(data, length, 1, fp) == 1;
}

template<typename Ignored>
inline bool write_data_internal(Ignored*, FILE* fp,
                                const void* data, size_t length) {
  return fwrite(data, length, 1, fp) == 1;
}

// ----- low-level I/O for iostream ----

// We want the caller to be responsible for #including <iostream>, not
// us, because iostream is a big header!  According to the standard,
// it's only legal to delay the instantiation the way we want to if
// the istream/ostream is a template type.  So we jump through hoops.
template<typename ISTREAM>
inline bool read_data_internal_for_istream(ISTREAM* fp,
                                           void* data, size_t length) {
  return fp->read(reinterpret_cast<char*>(data), length).good();
}
template<typename Ignored>
inline bool read_data_internal(Ignored*, std::istream* fp,
                               void* data, size_t length) {
  return read_data_internal_for_istream(fp, data, length);
}

template<typename OSTREAM>
inline bool write_data_internal_for_ostream(OSTREAM* fp,
                                            const void* data, size_t length) {
  return fp->write(reinterpret_cast<const char*>(data), length).good();
}
template<typename Ignored>
inline bool write_data_internal(Ignored*, std::ostream* fp,
                                const void* data, size_t length) {
  return write_data_internal_for_ostream(fp, data, length);
}

// ----- low-level I/O for custom streams ----

// The INPUT type needs to support a Read() method that takes a
// buffer and a length and returns the number of bytes read.
template <typename INPUT>
inline bool read_data_internal(INPUT* fp, void*,
                               void* data, size_t length) {
  return static_cast<size_t>(fp->Read(data, length)) == length;
}

// The OUTPUT type needs to support a Write() operation that takes
// a buffer and a length and returns the number of bytes written.
template <typename OUTPUT>
inline bool write_data_internal(OUTPUT* fp, void*,
                                const void* data, size_t length) {
  return static_cast<size_t>(fp->Write(data, length)) == length;
}

// ----- low-level I/O: the public API ----

template <typename INPUT>
inline bool read_data(INPUT* fp, void* data, size_t length) {
  return read_data_internal(fp, fp, data, length);
}

template <typename OUTPUT>
inline bool write_data(OUTPUT* fp, const void* data, size_t length) {
  return write_data_internal(fp, fp, data, length);
}

// Uses read_data() and write_data() to read/write an integer.
// length is the number of bytes to read/write (which may differ
// from sizeof(IntType), allowing us to save on a 32-bit system
// and load on a 64-bit system).  Excess bytes are taken to be 0.
// INPUT and OUTPUT must match legal inputs to read/write_data (above).
template <typename INPUT, typename IntType>
bool read_bigendian_number(INPUT* fp, IntType* value, size_t length) {
  *value = 0;
  unsigned char byte;
  // We require IntType to be unsigned or else the shifting gets all screwy.
  SPARSEHASH_COMPILE_ASSERT(static_cast<IntType>(-1) > static_cast<IntType>(0),
                            serializing_int_requires_an_unsigned_type);
  for (size_t i = 0; i < length; ++i) {
    if (!read_data(fp, &byte, sizeof(byte))) return false;
    *value |= static_cast<IntType>(byte) << ((length - 1 - i) * 8);
  }
  return true;
}

template <typename OUTPUT, typename IntType>
bool write_bigendian_number(OUTPUT* fp, IntType value, size_t length) {
  unsigned char byte;
  // We require IntType to be unsigned or else the shifting gets all screwy.
  SPARSEHASH_COMPILE_ASSERT(static_cast<IntType>(-1) > static_cast<IntType>(0),
                            serializing_int_requires_an_unsigned_type);
  for (size_t i = 0; i < length; ++i) {
    byte = (sizeof(value) <= length-1 - i)
        ? 0 : static_cast<unsigned char>((value >> ((length-1 - i) * 8)) & 255);
    if (!write_data(fp, &byte, sizeof(byte))) return false;
  }
  return true;
}

// If your keys and values are simple enough, you can pass this
// serializer to serialize()/unserialize().  "Simple enough" means
// value_type is a POD type that contains no pointers.  Note,
// however, we don't try to normalize endianness.
// This is the type used for NopointerSerializer.
template <typename value_type> struct pod_serializer {
  template <typename INPUT>
  bool operator()(INPUT* fp, value_type* value) const {
    return read_data(fp, value, sizeof(*value));
  }

  template <typename OUTPUT>
  bool operator()(OUTPUT* fp, const value_type& value) const {
    return write_data(fp, &value, sizeof(value));
  }
};

// Settings contains parameters for growing and shrinking the table.
// It also packages zero-size functor (ie. hasher).
//
// It does some munging of the hash value in cases where we think
// (fear) the original hash function might not be very good.  In
// particular, the default hash of pointers is the identity hash,
// so probably all the low bits are 0.  We identify when we think
// we're hashing a pointer, and chop off the low bits.  Note this
// isn't perfect: even when the key is a pointer, we can't tell
// for sure that the hash is the identity hash.  If it's not, this
// is needless work (and possibly, though not likely, harmful).

template<typename Key, typename HashFunc,
         typename SizeType, int HT_MIN_BUCKETS>
class sh_hashtable_settings : public HashFunc {
 public:
  typedef Key key_type;
  typedef HashFunc hasher;
  typedef SizeType size_type;

 public:
  sh_hashtable_settings(const hasher& hf,
                        const float ht_occupancy_flt,
                        const float ht_empty_flt)
      : hasher(hf),
        enlarge_threshold_(0),
        shrink_threshold_(0),
        consider_shrink_(false),
        use_empty_(false),
        use_deleted_(false),
        num_ht_copies_(0) {
    set_enlarge_factor(ht_occupancy_flt);
    set_shrink_factor(ht_empty_flt);
  }

  size_type hash(const key_type& v) const {
    // We munge the hash value when we don't trust hasher::operator().
    return hash_munger<Key>::MungedHash(hasher::operator()(v));
  }

  float enlarge_factor() const {
    return enlarge_factor_;
  }
  void set_enlarge_factor(float f) {
    enlarge_factor_ = f;
  }
  float shrink_factor() const {
    return shrink_factor_;
  }
  void set_shrink_factor(float f) {
    shrink_factor_ = f;
  }

  size_type enlarge_threshold() const {
    return enlarge_threshold_;
  }
  void set_enlarge_threshold(size_type t) {
    enlarge_threshold_ = t;
  }
  size_type shrink_threshold() const {
    return shrink_threshold_;
  }
  void set_shrink_threshold(size_type t) {
    shrink_threshold_ = t;
  }

  size_type enlarge_size(size_type x) const {
    return static_cast<size_type>(x * enlarge_factor_);
  }
  size_type shrink_size(size_type x) const {
    return static_cast<size_type>(x * shrink_factor_);
  }

  bool consider_shrink() const {
    return consider_shrink_;
  }
  void set_consider_shrink(bool t) {
    consider_shrink_ = t;
  }

  bool use_empty() const {
    return use_empty_;
  }
  void set_use_empty(bool t) {
    use_empty_ = t;
  }

  bool use_deleted() const {
    return use_deleted_;
  }
  void set_use_deleted(bool t) {
    use_deleted_ = t;
  }

  size_type num_ht_copies() const {
    return static_cast<size_type>(num_ht_copies_);
  }
  void inc_num_ht_copies() {
    ++num_ht_copies_;
  }

  // Reset the enlarge and shrink thresholds
  void reset_thresholds(size_type num_buckets) {
    set_enlarge_threshold(enlarge_size(num_buckets));
    set_shrink_threshold(shrink_size(num_buckets));
    // whatever caused us to reset already considered
    set_consider_shrink(false);
  }

  // Caller is resposible for calling reset_threshold right after
  // set_resizing_parameters.
  void set_resizing_parameters(float shrink, float grow) {
    assert(shrink >= 0.0);
    assert(grow <= 1.0);
    if (shrink > grow/2.0f)
      shrink = grow / 2.0f;     // otherwise we thrash hashtable size
    set_shrink_factor(shrink);
    set_enlarge_factor(grow);
  }

  // This is the smallest size a hashtable can be without being too crowded
  // If you like, you can give a min #buckets as well as a min #elts
  size_type min_buckets(size_type num_elts, size_type min_buckets_wanted) {
    float enlarge = enlarge_factor();
    size_type sz = HT_MIN_BUCKETS;             // min buckets allowed
    while ( sz < min_buckets_wanted ||
            num_elts >= static_cast<size_type>(sz * enlarge) ) {
      // This just prevents overflowing size_type, since sz can exceed
      // max_size() here.
      if (static_cast<size_type>(sz * 2) < sz) {
        throw std::length_error("resize overflow");  // protect against overflow
      }
      sz *= 2;
    }
    return sz;
  }

 private:
  template<class HashKey> class hash_munger {
   public:
    static size_t MungedHash(size_t hash) {
      return hash;
    }
  };
  // This matches when the hashtable key is a pointer.
  template<class HashKey> class hash_munger<HashKey*> {
   public:
    static size_t MungedHash(size_t hash) {
      // TODO(csilvers): consider rotating instead:
      //    static const int shift = (sizeof(void *) == 4) ? 2 : 3;
      //    return (hash << (sizeof(hash) * 8) - shift)) | (hash >> shift);
      // This matters if we ever change sparse/dense_hash_* to compare
      // hashes before comparing actual values.  It's speedy on x86.
      return hash / sizeof(void*);   // get rid of known-0 bits
    }
  };

  size_type enlarge_threshold_;  // table.size() * enlarge_factor
  size_type shrink_threshold_;   // table.size() * shrink_factor
  float enlarge_factor_;         // how full before resize
  float shrink_factor_;          // how empty before resize
  // consider_shrink=true if we should try to shrink before next insert
  bool consider_shrink_;
  bool use_empty_;    // used only by densehashtable, not sparsehashtable
  bool use_deleted_;  // false until delkey has been set
  // num_ht_copies is a counter incremented every Copy/Move
  unsigned int num_ht_copies_;
};

}  // namespace sparsehash_internal

#undef SPARSEHASH_COMPILE_ASSERT
_END_GOOGLE_NAMESPACE_

#endif  // UTIL_GTL_HASHTABLE_COMMON_H_

// Copyright (c) 2010, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ---

#ifndef UTIL_GTL_LIBC_ALLOCATOR_WITH_REALLOC_H_
#define UTIL_GTL_LIBC_ALLOCATOR_WITH_REALLOC_H_

_START_GOOGLE_NAMESPACE_

template<class T>
class libc_allocator_with_realloc {
 public:
  typedef T value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;

  libc_allocator_with_realloc() {}
  libc_allocator_with_realloc(const libc_allocator_with_realloc&) {}
  ~libc_allocator_with_realloc() {}

  pointer address(reference r) const  { return &r; }
  const_pointer address(const_reference r) const  { return &r; }

  pointer allocate(size_type n, const_pointer = 0) {
    return static_cast<pointer>(malloc(n * sizeof(value_type)));
  }
  void deallocate(pointer p, size_type) {
    free(p);
  }
  pointer reallocate(pointer p, size_type n) {
    // p points to a storage array whose objects have already been destroyed
    // cast to void* to prevent compiler warnings about calling realloc() on
    // an object which cannot be relocated in memory
    return static_cast<pointer>(realloc(static_cast<void*>(p), n * sizeof(value_type)));
  }

  size_type max_size() const  {
    return static_cast<size_type>(-1) / sizeof(value_type);
  }

  void construct(pointer p, const value_type& val) {
    new(p) value_type(val);
  }
  void destroy(pointer p) { p->~value_type(); }

  template <class U>
  libc_allocator_with_realloc(const libc_allocator_with_realloc<U>&) {}

  template<class U>
  struct rebind {
    typedef libc_allocator_with_realloc<U> other;
  };
};

// libc_allocator_with_realloc<void> specialization.
template<>
class libc_allocator_with_realloc<void> {
 public:
  typedef void value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef void* pointer;
  typedef const void* const_pointer;

  template<class U>
  struct rebind {
    typedef libc_allocator_with_realloc<U> other;
  };
};

template<class T>
inline bool operator==(const libc_allocator_with_realloc<T>&,
                       const libc_allocator_with_realloc<T>&) {
  return true;
}

template<class T>
inline bool operator!=(const libc_allocator_with_realloc<T>&,
                       const libc_allocator_with_realloc<T>&) {
  return false;
}

_END_GOOGLE_NAMESPACE_

#endif  // UTIL_GTL_LIBC_ALLOCATOR_WITH_REALLOC_H_

// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ----
//
// This code is compiled directly on many platforms, including client
// platforms like Windows, Mac, and embedded systems.  Before making
// any changes here, make sure that you're not breaking any platforms.
//
// Define a small subset of tr1 type traits. The traits we define are:
//   is_integral
//   is_floating_point
//   is_pointer
//   is_enum
//   is_reference
//   is_pod
//   has_trivial_constructor
//   has_trivial_copy
//   has_trivial_assign
//   has_trivial_destructor
//   remove_const
//   remove_volatile
//   remove_cv
//   remove_reference
//   add_reference
//   remove_pointer
//   is_same
//   is_convertible
// We can add more type traits as required.

#ifndef BASE_TYPE_TRAITS_H_
#define BASE_TYPE_TRAITS_H_


// Copyright 2005 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ----
//
// Template metaprogramming utility functions.
//
// This code is compiled directly on many platforms, including client
// platforms like Windows, Mac, and embedded systems.  Before making
// any changes here, make sure that you're not breaking any platforms.
//
//
// The names choosen here reflect those used in tr1 and the boost::mpl
// library, there are similar operations used in the Loki library as
// well.  I prefer the boost names for 2 reasons:
// 1.  I think that portions of the Boost libraries are more likely to
// be included in the c++ standard.
// 2.  It is not impossible that some of the boost libraries will be
// included in our own build in the future.
// Both of these outcomes means that we may be able to directly replace
// some of these with boost equivalents.
//
#ifndef BASE_TEMPLATE_UTIL_H_
#define BASE_TEMPLATE_UTIL_H_

_START_GOOGLE_NAMESPACE_

// Types small_ and big_ are guaranteed such that sizeof(small_) <
// sizeof(big_)
typedef char small_;

struct big_ {
  char dummy[2];
};

// Identity metafunction.
template <class T>
struct identity_ {
  typedef T type;
};

// integral_constant, defined in tr1, is a wrapper for an integer
// value. We don't really need this generality; we could get away
// with hardcoding the integer type to bool. We use the fully
// general integer_constant for compatibility with tr1.

template<class T, T v>
struct integral_constant {
  static const T value = v;
  typedef T value_type;
  typedef integral_constant<T, v> type;
};

template <class T, T v> const T integral_constant<T, v>::value;

// Abbreviations: true_type and false_type are structs that represent boolean
// true and false values. Also define the boost::mpl versions of those names,
// true_ and false_.
typedef integral_constant<bool, true>  true_type;
typedef integral_constant<bool, false> false_type;
typedef true_type  true_;
typedef false_type false_;

// if_ is a templatized conditional statement.
// if_<cond, A, B> is a compile time evaluation of cond.
// if_<>::type contains A if cond is true, B otherwise.
template<bool cond, typename A, typename B>
struct if_{
  typedef A type;
};

template<typename A, typename B>
struct if_<false, A, B> {
  typedef B type;
};

// type_equals_ is a template type comparator, similar to Loki IsSameType.
// type_equals_<A, B>::value is true iff "A" is the same type as "B".
//
// New code should prefer base::is_same, defined in base/type_traits.h.
// It is functionally identical, but is_same is the standard spelling.
template<typename A, typename B>
struct type_equals_ : public false_ {
};

template<typename A>
struct type_equals_<A, A> : public true_ {
};

// and_ is a template && operator.
// and_<A, B>::value evaluates "A::value && B::value".
template<typename A, typename B>
struct and_ : public integral_constant<bool, (A::value && B::value)> {
};

// or_ is a template || operator.
// or_<A, B>::value evaluates "A::value || B::value".
template<typename A, typename B>
struct or_ : public integral_constant<bool, (A::value || B::value)> {
};

_END_GOOGLE_NAMESPACE_

#endif  // BASE_TEMPLATE_UTIL_H_

_START_GOOGLE_NAMESPACE_

template <class T> struct is_integral;
template <class T> struct is_floating_point;
template <class T> struct is_pointer;
// MSVC can't compile this correctly, and neither can gcc 3.3.5 (at least)
#if !defined(_MSC_VER) && !(defined(__GNUC__) && __GNUC__ <= 3)
// is_enum uses is_convertible, which is not available on MSVC.
template <class T> struct is_enum;
#endif
template <class T> struct is_reference;
template <class T> struct is_pod;
template <class T> struct has_trivial_constructor;
template <class T> struct has_trivial_copy;
template <class T> struct has_trivial_assign;
template <class T> struct has_trivial_destructor;
template <class T> struct remove_const;
template <class T> struct remove_volatile;
template <class T> struct remove_cv;
template <class T> struct remove_reference;
template <class T> struct add_reference;
template <class T> struct remove_pointer;
template <class T, class U> struct is_same;
#if !defined(_MSC_VER) && !(defined(__GNUC__) && __GNUC__ <= 3)
template <class From, class To> struct is_convertible;
#endif

// is_integral is false except for the built-in integer types. A
// cv-qualified type is integral if and only if the underlying type is.
template <class T> struct is_integral : false_type { };
template<> struct is_integral<bool> : true_type { };
template<> struct is_integral<char> : true_type { };
template<> struct is_integral<unsigned char> : true_type { };
template<> struct is_integral<signed char> : true_type { };
#if defined(_MSC_VER)
// wchar_t is not by default a distinct type from unsigned short in
// Microsoft C.
// See http://msdn2.microsoft.com/en-us/library/dh8che7s(VS.80).aspx
template<> struct is_integral<__wchar_t> : true_type { };
#else
template<> struct is_integral<wchar_t> : true_type { };
#endif
template<> struct is_integral<short> : true_type { };
template<> struct is_integral<unsigned short> : true_type { };
template<> struct is_integral<int> : true_type { };
template<> struct is_integral<unsigned int> : true_type { };
template<> struct is_integral<long> : true_type { };
template<> struct is_integral<unsigned long> : true_type { };
#ifdef HAVE_LONG_LONG
template<> struct is_integral<long long> : true_type { };
template<> struct is_integral<unsigned long long> : true_type { };
#endif
template <class T> struct is_integral<const T> : is_integral<T> { };
template <class T> struct is_integral<volatile T> : is_integral<T> { };
template <class T> struct is_integral<const volatile T> : is_integral<T> { };

// is_floating_point is false except for the built-in floating-point types.
// A cv-qualified type is integral if and only if the underlying type is.
template <class T> struct is_floating_point : false_type { };
template<> struct is_floating_point<float> : true_type { };
template<> struct is_floating_point<double> : true_type { };
template<> struct is_floating_point<long double> : true_type { };
template <class T> struct is_floating_point<const T>
    : is_floating_point<T> { };
template <class T> struct is_floating_point<volatile T>
    : is_floating_point<T> { };
template <class T> struct is_floating_point<const volatile T>
    : is_floating_point<T> { };

// is_pointer is false except for pointer types. A cv-qualified type (e.g.
// "int* const", as opposed to "int const*") is cv-qualified if and only if
// the underlying type is.
template <class T> struct is_pointer : false_type { };
template <class T> struct is_pointer<T*> : true_type { };
template <class T> struct is_pointer<const T> : is_pointer<T> { };
template <class T> struct is_pointer<volatile T> : is_pointer<T> { };
template <class T> struct is_pointer<const volatile T> : is_pointer<T> { };

#if !defined(_MSC_VER) && !(defined(__GNUC__) && __GNUC__ <= 3)

namespace internal {

template <class T> struct is_class_or_union {
  template <class U> static small_ tester(void (U::*)());
  template <class U> static big_ tester(...);
  static const bool value = sizeof(tester<T>(0)) == sizeof(small_);
};

// is_convertible chokes if the first argument is an array. That's why
// we use add_reference here.
template <bool NotUnum, class T> struct is_enum_impl
    : is_convertible<typename add_reference<T>::type, int> { };

template <class T> struct is_enum_impl<true, T> : false_type { };

}  // namespace internal

// Specified by TR1 [4.5.1] primary type categories.

// Implementation note:
//
// Each type is either void, integral, floating point, array, pointer,
// reference, member object pointer, member function pointer, enum,
// union or class. Out of these, only integral, floating point, reference,
// class and enum types are potentially convertible to int. Therefore,
// if a type is not a reference, integral, floating point or class and
// is convertible to int, it's a enum. Adding cv-qualification to a type
// does not change whether it's an enum.
//
// Is-convertible-to-int check is done only if all other checks pass,
// because it can't be used with some types (e.g. void or classes with
// inaccessible conversion operators).
template <class T> struct is_enum
    : internal::is_enum_impl<
          is_same<T, void>::value ||
              is_integral<T>::value ||
              is_floating_point<T>::value ||
              is_reference<T>::value ||
              internal::is_class_or_union<T>::value,
          T> { };

template <class T> struct is_enum<const T> : is_enum<T> { };
template <class T> struct is_enum<volatile T> : is_enum<T> { };
template <class T> struct is_enum<const volatile T> : is_enum<T> { };

#endif

// is_reference is false except for reference types.
template<typename T> struct is_reference : false_type {};
template<typename T> struct is_reference<T&> : true_type {};

// We can't get is_pod right without compiler help, so fail conservatively.
// We will assume it's false except for arithmetic types, enumerations,
// pointers and cv-qualified versions thereof. Note that std::pair<T,U>
// is not a POD even if T and U are PODs.
template <class T> struct is_pod
 : integral_constant<bool, (is_integral<T>::value ||
                            is_floating_point<T>::value ||
#if !defined(_MSC_VER) && !(defined(__GNUC__) && __GNUC__ <= 3)
                            // is_enum is not available on MSVC.
                            is_enum<T>::value ||
#endif
                            is_pointer<T>::value)> { };
template <class T> struct is_pod<const T> : is_pod<T> { };
template <class T> struct is_pod<volatile T> : is_pod<T> { };
template <class T> struct is_pod<const volatile T> : is_pod<T> { };

// We can't get has_trivial_constructor right without compiler help, so
// fail conservatively. We will assume it's false except for: (1) types
// for which is_pod is true. (2) std::pair of types with trivial
// constructors. (3) array of a type with a trivial constructor.
// (4) const versions thereof.
template <class T> struct has_trivial_constructor : is_pod<T> { };
template <class T, class U> struct has_trivial_constructor<std::pair<T, U> >
  : integral_constant<bool,
                      (has_trivial_constructor<T>::value &&
                       has_trivial_constructor<U>::value)> { };
template <class A, int N> struct has_trivial_constructor<A[N]>
  : has_trivial_constructor<A> { };
template <class T> struct has_trivial_constructor<const T>
  : has_trivial_constructor<T> { };

// We can't get has_trivial_copy right without compiler help, so fail
// conservatively. We will assume it's false except for: (1) types
// for which is_pod is true. (2) std::pair of types with trivial copy
// constructors. (3) array of a type with a trivial copy constructor.
// (4) const versions thereof.
template <class T> struct has_trivial_copy : is_pod<T> { };
template <class T, class U> struct has_trivial_copy<std::pair<T, U> >
  : integral_constant<bool,
                      (has_trivial_copy<T>::value &&
                       has_trivial_copy<U>::value)> { };
template <class A, int N> struct has_trivial_copy<A[N]>
  : has_trivial_copy<A> { };
template <class T> struct has_trivial_copy<const T> : has_trivial_copy<T> { };

// We can't get has_trivial_assign right without compiler help, so fail
// conservatively. We will assume it's false except for: (1) types
// for which is_pod is true. (2) std::pair of types with trivial copy
// constructors. (3) array of a type with a trivial assign constructor.
template <class T> struct has_trivial_assign : is_pod<T> { };
template <class T, class U> struct has_trivial_assign<std::pair<T, U> >
  : integral_constant<bool,
                      (has_trivial_assign<T>::value &&
                       has_trivial_assign<U>::value)> { };
template <class A, int N> struct has_trivial_assign<A[N]>
  : has_trivial_assign<A> { };

// We can't get has_trivial_destructor right without compiler help, so
// fail conservatively. We will assume it's false except for: (1) types
// for which is_pod is true. (2) std::pair of types with trivial
// destructors. (3) array of a type with a trivial destructor.
// (4) const versions thereof.
template <class T> struct has_trivial_destructor : is_pod<T> { };
template <class T, class U> struct has_trivial_destructor<std::pair<T, U> >
  : integral_constant<bool,
                      (has_trivial_destructor<T>::value &&
                       has_trivial_destructor<U>::value)> { };
template <class A, int N> struct has_trivial_destructor<A[N]>
  : has_trivial_destructor<A> { };
template <class T> struct has_trivial_destructor<const T>
  : has_trivial_destructor<T> { };

// Specified by TR1 [4.7.1]
template<typename T> struct remove_const { typedef T type; };
template<typename T> struct remove_const<T const> { typedef T type; };
template<typename T> struct remove_volatile { typedef T type; };
template<typename T> struct remove_volatile<T volatile> { typedef T type; };
template<typename T> struct remove_cv {
  typedef typename remove_const<typename remove_volatile<T>::type>::type type;
};

// Specified by TR1 [4.7.2] Reference modifications.
template<typename T> struct remove_reference { typedef T type; };
template<typename T> struct remove_reference<T&> { typedef T type; };

template <typename T> struct add_reference { typedef T& type; };
template <typename T> struct add_reference<T&> { typedef T& type; };

// Specified by TR1 [4.7.4] Pointer modifications.
template<typename T> struct remove_pointer { typedef T type; };
template<typename T> struct remove_pointer<T*> { typedef T type; };
template<typename T> struct remove_pointer<T* const> { typedef T type; };
template<typename T> struct remove_pointer<T* volatile> { typedef T type; };
template<typename T> struct remove_pointer<T* const volatile> {
  typedef T type; };

// Specified by TR1 [4.6] Relationships between types
template<typename T, typename U> struct is_same : public false_type { };
template<typename T> struct is_same<T, T> : public true_type { };

// Specified by TR1 [4.6] Relationships between types
#if !defined(_MSC_VER) && !(defined(__GNUC__) && __GNUC__ <= 3)
namespace internal {

// This class is an implementation detail for is_convertible, and you
// don't need to know how it works to use is_convertible. For those
// who care: we declare two different functions, one whose argument is
// of type To and one with a variadic argument list. We give them
// return types of different size, so we can use sizeof to trick the
// compiler into telling us which function it would have chosen if we
// had called it with an argument of type From.  See Alexandrescu's
// _Modern C++ Design_ for more details on this sort of trick.

template <typename From, typename To>
struct ConvertHelper {
  static small_ Test(To);
  static big_ Test(...);
  static From Create();
};
}  // namespace internal

// Inherits from true_type if From is convertible to To, false_type otherwise.
template <typename From, typename To>
struct is_convertible
    : integral_constant<bool,
                        sizeof(internal::ConvertHelper<From, To>::Test(
                                  internal::ConvertHelper<From, To>::Create()))
                        == sizeof(small_)> {
};
#endif

_END_GOOGLE_NAMESPACE_

// Right now these macros are no-ops, and mostly just document the fact
// these types are PODs, for human use.  They may be made more contentful
// later.  The typedef is just to make it legal to put a semicolon after
// these macros.
#define DECLARE_POD(TypeName) typedef int Dummy_Type_For_DECLARE_POD
#define DECLARE_NESTED_POD(TypeName) DECLARE_POD(TypeName)
#define PROPAGATE_POD_FROM_TEMPLATE_ARGUMENT(TemplateName)             \
    typedef int Dummy_Type_For_PROPAGATE_POD_FROM_TEMPLATE_ARGUMENT
#define ENFORCE_POD(TypeName) typedef int Dummy_Type_For_ENFORCE_POD

#endif  // BASE_TYPE_TRAITS_H_


_START_GOOGLE_NAMESPACE_

namespace base {   // just to make google->opensource transition easier
using GOOGLE_NAMESPACE::true_type;
using GOOGLE_NAMESPACE::false_type;
using GOOGLE_NAMESPACE::integral_constant;
using GOOGLE_NAMESPACE::is_same;
using GOOGLE_NAMESPACE::remove_const;
}

// The probing method
// Linear probing
// #define JUMP_(key, num_probes)    ( 1 )
// Quadratic probing
#define JUMP_(key, num_probes)    ( num_probes )

// Hashtable class, used to implement the hashed associative containers
// hash_set and hash_map.

// Value: what is stored in the table (each bucket is a Value).
// Key: something in a 1-to-1 correspondence to a Value, that can be used
//      to search for a Value in the table (find() takes a Key).
// HashFcn: Takes a Key and returns an integer, the more unique the better.
// ExtractKey: given a Value, returns the unique Key associated with it.
//             Must inherit from unary_function, or at least have a
//             result_type enum indicating the return type of operator().
// SetKey: given a Value* and a Key, modifies the value such that
//         ExtractKey(value) == key.  We guarantee this is only called
//         with key == deleted_key or key == empty_key.
// EqualKey: Given two Keys, says whether they are the same (that is,
//           if they are both associated with the same Value).
// Alloc: STL allocator to use to allocate memory.

template <class Value, class Key, class HashFcn,
          class ExtractKey, class SetKey, class EqualKey, class Alloc>
class dense_hashtable;

template <class V, class K, class HF, class ExK, class SetK, class EqK, class A>
struct dense_hashtable_iterator;

template <class V, class K, class HF, class ExK, class SetK, class EqK, class A>
struct dense_hashtable_const_iterator;

// We're just an array, but we need to skip over empty and deleted elements
template <class V, class K, class HF, class ExK, class SetK, class EqK, class A>
struct dense_hashtable_iterator {
 private:
  typedef typename A::template rebind<V>::other value_alloc_type;

 public:
  typedef dense_hashtable_iterator<V,K,HF,ExK,SetK,EqK,A>       iterator;
  typedef dense_hashtable_const_iterator<V,K,HF,ExK,SetK,EqK,A> const_iterator;

  typedef std::forward_iterator_tag iterator_category;  // very little defined!
  typedef V value_type;
  typedef typename value_alloc_type::difference_type difference_type;
  typedef typename value_alloc_type::size_type size_type;
  typedef typename value_alloc_type::reference reference;
  typedef typename value_alloc_type::pointer pointer;

  // "Real" constructor and default constructor
  dense_hashtable_iterator(const dense_hashtable<V,K,HF,ExK,SetK,EqK,A> *h,
                           pointer it, pointer it_end, bool advance)
    : ht(h), pos(it), end(it_end)   {
    if (advance)  advance_past_empty_and_deleted();
  }
  dense_hashtable_iterator() { }
  // The default destructor is fine; we don't define one
  // The default operator= is fine; we don't define one

  // Happy dereferencer
  reference operator*() const { return *pos; }
  pointer operator->() const { return &(operator*()); }

  // Arithmetic.  The only hard part is making sure that
  // we're not on an empty or marked-deleted array element
  void advance_past_empty_and_deleted() {
    while ( pos != end && (ht->test_empty(*this) || ht->test_deleted(*this)) )
      ++pos;
  }
  iterator& operator++()   {
    assert(pos != end); ++pos; advance_past_empty_and_deleted(); return *this;
  }
  iterator operator++(int) { iterator tmp(*this); ++*this; return tmp; }

  // Comparison.
  bool operator==(const iterator& it) const { return pos == it.pos; }
  bool operator!=(const iterator& it) const { return pos != it.pos; }

  // The actual data
  const dense_hashtable<V,K,HF,ExK,SetK,EqK,A> *ht;
  pointer pos, end;
};

// Now do it all again, but with const-ness!
template <class V, class K, class HF, class ExK, class SetK, class EqK, class A>
struct dense_hashtable_const_iterator {
 private:
  typedef typename A::template rebind<V>::other value_alloc_type;

 public:
  typedef dense_hashtable_iterator<V,K,HF,ExK,SetK,EqK,A>       iterator;
  typedef dense_hashtable_const_iterator<V,K,HF,ExK,SetK,EqK,A> const_iterator;

  typedef std::forward_iterator_tag iterator_category;  // very little defined!
  typedef V value_type;
  typedef typename value_alloc_type::difference_type difference_type;
  typedef typename value_alloc_type::size_type size_type;
  typedef typename value_alloc_type::const_reference reference;
  typedef typename value_alloc_type::const_pointer pointer;

  // "Real" constructor and default constructor
  dense_hashtable_const_iterator(
      const dense_hashtable<V,K,HF,ExK,SetK,EqK,A> *h,
      pointer it, pointer it_end, bool advance)
    : ht(h), pos(it), end(it_end)   {
    if (advance)  advance_past_empty_and_deleted();
  }
  dense_hashtable_const_iterator()
    : ht(NULL), pos(pointer()), end(pointer()) { }
  // This lets us convert regular iterators to const iterators
  dense_hashtable_const_iterator(const iterator &it)
    : ht(it.ht), pos(it.pos), end(it.end) { }
  // The default destructor is fine; we don't define one
  // The default operator= is fine; we don't define one

  // Happy dereferencer
  reference operator*() const { return *pos; }
  pointer operator->() const { return &(operator*()); }

  // Arithmetic.  The only hard part is making sure that
  // we're not on an empty or marked-deleted array element
  void advance_past_empty_and_deleted() {
    while ( pos != end && (ht->test_empty(*this) || ht->test_deleted(*this)) )
      ++pos;
  }
  const_iterator& operator++()   {
    assert(pos != end); ++pos; advance_past_empty_and_deleted(); return *this;
  }
  const_iterator operator++(int) { const_iterator tmp(*this); ++*this; return tmp; }

  // Comparison.
  bool operator==(const const_iterator& it) const { return pos == it.pos; }
  bool operator!=(const const_iterator& it) const { return pos != it.pos; }

  // The actual data
  const dense_hashtable<V,K,HF,ExK,SetK,EqK,A> *ht;
  pointer pos, end;
};

template <class Value, class Key, class HashFcn,
          class ExtractKey, class SetKey, class EqualKey, class Alloc>
class dense_hashtable {
 private:
  typedef typename Alloc::template rebind<Value>::other value_alloc_type;

 public:
  typedef Key key_type;
  typedef Value value_type;
  typedef HashFcn hasher;
  typedef EqualKey key_equal;
  typedef Alloc allocator_type;

  typedef typename value_alloc_type::size_type size_type;
  typedef typename value_alloc_type::difference_type difference_type;
  typedef typename value_alloc_type::reference reference;
  typedef typename value_alloc_type::const_reference const_reference;
  typedef typename value_alloc_type::pointer pointer;
  typedef typename value_alloc_type::const_pointer const_pointer;
  typedef dense_hashtable_iterator<Value, Key, HashFcn,
                                   ExtractKey, SetKey, EqualKey, Alloc>
  iterator;

  typedef dense_hashtable_const_iterator<Value, Key, HashFcn,
                                         ExtractKey, SetKey, EqualKey, Alloc>
  const_iterator;

  // These come from tr1.  For us they're the same as regular iterators.
  typedef iterator local_iterator;
  typedef const_iterator const_local_iterator;

  // How full we let the table get before we resize, by default.
  // Knuth says .8 is good -- higher causes us to probe too much,
  // though it saves memory.
  static const int HT_OCCUPANCY_PCT;  // defined at the bottom of this file

  // How empty we let the table get before we resize lower, by default.
  // (0.0 means never resize lower.)
  // It should be less than OCCUPANCY_PCT / 2 or we thrash resizing
  static const int HT_EMPTY_PCT;      // defined at the bottom of this file

  // Minimum size we're willing to let hashtables be.
  // Must be a power of two, and at least 4.
  // Note, however, that for a given hashtable, the initial size is a
  // function of the first constructor arg, and may be >HT_MIN_BUCKETS.
  static const size_type HT_MIN_BUCKETS = 4;

  // By default, if you don't specify a hashtable size at
  // construction-time, we use this size.  Must be a power of two, and
  // at least HT_MIN_BUCKETS.
  static const size_type HT_DEFAULT_STARTING_BUCKETS = 32;

  // ITERATOR FUNCTIONS
  iterator begin()             { return iterator(this, table,
                                                 table + num_buckets, true); }
  iterator end()               { return iterator(this, table + num_buckets,
                                                 table + num_buckets, true); }
  const_iterator begin() const { return const_iterator(this, table,
                                                       table+num_buckets,true);}
  const_iterator end() const   { return const_iterator(this, table + num_buckets,
                                                       table+num_buckets,true);}

  // These come from tr1 unordered_map.  They iterate over 'bucket' n.
  // We'll just consider bucket n to be the n-th element of the table.
  local_iterator begin(size_type i) {
    return local_iterator(this, table + i, table + i+1, false);
  }
  local_iterator end(size_type i) {
    local_iterator it = begin(i);
    if (!test_empty(i) && !test_deleted(i))
      ++it;
    return it;
  }
  const_local_iterator begin(size_type i) const {
    return const_local_iterator(this, table + i, table + i+1, false);
  }
  const_local_iterator end(size_type i) const {
    const_local_iterator it = begin(i);
    if (!test_empty(i) && !test_deleted(i))
      ++it;
    return it;
  }

  // ACCESSOR FUNCTIONS for the things we templatize on, basically
  hasher hash_funct() const               { return settings; }
  key_equal key_eq() const                { return key_info; }
  allocator_type get_allocator() const {
    return allocator_type(val_info);
  }

  // Accessor function for statistics gathering.
  int num_table_copies() const { return settings.num_ht_copies(); }

 private:
  // Annoyingly, we can't copy values around, because they might have
  // const components (they're probably pair<const X, Y>).  We use
  // explicit destructor invocation and placement new to get around
  // this.  Arg.
  void set_value(pointer dst, const_reference src) {
    dst->~value_type();   // delete the old value, if any
    new(dst) value_type(src);
  }

  void destroy_buckets(size_type first, size_type last) {
    for ( ; first != last; ++first)
      table[first].~value_type();
  }

  // DELETE HELPER FUNCTIONS
  // This lets the user describe a key that will indicate deleted
  // table entries.  This key should be an "impossible" entry --
  // if you try to insert it for real, you won't be able to retrieve it!
  // (NB: while you pass in an entire value, only the key part is looked
  // at.  This is just because I don't know how to assign just a key.)
 private:
  void squash_deleted() {           // gets rid of any deleted entries we have
    if ( num_deleted ) {            // get rid of deleted before writing
      dense_hashtable tmp(*this);   // copying will get rid of deleted
      swap(tmp);                    // now we are tmp
    }
    assert(num_deleted == 0);
  }

  // Test if the given key is the deleted indicator.  Requires
  // num_deleted > 0, for correctness of read(), and because that
  // guarantees that key_info.delkey is valid.
  bool test_deleted_key(const key_type& key) const {
    assert(num_deleted > 0);
    return equals(key_info.delkey, key);
  }

 public:
  void set_deleted_key(const key_type &key) {
    // the empty indicator (if specified) and the deleted indicator
    // must be different
    assert((!settings.use_empty() || !equals(key, get_key(val_info.emptyval)))
           && "Passed the empty-key to set_deleted_key");
    // It's only safe to change what "deleted" means if we purge deleted guys
    squash_deleted();
    settings.set_use_deleted(true);
    key_info.delkey = key;
  }
  void clear_deleted_key() {
    squash_deleted();
    settings.set_use_deleted(false);
  }
  key_type deleted_key() const {
    assert(settings.use_deleted()
           && "Must set deleted key before calling deleted_key");
    return key_info.delkey;
  }

  // These are public so the iterators can use them
  // True if the item at position bucknum is "deleted" marker
  bool test_deleted(size_type bucknum) const {
    // Invariant: !use_deleted() implies num_deleted is 0.
    assert(settings.use_deleted() || num_deleted == 0);
    return num_deleted > 0 && test_deleted_key(get_key(table[bucknum]));
  }
  bool test_deleted(const iterator &it) const {
    // Invariant: !use_deleted() implies num_deleted is 0.
    assert(settings.use_deleted() || num_deleted == 0);
    return num_deleted > 0 && test_deleted_key(get_key(*it));
  }
  bool test_deleted(const const_iterator &it) const {
    // Invariant: !use_deleted() implies num_deleted is 0.
    assert(settings.use_deleted() || num_deleted == 0);
    return num_deleted > 0 && test_deleted_key(get_key(*it));
  }

 private:
  void check_use_deleted(const char* caller) {
    (void)caller;    // could log it if the assert failed
    assert(settings.use_deleted());
  }

  // Set it so test_deleted is true.  true if object didn't used to be deleted.
  bool set_deleted(iterator &it) {
    check_use_deleted("set_deleted()");
    bool retval = !test_deleted(it);
    // &* converts from iterator to value-type.
    set_key(&(*it), key_info.delkey);
    return retval;
  }
  // Set it so test_deleted is false.  true if object used to be deleted.
  bool clear_deleted(iterator &it) {
    check_use_deleted("clear_deleted()");
    // Happens automatically when we assign something else in its place.
    return test_deleted(it);
  }

  // We also allow to set/clear the deleted bit on a const iterator.
  // We allow a const_iterator for the same reason you can delete a
  // const pointer: it's convenient, and semantically you can't use
  // 'it' after it's been deleted anyway, so its const-ness doesn't
  // really matter.
  bool set_deleted(const_iterator &it) {
    check_use_deleted("set_deleted()");
    bool retval = !test_deleted(it);
    set_key(const_cast<pointer>(&(*it)), key_info.delkey);
    return retval;
  }
  // Set it so test_deleted is false.  true if object used to be deleted.
  bool clear_deleted(const_iterator &it) {
    check_use_deleted("clear_deleted()");
    return test_deleted(it);
  }

  // EMPTY HELPER FUNCTIONS
  // This lets the user describe a key that will indicate empty (unused)
  // table entries.  This key should be an "impossible" entry --
  // if you try to insert it for real, you won't be able to retrieve it!
  // (NB: while you pass in an entire value, only the key part is looked
  // at.  This is just because I don't know how to assign just a key.)
 public:
  // These are public so the iterators can use them
  // True if the item at position bucknum is "empty" marker
  bool test_empty(size_type bucknum) const {
    assert(settings.use_empty());  // we always need to know what's empty!
    return equals(get_key(val_info.emptyval), get_key(table[bucknum]));
  }
  bool test_empty(const iterator &it) const {
    assert(settings.use_empty());  // we always need to know what's empty!
    return equals(get_key(val_info.emptyval), get_key(*it));
  }
  bool test_empty(const const_iterator &it) const {
    assert(settings.use_empty());  // we always need to know what's empty!
    return equals(get_key(val_info.emptyval), get_key(*it));
  }

 private:
  void fill_range_with_empty(pointer table_start, pointer table_end) {
    std::uninitialized_fill(table_start, table_end, val_info.emptyval);
  }

 public:
  // TODO(csilvers): change all callers of this to pass in a key instead,
  //                 and take a const key_type instead of const value_type.
  void set_empty_key(const_reference val) {
    // Once you set the empty key, you can't change it
    assert(!settings.use_empty() && "Calling set_empty_key multiple times");
    // The deleted indicator (if specified) and the empty indicator
    // must be different.
    assert((!settings.use_deleted() || !equals(get_key(val), key_info.delkey))
           && "Setting the empty key the same as the deleted key");
    settings.set_use_empty(true);
    set_value(&val_info.emptyval, val);

    assert(!table);                  // must set before first use
    // num_buckets was set in constructor even though table was NULL
    table = val_info.allocate(num_buckets);
    assert(table);
    fill_range_with_empty(table, table + num_buckets);
  }
  // TODO(user): return a key_type rather than a value_type
  value_type empty_key() const {
    assert(settings.use_empty());
    return val_info.emptyval;
  }

  // FUNCTIONS CONCERNING SIZE
 public:
  size_type size() const      { return num_elements - num_deleted; }
  size_type max_size() const  { return val_info.max_size(); }
  bool empty() const          { return size() == 0; }
  size_type bucket_count() const      { return num_buckets; }
  size_type max_bucket_count() const  { return max_size(); }
  size_type nonempty_bucket_count() const { return num_elements; }
  // These are tr1 methods.  Their idea of 'bucket' doesn't map well to
  // what we do.  We just say every bucket has 0 or 1 items in it.
  size_type bucket_size(size_type i) const {
    return begin(i) == end(i) ? 0 : 1;
  }

 private:
  // Because of the above, size_type(-1) is never legal; use it for errors
  static const size_type ILLEGAL_BUCKET = size_type(-1);

  // Used after a string of deletes.  Returns true if we actually shrunk.
  // TODO(csilvers): take a delta so we can take into account inserts
  // done after shrinking.  Maybe make part of the Settings class?
  bool maybe_shrink() {
    assert(num_elements >= num_deleted);
    assert((bucket_count() & (bucket_count()-1)) == 0); // is a power of two
    assert(bucket_count() >= HT_MIN_BUCKETS);
    bool retval = false;

    // If you construct a hashtable with < HT_DEFAULT_STARTING_BUCKETS,
    // we'll never shrink until you get relatively big, and we'll never
    // shrink below HT_DEFAULT_STARTING_BUCKETS.  Otherwise, something
    // like "dense_hash_set<int> x; x.insert(4); x.erase(4);" will
    // shrink us down to HT_MIN_BUCKETS buckets, which is too small.
    const size_type num_remain = num_elements - num_deleted;
    const size_type shrink_threshold = settings.shrink_threshold();
    if (shrink_threshold > 0 && num_remain < shrink_threshold &&
        bucket_count() > HT_DEFAULT_STARTING_BUCKETS) {
      const float shrink_factor = settings.shrink_factor();
      size_type sz = bucket_count() / 2;    // find how much we should shrink
      while (sz > HT_DEFAULT_STARTING_BUCKETS &&
             num_remain < sz * shrink_factor) {
        sz /= 2;                            // stay a power of 2
      }
      dense_hashtable tmp(*this, sz);       // Do the actual resizing
      swap(tmp);                            // now we are tmp
      retval = true;
    }
    settings.set_consider_shrink(false);    // because we just considered it
    return retval;
  }

  // We'll let you resize a hashtable -- though this makes us copy all!
  // When you resize, you say, "make it big enough for this many more elements"
  // Returns true if we actually resized, false if size was already ok.
  bool resize_delta(size_type delta) {
    bool did_resize = false;
    if ( settings.consider_shrink() ) {  // see if lots of deletes happened
      if ( maybe_shrink() )
        did_resize = true;
    }
    if (num_elements >=
        (std::numeric_limits<size_type>::max)() - delta) {
      throw std::length_error("resize overflow");
    }
    if ( bucket_count() >= HT_MIN_BUCKETS &&
         (num_elements + delta) <= settings.enlarge_threshold() )
      return did_resize;                          // we're ok as we are

    // Sometimes, we need to resize just to get rid of all the
    // "deleted" buckets that are clogging up the hashtable.  So when
    // deciding whether to resize, count the deleted buckets (which
    // are currently taking up room).  But later, when we decide what
    // size to resize to, *don't* count deleted buckets, since they
    // get discarded during the resize.
    size_type needed_size = settings.min_buckets(num_elements + delta, 0);
    if ( needed_size <= bucket_count() )      // we have enough buckets
      return did_resize;

    size_type resize_to =
      settings.min_buckets(num_elements - num_deleted + delta, bucket_count());

    // When num_deleted is large, we may still grow but we do not want to
    // over expand.  So we reduce needed_size by a portion of num_deleted
    // (the exact portion does not matter).  This is especially helpful
    // when min_load_factor is zero (no shrink at all) to avoid doubling
    // the bucket count to infinity.  See also test ResizeWithoutShrink.
    needed_size = settings.min_buckets(num_elements - num_deleted / 4 + delta, 0);
    if (resize_to < needed_size &&    // may double resize_to
        resize_to < (std::numeric_limits<size_type>::max)() / 2) {
      // This situation means that we have enough deleted elements,
      // that once we purge them, we won't actually have needed to
      // grow.  But we may want to grow anyway: if we just purge one
      // element, say, we'll have to grow anyway next time we
      // insert.  Might as well grow now, since we're already going
      // through the trouble of copying (in order to purge the
      // deleted elements).
      const size_type target =
          static_cast<size_type>(settings.shrink_size(resize_to*2));
      if (num_elements - num_deleted + delta >= target) {
        // Good, we won't be below the shrink threshhold even if we double.
        resize_to *= 2;
      }
    }
    dense_hashtable tmp(*this, resize_to);
    swap(tmp);                             // now we are tmp
    return true;
  }

  // We require table be not-NULL and empty before calling this.
  void resize_table(size_type /*old_size*/, size_type new_size,
                    base::true_type) {
    table = val_info.realloc_or_die(table, new_size);
  }

  void resize_table(size_type old_size, size_type new_size, base::false_type) {
    val_info.deallocate(table, old_size);
    table = val_info.allocate(new_size);
  }

  // Used to actually do the rehashing when we grow/shrink a hashtable
  void copy_from(const dense_hashtable &ht, size_type min_buckets_wanted) {
    clear_to_size(settings.min_buckets(ht.size(), min_buckets_wanted));

    // We use a normal iterator to get non-deleted bcks from ht
    // We could use insert() here, but since we know there are
    // no duplicates and no deleted items, we can be more efficient
    assert((bucket_count() & (bucket_count()-1)) == 0);      // a power of two
    for ( const_iterator it = ht.begin(); it != ht.end(); ++it ) {
      size_type num_probes = 0;              // how many times we've probed
      size_type bucknum;
      const size_type bucket_count_minus_one = bucket_count() - 1;
      for (bucknum = hash(get_key(*it)) & bucket_count_minus_one;
           !test_empty(bucknum);                               // not empty
           bucknum = (bucknum + JUMP_(key, num_probes)) & bucket_count_minus_one) {
        ++num_probes;
        assert(num_probes < bucket_count()
               && "Hashtable is full: an error in key_equal<> or hash<>");
      }
      set_value(&table[bucknum], *it);       // copies the value to here
      num_elements++;
    }
    settings.inc_num_ht_copies();
  }

  // Required by the spec for hashed associative container
 public:
  // Though the docs say this should be num_buckets, I think it's much
  // more useful as num_elements.  As a special feature, calling with
  // req_elements==0 will cause us to shrink if we can, saving space.
  void resize(size_type req_elements) {       // resize to this or larger
    if ( settings.consider_shrink() || req_elements == 0 )
      maybe_shrink();
    if ( req_elements > num_elements )
      resize_delta(req_elements - num_elements);
  }

  // Get and change the value of shrink_factor and enlarge_factor.  The
  // description at the beginning of this file explains how to choose
  // the values.  Setting the shrink parameter to 0.0 ensures that the
  // table never shrinks.
  void get_resizing_parameters(float* shrink, float* grow) const {
    *shrink = settings.shrink_factor();
    *grow = settings.enlarge_factor();
  }
  void set_resizing_parameters(float shrink, float grow) {
    settings.set_resizing_parameters(shrink, grow);
    settings.reset_thresholds(bucket_count());
  }

  // CONSTRUCTORS -- as required by the specs, we take a size,
  // but also let you specify a hashfunction, key comparator,
  // and key extractor.  We also define a copy constructor and =.
  // DESTRUCTOR -- needs to free the table
  explicit dense_hashtable(size_type expected_max_items_in_table = 0,
                           const HashFcn& hf = HashFcn(),
                           const EqualKey& eql = EqualKey(),
                           const ExtractKey& ext = ExtractKey(),
                           const SetKey& set = SetKey(),
                           const Alloc& alloc = Alloc())
      : settings(hf),
        key_info(ext, set, eql),
        num_deleted(0),
        num_elements(0),
        num_buckets(expected_max_items_in_table == 0
                    ? HT_DEFAULT_STARTING_BUCKETS
                    : settings.min_buckets(expected_max_items_in_table, 0)),
        val_info(alloc_impl<value_alloc_type>(alloc)),
        table(NULL) {
    // table is NULL until emptyval is set.  However, we set num_buckets
    // here so we know how much space to allocate once emptyval is set
    settings.reset_thresholds(bucket_count());
  }

  // As a convenience for resize(), we allow an optional second argument
  // which lets you make this new hashtable a different size than ht
  dense_hashtable(const dense_hashtable& ht,
                  size_type min_buckets_wanted = HT_DEFAULT_STARTING_BUCKETS)
      : settings(ht.settings),
        key_info(ht.key_info),
        num_deleted(0),
        num_elements(0),
        num_buckets(0),
        val_info(ht.val_info),
        table(NULL) {
    if (!ht.settings.use_empty()) {
      // If use_empty isn't set, copy_from will crash, so we do our own copying.
      assert(ht.empty());
      num_buckets = settings.min_buckets(ht.size(), min_buckets_wanted);
      settings.reset_thresholds(bucket_count());
      return;
    }
    settings.reset_thresholds(bucket_count());
    copy_from(ht, min_buckets_wanted);   // copy_from() ignores deleted entries
  }

  dense_hashtable& operator= (const dense_hashtable& ht) {
    if (&ht == this)  return *this;        // don't copy onto ourselves
    if (!ht.settings.use_empty()) {
      assert(ht.empty());
      dense_hashtable empty_table(ht);  // empty table with ht's thresholds
      this->swap(empty_table);
      return *this;
    }
    settings = ht.settings;
    key_info = ht.key_info;
    set_value(&val_info.emptyval, ht.val_info.emptyval);
    // copy_from() calls clear and sets num_deleted to 0 too
    copy_from(ht, HT_MIN_BUCKETS);
    // we purposefully don't copy the allocator, which may not be copyable
    return *this;
  }

  ~dense_hashtable() {
    if (table) {
      destroy_buckets(0, num_buckets);
      val_info.deallocate(table, num_buckets);
    }
  }

  // Many STL algorithms use swap instead of copy constructors
  void swap(dense_hashtable& ht) {
    std::swap(settings, ht.settings);
    std::swap(key_info, ht.key_info);
    std::swap(num_deleted, ht.num_deleted);
    std::swap(num_elements, ht.num_elements);
    std::swap(num_buckets, ht.num_buckets);
    { value_type tmp;     // for annoying reasons, swap() doesn't work
      set_value(&tmp, val_info.emptyval);
      set_value(&val_info.emptyval, ht.val_info.emptyval);
      set_value(&ht.val_info.emptyval, tmp);
    }
    std::swap(table, ht.table);
    settings.reset_thresholds(bucket_count());  // also resets consider_shrink
    ht.settings.reset_thresholds(ht.bucket_count());
    // we purposefully don't swap the allocator, which may not be swap-able
  }

 private:
  void clear_to_size(size_type new_num_buckets) {
    if (!table) {
      table = val_info.allocate(new_num_buckets);
    } else {
      destroy_buckets(0, num_buckets);
      if (new_num_buckets != num_buckets) {   // resize, if necessary
        typedef base::integral_constant<bool,
            base::is_same<value_alloc_type,
                          libc_allocator_with_realloc<value_type> >::value>
            realloc_ok;
        resize_table(num_buckets, new_num_buckets, realloc_ok());
      }
    }
    assert(table);
    fill_range_with_empty(table, table + new_num_buckets);
    num_elements = 0;
    num_deleted = 0;
    num_buckets = new_num_buckets;          // our new size
    settings.reset_thresholds(bucket_count());
  }

 public:
  // It's always nice to be able to clear a table without deallocating it
  void clear() {
    // If the table is already empty, and the number of buckets is
    // already as we desire, there's nothing to do.
    const size_type new_num_buckets = settings.min_buckets(0, 0);
    if (num_elements == 0 && new_num_buckets == num_buckets) {
      return;
    }
    clear_to_size(new_num_buckets);
  }

  // Clear the table without resizing it.
  // Mimicks the stl_hashtable's behaviour when clear()-ing in that it
  // does not modify the bucket count
  void clear_no_resize() {
    if (num_elements > 0) {
      assert(table);
      destroy_buckets(0, num_buckets);
      fill_range_with_empty(table, table + num_buckets);
    }
    // don't consider to shrink before another erase()
    settings.reset_thresholds(bucket_count());
    num_elements = 0;
    num_deleted = 0;
  }

  // LOOKUP ROUTINES
 private:
  // Returns a pair of positions: 1st where the object is, 2nd where
  // it would go if you wanted to insert it.  1st is ILLEGAL_BUCKET
  // if object is not found; 2nd is ILLEGAL_BUCKET if it is.
  // Note: because of deletions where-to-insert is not trivial: it's the
  // first deleted bucket we see, as long as we don't find the key later
  std::pair<size_type, size_type> find_position(const key_type &key) const {
    size_type num_probes = 0;              // how many times we've probed
    const size_type bucket_count_minus_one = bucket_count() - 1;
    size_type bucknum = hash(key) & bucket_count_minus_one;
    size_type insert_pos = ILLEGAL_BUCKET; // where we would insert
    while ( 1 ) {                          // probe until something happens
      if ( test_empty(bucknum) ) {         // bucket is empty
        if ( insert_pos == ILLEGAL_BUCKET )   // found no prior place to insert
          return std::pair<size_type,size_type>(ILLEGAL_BUCKET, bucknum);
        else
          return std::pair<size_type,size_type>(ILLEGAL_BUCKET, insert_pos);

      } else if ( test_deleted(bucknum) ) {// keep searching, but mark to insert
        if ( insert_pos == ILLEGAL_BUCKET )
          insert_pos = bucknum;

      } else if ( equals(key, get_key(table[bucknum])) ) {
        return std::pair<size_type,size_type>(bucknum, ILLEGAL_BUCKET);
      }
      ++num_probes;                        // we're doing another probe
      bucknum = (bucknum + JUMP_(key, num_probes)) & bucket_count_minus_one;
      assert(num_probes < bucket_count()
             && "Hashtable is full: an error in key_equal<> or hash<>");
    }
  }

 public:

  iterator find(const key_type& key) {
    if ( size() == 0 ) return end();
    std::pair<size_type, size_type> pos = find_position(key);
    if ( pos.first == ILLEGAL_BUCKET )     // alas, not there
      return end();
    else
      return iterator(this, table + pos.first, table + num_buckets, false);
  }

  const_iterator find(const key_type& key) const {
    if ( size() == 0 ) return end();
    std::pair<size_type, size_type> pos = find_position(key);
    if ( pos.first == ILLEGAL_BUCKET )     // alas, not there
      return end();
    else
      return const_iterator(this, table + pos.first, table+num_buckets, false);
  }

  // This is a tr1 method: the bucket a given key is in, or what bucket
  // it would be put in, if it were to be inserted.  Shrug.
  size_type bucket(const key_type& key) const {
    std::pair<size_type, size_type> pos = find_position(key);
    return pos.first == ILLEGAL_BUCKET ? pos.second : pos.first;
  }

  // Counts how many elements have key key.  For maps, it's either 0 or 1.
  size_type count(const key_type &key) const {
    std::pair<size_type, size_type> pos = find_position(key);
    return pos.first == ILLEGAL_BUCKET ? 0 : 1;
  }

  // Likewise, equal_range doesn't really make sense for us.  Oh well.
  std::pair<iterator,iterator> equal_range(const key_type& key) {
    iterator pos = find(key);      // either an iterator or end
    if (pos == end()) {
      return std::pair<iterator,iterator>(pos, pos);
    } else {
      const iterator startpos = pos++;
      return std::pair<iterator,iterator>(startpos, pos);
    }
  }
  std::pair<const_iterator,const_iterator> equal_range(const key_type& key)
      const {
    const_iterator pos = find(key);      // either an iterator or end
    if (pos == end()) {
      return std::pair<const_iterator,const_iterator>(pos, pos);
    } else {
      const const_iterator startpos = pos++;
      return std::pair<const_iterator,const_iterator>(startpos, pos);
    }
  }

  // INSERTION ROUTINES
 private:
  // Private method used by insert_noresize and find_or_insert.
  iterator insert_at(const_reference obj, size_type pos) {
    if (size() >= max_size()) {
      throw std::length_error("insert overflow");
    }
    if ( test_deleted(pos) ) {      // just replace if it's been del.
      // shrug: shouldn't need to be const.
      const_iterator delpos(this, table + pos, table + num_buckets, false);
      clear_deleted(delpos);
      assert( num_deleted > 0);
      --num_deleted;                // used to be, now it isn't
    } else {
      ++num_elements;               // replacing an empty bucket
    }
    set_value(&table[pos], obj);
    return iterator(this, table + pos, table + num_buckets, false);
  }

  // If you know *this is big enough to hold obj, use this routine
  std::pair<iterator, bool> insert_noresize(const_reference obj) {
    // First, double-check we're not inserting delkey or emptyval
    assert((!settings.use_empty() || !equals(get_key(obj),
                                             get_key(val_info.emptyval)))
           && "Inserting the empty key");
    assert((!settings.use_deleted() || !equals(get_key(obj), key_info.delkey))
           && "Inserting the deleted key");
    const std::pair<size_type,size_type> pos = find_position(get_key(obj));
    if ( pos.first != ILLEGAL_BUCKET) {      // object was already there
      return std::pair<iterator,bool>(iterator(this, table + pos.first,
                                          table + num_buckets, false),
                                 false);          // false: we didn't insert
    } else {                                 // pos.second says where to put it
      return std::pair<iterator,bool>(insert_at(obj, pos.second), true);
    }
  }

  // Specializations of insert(it, it) depending on the power of the iterator:
  // (1) Iterator supports operator-, resize before inserting
  template <class ForwardIterator>
  void insert(ForwardIterator f, ForwardIterator l, std::forward_iterator_tag) {
    size_t dist = std::distance(f, l);
    if (dist >= (std::numeric_limits<size_type>::max)()) {
      throw std::length_error("insert-range overflow");
    }
    resize_delta(static_cast<size_type>(dist));
    for ( ; dist > 0; --dist, ++f) {
      insert_noresize(*f);
    }
  }

  // (2) Arbitrary iterator, can't tell how much to resize
  template <class InputIterator>
  void insert(InputIterator f, InputIterator l, std::input_iterator_tag) {
    for ( ; f != l; ++f)
      insert(*f);
  }

 public:
  // This is the normal insert routine, used by the outside world
  std::pair<iterator, bool> insert(const_reference obj) {
    resize_delta(1);                      // adding an object, grow if need be
    return insert_noresize(obj);
  }

  // When inserting a lot at a time, we specialize on the type of iterator
  template <class InputIterator>
  void insert(InputIterator f, InputIterator l) {
    // specializes on iterator type
    insert(f, l,
           typename std::iterator_traits<InputIterator>::iterator_category());
  }

  // DefaultValue is a functor that takes a key and returns a value_type
  // representing the default value to be inserted if none is found.
  template <class DefaultValue>
  value_type& find_or_insert(const key_type& key) {
    // First, double-check we're not inserting emptykey or delkey
    assert((!settings.use_empty() || !equals(key, get_key(val_info.emptyval)))
           && "Inserting the empty key");
    assert((!settings.use_deleted() || !equals(key, key_info.delkey))
           && "Inserting the deleted key");
    const std::pair<size_type,size_type> pos = find_position(key);
    DefaultValue default_value;
    if ( pos.first != ILLEGAL_BUCKET) {  // object was already there
      return table[pos.first];
    } else if (resize_delta(1)) {        // needed to rehash to make room
      // Since we resized, we can't use pos, so recalculate where to insert.
      return *insert_noresize(default_value(key)).first;
    } else {                             // no need to rehash, insert right here
      return *insert_at(default_value(key), pos.second);
    }
  }

  // DELETION ROUTINES
  size_type erase(const key_type& key) {
    // First, double-check we're not trying to erase delkey or emptyval.
    assert((!settings.use_empty() || !equals(key, get_key(val_info.emptyval)))
           && "Erasing the empty key");
    assert((!settings.use_deleted() || !equals(key, key_info.delkey))
           && "Erasing the deleted key");
    const_iterator pos = find(key);   // shrug: shouldn't need to be const
    if ( pos != end() ) {
      assert(!test_deleted(pos));  // or find() shouldn't have returned it
      set_deleted(pos);
      ++num_deleted;
      settings.set_consider_shrink(true); // will think about shrink after next insert
      return 1;                    // because we deleted one thing
    } else {
      return 0;                    // because we deleted nothing
    }
  }

  // We return the iterator past the deleted item.
  void erase(iterator pos) {
    if ( pos == end() ) return;    // sanity check
    if ( set_deleted(pos) ) {      // true if object has been newly deleted
      ++num_deleted;
      settings.set_consider_shrink(true); // will think about shrink after next insert
    }
  }

  void erase(iterator f, iterator l) {
    for ( ; f != l; ++f) {
      if ( set_deleted(f)  )       // should always be true
        ++num_deleted;
    }
    settings.set_consider_shrink(true); // will think about shrink after next insert
  }

  // We allow you to erase a const_iterator just like we allow you to
  // erase an iterator.  This is in parallel to 'delete': you can delete
  // a const pointer just like a non-const pointer.  The logic is that
  // you can't use the object after it's erased anyway, so it doesn't matter
  // if it's const or not.
  void erase(const_iterator pos) {
    if ( pos == end() ) return;    // sanity check
    if ( set_deleted(pos) ) {      // true if object has been newly deleted
      ++num_deleted;
      settings.set_consider_shrink(true); // will think about shrink after next insert
    }
  }
  void erase(const_iterator f, const_iterator l) {
    for ( ; f != l; ++f) {
      if ( set_deleted(f)  )       // should always be true
        ++num_deleted;
    }
    settings.set_consider_shrink(true);   // will think about shrink after next insert
  }

  // COMPARISON
  bool operator==(const dense_hashtable& ht) const {
    if (size() != ht.size()) {
      return false;
    } else if (this == &ht) {
      return true;
    } else {
      // Iterate through the elements in "this" and see if the
      // corresponding element is in ht
      for ( const_iterator it = begin(); it != end(); ++it ) {
        const_iterator it2 = ht.find(get_key(*it));
        if ((it2 == ht.end()) || (*it != *it2)) {
          return false;
        }
      }
      return true;
    }
  }
  bool operator!=(const dense_hashtable& ht) const {
    return !(*this == ht);
  }

  // I/O
  // We support reading and writing hashtables to disk.  Alas, since
  // I don't know how to write a hasher or key_equal, you have to make
  // sure everything but the table is the same.  We compact before writing.
 private:
  // Every time the disk format changes, this should probably change too
  typedef unsigned long MagicNumberType;
  static const MagicNumberType MAGIC_NUMBER = 0x13578642;

 public:
  // I/O -- this is an add-on for writing hash table to disk
  //
  // INPUT and OUTPUT must be either a FILE, *or* a C++ stream
  //    (istream, ostream, etc) *or* a class providing
  //    Read(void*, size_t) and Write(const void*, size_t)
  //    (respectively), which writes a buffer into a stream
  //    (which the INPUT/OUTPUT instance presumably owns).

  typedef sparsehash_internal::pod_serializer<value_type> NopointerSerializer;

  // ValueSerializer: a functor.  operator()(OUTPUT*, const value_type&)
  template <typename ValueSerializer, typename OUTPUT>
  bool serialize(ValueSerializer serializer, OUTPUT *fp) {
    squash_deleted();           // so we don't have to worry about delkey
    if ( !sparsehash_internal::write_bigendian_number(fp, MAGIC_NUMBER, 4) )
      return false;
    if ( !sparsehash_internal::write_bigendian_number(fp, num_buckets, 8) )
      return false;
    if ( !sparsehash_internal::write_bigendian_number(fp, num_elements, 8) )
      return false;
    // Now write a bitmap of non-empty buckets.
    for ( size_type i = 0; i < num_buckets; i += 8 ) {
      unsigned char bits = 0;
      for ( int bit = 0; bit < 8; ++bit ) {
        if ( i + bit < num_buckets && !test_empty(i + bit) )
          bits |= (1 << bit);
      }
      if ( !sparsehash_internal::write_data(fp, &bits, sizeof(bits)) )
        return false;
      for ( int bit = 0; bit < 8; ++bit ) {
        if ( bits & (1 << bit) ) {
          if ( !serializer(fp, table[i + bit]) ) return false;
        }
      }
    }
    return true;
  }

  // INPUT: anything we've written an overload of read_data() for.
  // ValueSerializer: a functor.  operator()(INPUT*, value_type*)
  template <typename ValueSerializer, typename INPUT>
  bool unserialize(ValueSerializer serializer, INPUT *fp) {
    assert(settings.use_empty() && "empty_key not set for read");

    clear();                        // just to be consistent
    MagicNumberType magic_read;
    if ( !sparsehash_internal::read_bigendian_number(fp, &magic_read, 4) )
      return false;
    if ( magic_read != MAGIC_NUMBER ) {
      return false;
    }
    size_type new_num_buckets;
    if ( !sparsehash_internal::read_bigendian_number(fp, &new_num_buckets, 8) )
      return false;
    clear_to_size(new_num_buckets);
    if ( !sparsehash_internal::read_bigendian_number(fp, &num_elements, 8) )
      return false;

    // Read the bitmap of non-empty buckets.
    for (size_type i = 0; i < num_buckets; i += 8) {
      unsigned char bits;
      if ( !sparsehash_internal::read_data(fp, &bits, sizeof(bits)) )
        return false;
      for ( int bit = 0; bit < 8; ++bit ) {
        if ( i + bit < num_buckets && (bits & (1 << bit)) ) {  // not empty
          if ( !serializer(fp, &table[i + bit]) ) return false;
        }
      }
    }
    return true;
  }

 private:
  template <class A>
  class alloc_impl : public A {
   public:
    typedef typename A::pointer pointer;
    typedef typename A::size_type size_type;

    // Convert a normal allocator to one that has realloc_or_die()
    alloc_impl(const A& a) : A(a) { }

    // realloc_or_die should only be used when using the default
    // allocator (libc_allocator_with_realloc).
    pointer realloc_or_die(pointer /*ptr*/, size_type /*n*/) {
      fprintf(stderr, "realloc_or_die is only supported for "
                      "libc_allocator_with_realloc\n");
      exit(1);
      return NULL;
    }
  };

  // A template specialization of alloc_impl for
  // libc_allocator_with_realloc that can handle realloc_or_die.
  template <class A>
  class alloc_impl<libc_allocator_with_realloc<A> >
      : public libc_allocator_with_realloc<A> {
   public:
    typedef typename libc_allocator_with_realloc<A>::pointer pointer;
    typedef typename libc_allocator_with_realloc<A>::size_type size_type;

    alloc_impl(const libc_allocator_with_realloc<A>& a)
        : libc_allocator_with_realloc<A>(a) { }

    pointer realloc_or_die(pointer ptr, size_type n) {
      pointer retval = this->reallocate(ptr, n);
      if (retval == NULL) {
        fprintf(stderr,
                "sparsehash: FATAL ERROR: failed to reallocate "
                "%lu elements for ptr %p",
                static_cast<unsigned long>(n), static_cast<void*>(ptr));
        exit(1);
      }
      return retval;
    }
  };

  // Package allocator with emptyval to eliminate memory needed for
  // the zero-size allocator.
  // If new fields are added to this class, we should add them to
  // operator= and swap.
  class ValInfo : public alloc_impl<value_alloc_type> {
   public:
    typedef typename alloc_impl<value_alloc_type>::value_type value_type;

    ValInfo(const alloc_impl<value_alloc_type>& a)
        : alloc_impl<value_alloc_type>(a), emptyval() { }
    ValInfo(const ValInfo& v)
        : alloc_impl<value_alloc_type>(v), emptyval(v.emptyval) { }

    value_type emptyval;    // which key marks unused entries
  };

  // Package functors with another class to eliminate memory needed for
  // zero-size functors.  Since ExtractKey and hasher's operator() might
  // have the same function signature, they must be packaged in
  // different classes.
  struct Settings :
      sparsehash_internal::sh_hashtable_settings<key_type, hasher,
                                                 size_type, HT_MIN_BUCKETS> {
    explicit Settings(const hasher& hf)
        : sparsehash_internal::sh_hashtable_settings<key_type, hasher,
                                                     size_type, HT_MIN_BUCKETS>(
            hf, HT_OCCUPANCY_PCT / 100.0f, HT_EMPTY_PCT / 100.0f) {}
  };

  // Packages ExtractKey and SetKey functors.
  class KeyInfo : public ExtractKey, public SetKey, public EqualKey {
   public:
    KeyInfo(const ExtractKey& ek, const SetKey& sk, const EqualKey& eq)
        : ExtractKey(ek),
          SetKey(sk),
          EqualKey(eq) {
    }

    // We want to return the exact same type as ExtractKey: Key or const Key&
    typename ExtractKey::result_type get_key(const_reference v) const {
      return ExtractKey::operator()(v);
    }
    void set_key(pointer v, const key_type& k) const {
      SetKey::operator()(v, k);
    }
    bool equals(const key_type& a, const key_type& b) const {
      return EqualKey::operator()(a, b);
    }

    // Which key marks deleted entries.
    // TODO(csilvers): make a pointer, and get rid of use_deleted (benchmark!)
    typename base::remove_const<key_type>::type delkey;
  };

  // Utility functions to access the templated operators
  size_type hash(const key_type& v) const {
    return settings.hash(v);
  }
  bool equals(const key_type& a, const key_type& b) const {
    return key_info.equals(a, b);
  }
  typename ExtractKey::result_type get_key(const_reference v) const {
    return key_info.get_key(v);
  }
  void set_key(pointer v, const key_type& k) const {
    key_info.set_key(v, k);
  }

 private:
  // Actual data
  Settings settings;
  KeyInfo key_info;

  size_type num_deleted;  // how many occupied buckets are marked deleted
  size_type num_elements;
  size_type num_buckets;
  ValInfo val_info;       // holds emptyval, and also the allocator
  pointer table;
};

// We need a global swap as well
template <class V, class K, class HF, class ExK, class SetK, class EqK, class A>
inline void swap(dense_hashtable<V,K,HF,ExK,SetK,EqK,A> &x,
                 dense_hashtable<V,K,HF,ExK,SetK,EqK,A> &y) {
  x.swap(y);
}

#undef JUMP_

template <class V, class K, class HF, class ExK, class SetK, class EqK, class A>
const typename dense_hashtable<V,K,HF,ExK,SetK,EqK,A>::size_type
  dense_hashtable<V,K,HF,ExK,SetK,EqK,A>::ILLEGAL_BUCKET;

// How full we let the table get before we resize.  Knuth says .8 is
// good -- higher causes us to probe too much, though saves memory.
// However, we go with .5, getting better performance at the cost of
// more space (a trade-off densehashtable explicitly chooses to make).
// Feel free to play around with different values, though, via
// max_load_factor() and/or set_resizing_parameters().
template <class V, class K, class HF, class ExK, class SetK, class EqK, class A>
const int dense_hashtable<V,K,HF,ExK,SetK,EqK,A>::HT_OCCUPANCY_PCT = 50;

// How empty we let the table get before we resize lower.
// It should be less than OCCUPANCY_PCT / 2 or we thrash resizing.
template <class V, class K, class HF, class ExK, class SetK, class EqK, class A>
const int dense_hashtable<V,K,HF,ExK,SetK,EqK,A>::HT_EMPTY_PCT
  = static_cast<int>(0.4 *
                     dense_hashtable<V,K,HF,ExK,SetK,EqK,A>::HT_OCCUPANCY_PCT);

_END_GOOGLE_NAMESPACE_

#endif /* _DENSEHASHTABLE_H_ */

_START_GOOGLE_NAMESPACE_

template <class Key,
          class T,
          class HashFcn = SPARSEHASH_HASH<Key>,  // defined in sparseconfig.h
          class EqualKey = std::equal_to<Key>,
          class Alloc = libc_allocator_with_realloc<std::pair<const Key, T> > >
class dense_hash_map {
   private:
    // Apparently select1st is not stl-standard, so we define our own
    struct SelectKey {
        typedef const Key& result_type;
        const Key& operator()(const std::pair<const Key, T>& p) const { return p.first; }
    };
    struct SetKey {
        void operator()(std::pair<const Key, T>* value, const Key& new_key) const {
            *const_cast<Key*>(&value->first) = new_key;
            // It would be nice to clear the rest of value here as well, in
            // case it's taking up a lot of memory.  We do this by clearing
            // the value.  This assumes T has a zero-arg constructor!
            value->second = T();
        }
    };
    // For operator[].
    struct DefaultValue {
        std::pair<const Key, T> operator()(const Key& key) { return std::make_pair(key, T()); }
    };

    // The actual data
    typedef dense_hashtable<std::pair<const Key, T>, Key, HashFcn, SelectKey, SetKey, EqualKey, Alloc> ht;
    ht rep;

   public:
    typedef typename ht::key_type key_type;
    typedef T data_type;
    typedef T mapped_type;
    typedef typename ht::value_type value_type;
    typedef typename ht::hasher hasher;
    typedef typename ht::key_equal key_equal;
    typedef Alloc allocator_type;

    typedef typename ht::size_type size_type;
    typedef typename ht::difference_type difference_type;
    typedef typename ht::pointer pointer;
    typedef typename ht::const_pointer const_pointer;
    typedef typename ht::reference reference;
    typedef typename ht::const_reference const_reference;

    typedef typename ht::iterator iterator;
    typedef typename ht::const_iterator const_iterator;
    typedef typename ht::local_iterator local_iterator;
    typedef typename ht::const_local_iterator const_local_iterator;

    // Iterator functions
    iterator begin() { return rep.begin(); }
    iterator end() { return rep.end(); }
    const_iterator begin() const { return rep.begin(); }
    const_iterator end() const { return rep.end(); }

    // These come from tr1's unordered_map. For us, a bucket has 0 or 1 elements.
    local_iterator begin(size_type i) { return rep.begin(i); }
    local_iterator end(size_type i) { return rep.end(i); }
    const_local_iterator begin(size_type i) const { return rep.begin(i); }
    const_local_iterator end(size_type i) const { return rep.end(i); }

    // Accessor functions
    allocator_type get_allocator() const { return rep.get_allocator(); }
    hasher hash_funct() const { return rep.hash_funct(); }
    hasher hash_function() const { return hash_funct(); }
    key_equal key_eq() const { return rep.key_eq(); }

    // Constructors
    explicit dense_hash_map(size_type expected_max_items_in_table = 0,
                            const hasher& hf = hasher(),
                            const key_equal& eql = key_equal(),
                            const allocator_type& alloc = allocator_type())
        : rep(expected_max_items_in_table, hf, eql, SelectKey(), SetKey(), alloc) {}

    template <class InputIterator>
    dense_hash_map(InputIterator f,
                   InputIterator l,
                   const key_type& empty_key_val,
                   const key_type& deleted_key_val, //added
                   size_type expected_max_items_in_table = 0,
                   const hasher& hf = hasher(),
                   const key_equal& eql = key_equal(),
                   const allocator_type& alloc = allocator_type())
        : rep(expected_max_items_in_table, hf, eql, SelectKey(), SetKey(), alloc) {
        set_empty_key(empty_key_val);
        set_deleted_key(deleted_key_val); //added
        rep.insert(f, l);
    }

    /* added */
    dense_hash_map(std::initializer_list<std::pair<const key_type, data_type> > _Ilist,
                   const key_type& empty_key_val,
                   const key_type& deleted_key_val,
                   size_type expected_max_items_in_table = 0,
                   const hasher& hf = hasher(),
                   const key_equal& eql = key_equal(),
                   const allocator_type& alloc = allocator_type())
        : rep(expected_max_items_in_table, hf, eql, SelectKey(), SetKey(), alloc) {
        set_empty_key(empty_key_val);
        set_deleted_key(deleted_key_val);
        rep.insert(_Ilist.begin(), _Ilist.end());
    }

    template <typename T, typename U>
    void emplace(const T& key, const U& val) {
        this->operator[](static_cast<key_type>(key)) = static_cast<data_type>(val);
    }
    void emplace(const key_type& key, const data_type& val) { this->operator[](key) = val; }

    void reserve(size_type hint) { rep.resize(hint); } //added
    /* end of added */

    // We use the default copy constructor
    // We use the default operator=()
    // We use the default destructor

    void clear() { rep.clear(); }
    // This clears the hash map without resizing it down to the minimum
    // bucket count, but rather keeps the number of buckets constant
    void clear_no_resize() { rep.clear_no_resize(); }
    void swap(dense_hash_map& hs) { rep.swap(hs.rep); }

    // Functions concerning size
    size_type size() const { return rep.size(); }
    size_type max_size() const { return rep.max_size(); }
    bool empty() const { return rep.empty(); }
    size_type bucket_count() const { return rep.bucket_count(); }
    size_type max_bucket_count() const { return rep.max_bucket_count(); }

    // These are tr1 methods.  bucket() is the bucket the key is or would be in.
    size_type bucket_size(size_type i) const { return rep.bucket_size(i); }
    size_type bucket(const key_type& key) const { return rep.bucket(key); }
    float load_factor() const { return size() * 1.0f / bucket_count(); }
    float max_load_factor() const {
        float shrink, grow;
        rep.get_resizing_parameters(&shrink, &grow);
        return grow;
    }
    void max_load_factor(float new_grow) {
        float shrink, grow;
        rep.get_resizing_parameters(&shrink, &grow);
        rep.set_resizing_parameters(shrink, new_grow);
    }
    // These aren't tr1 methods but perhaps ought to be.
    float min_load_factor() const {
        float shrink, grow;
        rep.get_resizing_parameters(&shrink, &grow);
        return shrink;
    }
    void min_load_factor(float new_shrink) {
        float shrink, grow;
        rep.get_resizing_parameters(&shrink, &grow);
        rep.set_resizing_parameters(new_shrink, grow);
    }
    // Deprecated; use min_load_factor() or max_load_factor() instead.
    void set_resizing_parameters(float shrink, float grow) { rep.set_resizing_parameters(shrink, grow); }

    void resize(size_type hint) { rep.resize(hint); }
    void rehash(size_type hint) { resize(hint); }  // the tr1 name

    // Lookup routines
    iterator find(const key_type& key) { return rep.find(key); }
    const_iterator find(const key_type& key) const { return rep.find(key); }

    data_type& operator[](const key_type& key) {  // This is our value-add!
        // If key is in the hashtable, returns find(key)->second,
        // otherwise returns insert(value_type(key, T()).first->second.
        // Note it does not create an empty T unless the find fails.
        return rep.template find_or_insert<DefaultValue>(key).second;
    }

    size_type count(const key_type& key) const { return rep.count(key); }

    std::pair<iterator, iterator> equal_range(const key_type& key) { return rep.equal_range(key); }
    std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const { return rep.equal_range(key); }

    // Insertion routines
    std::pair<iterator, bool> insert(const value_type& obj) { return rep.insert(obj); }
    template <class InputIterator>
    void insert(InputIterator f, InputIterator l) {
        rep.insert(f, l);
    }
    void insert(const_iterator f, const_iterator l) { rep.insert(f, l); }
    // Required for std::insert_iterator; the passed-in iterator is ignored.
    iterator insert(iterator, const value_type& obj) { return insert(obj).first; }

    // Deletion and empty routines
    // THESE ARE NON-STANDARD!  I make you specify an "impossible" key
    // value to identify deleted and empty buckets.  You can change the
    // deleted key as time goes on, or get rid of it entirely to be insert-only.
    void set_empty_key(const key_type& key) {             // YOU MUST CALL THIS!
        rep.set_empty_key(value_type(key, data_type()));  // rep wants a value
    }
    key_type empty_key() const {
        return rep.empty_key().first;  // rep returns a value
    }

    void set_deleted_key(const key_type& key) { rep.set_deleted_key(key); }
    void clear_deleted_key() { rep.clear_deleted_key(); }
    key_type deleted_key() const { return rep.deleted_key(); }

    // These are standard
    size_type erase(const key_type& key) { return rep.erase(key); }
    void erase(iterator it) { rep.erase(it); }
    void erase(iterator f, iterator l) { rep.erase(f, l); }

    // Comparison
    bool operator==(const dense_hash_map& hs) const { return rep == hs.rep; }
    bool operator!=(const dense_hash_map& hs) const { return rep != hs.rep; }

    // I/O -- this is an add-on for writing hash map to disk
    //
    // For maximum flexibility, this does not assume a particular
    // file type (though it will probably be a FILE *).  We just pass
    // the fp through to rep.

    // If your keys and values are simple enough, you can pass this
    // serializer to serialize()/unserialize().  "Simple enough" means
    // value_type is a POD type that contains no pointers.  Note,
    // however, we don't try to normalize endianness.
    typedef typename ht::NopointerSerializer NopointerSerializer;

    // serializer: a class providing operator()(OUTPUT*, const value_type&)
    //    (writing value_type to OUTPUT).  You can specify a
    //    NopointerSerializer object if appropriate (see above).
    // fp: either a FILE*, OR an ostream*/subclass_of_ostream*, OR a
    //    pointer to a class providing size_t Write(const void*, size_t),
    //    which writes a buffer into a stream (which fp presumably
    //    owns) and returns the number of bytes successfully written.
    //    Note basic_ostream<not_char> is not currently supported.
    template <typename ValueSerializer, typename OUTPUT>
    bool serialize(ValueSerializer serializer, OUTPUT* fp) {
        return rep.serialize(serializer, fp);
    }

    // serializer: a functor providing operator()(INPUT*, value_type*)
    //    (reading from INPUT and into value_type).  You can specify a
    //    NopointerSerializer object if appropriate (see above).
    // fp: either a FILE*, OR an istream*/subclass_of_istream*, OR a
    //    pointer to a class providing size_t Read(void*, size_t),
    //    which reads into a buffer from a stream (which fp presumably
    //    owns) and returns the number of bytes successfully read.
    //    Note basic_istream<not_char> is not currently supported.
    // NOTE: Since value_type is std::pair<const Key, T>, ValueSerializer
    // may need to do a const cast in order to fill in the key.
    template <typename ValueSerializer, typename INPUT>
    bool unserialize(ValueSerializer serializer, INPUT* fp) {
        return rep.unserialize(serializer, fp);
    }
};

// We need a global swap as well
template <class Key, class T, class HashFcn, class EqualKey, class Alloc>
inline void swap(dense_hash_map<Key, T, HashFcn, EqualKey, Alloc>& hm1,
                 dense_hash_map<Key, T, HashFcn, EqualKey, Alloc>& hm2) {
    hm1.swap(hm2);
}

_END_GOOGLE_NAMESPACE_

#endif /* _DENSE_HASH_MAP_H_ */
