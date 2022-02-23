/* container.hpp | MIT License | https://github.com/kirin123kirin/cutil/raw/main/LICENSE */
#ifndef CONTAINER_HPP
#define CONTAINER_HPP

#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <bitset>
#include <new>

#if __has_include("rpmalloc.hpp")
#include "rpmalloc.hpp"
#endif

#if __has_include("sparsehash/dense_hash_map")
#include "sparsehash/dense_hash_map"
#endif

#if __has_include("sparsehash/dense_hash_set")
#include "sparsehash/dense_hash_set"
#endif

#if _WIN32 || _WIN64
#include <malloc.h>
#else
#include <alloca.h>
#endif

#if _WIN32 || _WIN64
#undef max
#endif

#ifdef Py_PYTHON_H
#define CMALLOC PyMem_Malloc
#define CREALLOC PyMem_Realloc
#define CCALLOC PyMem_Calloc
#define CFREE PyMem_Free
#ifdef RPMALLOC_HPP
#undef RPMALLOC_HPP
#endif
#elif defined RPMALLOC_HPP
#define CMALLOC rpmalloc
#define CREALLOC rprealloc
#define CCALLOC rpcalloc
#define CFREE rpfree
#else
#define CMALLOC malloc
#define CREALLOC realloc
#define CCALLOC calloc
#define CFREE free
#endif


#ifdef _WIN32

void
operator delete(void* p) noexcept {
	CFREE(p);
}

void
operator delete[](void* p) noexcept {
	CFREE(p);
}

void*
operator new(std::size_t size) noexcept(false) {
	return CMALLOC(size);
}

void*
operator new[](std::size_t size) noexcept(false) {
	return CMALLOC(size);
}

void*
operator new(std::size_t size, const std::nothrow_t& tag) noexcept {
	(void)sizeof(tag);
	return CMALLOC(size);
}

void*
operator new[](std::size_t size, const std::nothrow_t& tag) noexcept {
	(void)sizeof(tag);
	return CMALLOC(size);
}

#if (__cplusplus >= 201402L || _MSC_VER >= 1916)

void
operator delete(void* p, std::size_t size) noexcept {
	(void)sizeof(size);
	CFREE(p);
}

void
operator delete[](void* p, std::size_t size) noexcept {
	(void)sizeof(size);
	CFREE(p);
}

#endif

#if (__cplusplus > 201402L || defined(__cpp_aligned_new))

void
operator delete(void* p, std::align_val_t align) noexcept {
	(void)sizeof(align);
	CFREE(p);
}

void
operator delete[](void* p, std::align_val_t align) noexcept {
	(void)sizeof(align);
	CFREE(p);
}

void
operator delete(void* p, std::size_t size, std::align_val_t align) noexcept {
	(void)sizeof(size);
	(void)sizeof(align);
	CFREE(p);
}

void
operator delete[](void* p, std::size_t size, std::align_val_t align) noexcept {
	(void)sizeof(size);
	(void)sizeof(align);
	CFREE(p);
}

void*
operator new(std::size_t size, std::align_val_t align) noexcept(false) {
	return rpaligned_alloc((size_t)align, size);
}

void*
operator new[](std::size_t size, std::align_val_t align) noexcept(false) {
	return rpaligned_alloc((size_t)align, size);
}

void*
operator new(std::size_t size, std::align_val_t align, const std::nothrow_t& tag) noexcept {
	(void)sizeof(tag);
	return rpaligned_alloc((size_t)align, size);
}

void*
operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t& tag) noexcept {
	(void)sizeof(tag);
	return rpaligned_alloc((size_t)align, size);
}

#endif

#endif

namespace gm {

#define DEFAULT_BLOCK_SIZE 1024 * 16
#define DEFAULT_STACK_UNIT_LENGTH 1024 * 4

// static bool initialized = false;

// #ifdef Py_PYTHON_H
// void initializer() {
//     if(!Py_IsInitialized()) {
//         Py_Initialize();
//         initialized = true;
//     }
// }
// void finalizer() {
//     if(initialized) {
//         Py_Finalize();
//         initialized = false;
//     }
// }
// #elif defined RPMALLOC_HPP
// inline void initializer() {
//     if(initialized == false) {
//         rpmalloc_initialize();
//         initialized = true;
//     }
// }
// inline void finalizer() {
//     if(initialized) {
//         rpmalloc_finalize();
//         initialized = false;
//     }
// }
// #else
// void initializer() {
//     initialized = true;
// }
// void finalizer() {
//     initialized = false;
// }
// #endif

template <typename T>
class MemPool {
   public:
    typedef T value_type;
    typedef std::size_t size_type;

    template <typename U>
    struct rebind {
        typedef MemPool<U> other;
    };

    /* Member functions */
    MemPool() noexcept {
        curBlk = nullptr;
        curSlot = nullptr;
        lastSlot = nullptr;
        freeSlots = nullptr;
    }

    template <typename U>
    MemPool(const MemPool<U>& other) noexcept : MemPool() {}

    template <typename U>
    MemPool(MemPool<U>&& other) noexcept {
        curBlk = other.curBlk;
        other.curBlk = nullptr;
        curSlot = other.curSlot;
        lastSlot = other.lastSlot;
        freeSlots = other.freeSlots;
    }

    ~MemPool() noexcept {
        if(curBlk != nullptr)
            Clear();
    }

    void Clear() {
        slot_p curr = curBlk;
        while(curr != nullptr) {
            slot_p prev = curr->next;
            free(curr);
            curr = prev;
        }
        curBlk = nullptr;
    }

    template <typename U>
    MemPool& operator=(const MemPool<U>& other) = delete;

    template <typename U>
    MemPool& operator=(MemPool<U>&& other) noexcept {
        if(this != &other) {
            std::swap(curBlk, other.curBlk);
            curSlot = other.curSlot;
            lastSlot = other.lastSlot;
            freeSlots = other.freeSlots;
        }
        return *this;
    }

    T* Mem_New(size_type n = 1) {
        if(freeSlots != nullptr) {
            T* result = reinterpret_cast<T*>(freeSlots);
            freeSlots = freeSlots->next;
            return result;
        } else {
            if(curSlot >= lastSlot)
                New_Block();
            return reinterpret_cast<T*>(curSlot++);
        }
    }

    T* Mem_Rel(T* p, size_type n = 1) { return Mem_New(n); }

    void Mem_Del(T* p) {
        if(p != nullptr) {
            reinterpret_cast<slot_p>(p)->next = freeSlots;
            freeSlots = reinterpret_cast<slot_p>(p);
        }
    }

    bool operator==(const MemPool<T>&) { return true; }
    bool operator!=(const MemPool<T>&) { return false; }

   private:
    union Slot {
        value_type element;
        Slot* next;
    };

    typedef char* data_p;
    typedef Slot slot_t;
    typedef Slot* slot_p;

    slot_p curBlk;
    slot_p curSlot;
    slot_p lastSlot;
    slot_p freeSlots;

    void New_Block() {
        data_p newBlk, body;
        slot_p newSlot;
        size_type align = alignof(slot_t), pad;

        void* blk = malloc(DEFAULT_BLOCK_SIZE);
        if(blk == NULL)
            throw std::runtime_error("Failed Memory Pool malloc.");

        newSlot = reinterpret_cast<slot_p>(blk);
        newSlot->next = curBlk;
        curBlk = newSlot;

        newBlk = reinterpret_cast<data_p>(blk);
        body = newBlk + sizeof(slot_p);

        pad = (align - reinterpret_cast<uintptr_t>(body)) % align;

        curSlot = reinterpret_cast<slot_p>(body + pad);
        lastSlot = reinterpret_cast<slot_p>(newBlk + DEFAULT_BLOCK_SIZE - sizeof(slot_t) + 1);
    }

    static_assert(DEFAULT_BLOCK_SIZE >= 2 * sizeof(slot_t), "DEFAULT_BLOCK_SIZE too small.");
};

#if defined(RPMALLOC_HPP) || defined(Py_PYTHON_H)

template <class T>
class allocator {
   public:
    typedef T value_type;
    typedef std::size_t size_type;
    typedef ptrdiff_t difference_type;

    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;

    // allocator() { initializer(); };
    allocator() {};
    allocator(const allocator&) {}
    ~allocator() {}

    pointer allocate(size_type n, const_pointer = 0) {
        if(n > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::bad_array_new_length();
        return static_cast<pointer>(CMALLOC(n * sizeof(value_type)));
    }
    void deallocate(pointer p, size_type) { CFREE(p); }
    pointer reallocate(pointer p, size_type n) {
        return static_cast<pointer>(CREALLOC(static_cast<void*>(p), n * sizeof(value_type)));
    }

    pointer address(reference r) const { return &r; }
    const_pointer address(const_reference r) const { return &r; }

    size_type max_size() const { return static_cast<size_type>(-1) / sizeof(value_type); }

    void construct(pointer p, const value_type& val) { new(p) value_type(val); }
    void destroy(pointer p) { p->~value_type(); }

    template <class U>
    allocator(const allocator<U>&) {}

    template <class U>
    struct rebind {
        typedef allocator<U> other;
    };

    inline bool operator==(const allocator<T>&) { return true; }

    inline bool operator!=(const allocator<T>&) { return false; }
};

#else
template <class T, class _PoolTy = MemPool<T>>
class allocator {
    using pool_type = _PoolTy;

   public:
    typedef T value_type;
    typedef std::size_t size_type;
    typedef ptrdiff_t difference_type;

    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;

    static pool_type pool;

    allocator(){};
    allocator(const allocator&) {}
    ~allocator() {}

    pointer allocate(size_type n, const_pointer = 0) {
        if(n > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::bad_array_new_length();
        return static_cast<pointer>(pool.Mem_New(n * sizeof(value_type)));
    }
    void deallocate(pointer p, size_type) { pool.Mem_Del(p); }
    pointer reallocate(pointer p, size_type n) {
        return static_cast<pointer>(pool.Mem_Rel(static_cast<void*>(p), n * sizeof(value_type)));
    }

    pointer address(reference r) const { return &r; }
    const_pointer address(const_reference r) const { return &r; }

    size_type max_size() const { return static_cast<size_type>(-1) / sizeof(value_type); }

    void construct(pointer p, const value_type& val) { new(p) value_type(val); }
    void destroy(pointer p) { p->~value_type(); }

    template <class U>
    allocator(const allocator<U>&) {}

    template <class U>
    struct rebind {
        typedef allocator<U> other;
    };

    inline bool operator==(const allocator<T>&) { return true; }

    inline bool operator!=(const allocator<T>&) { return false; }
};

template <class T, class _PoolTy>
_PoolTy allocator<T, _PoolTy>::pool = _PoolTy();

#endif

template <class _Ty, class _Alloc = allocator<_Ty>>
using vector = std::vector<_Ty, _Alloc>;

template <class _Kty,
          class _Ty,
          class _Hasher = std::hash<_Kty>,
          class _Keyeq = std::equal_to<_Kty>,
          class _Alloc = allocator<std::pair<const _Kty, _Ty>>>
using unordered_map = std::unordered_map<_Kty, _Ty, _Hasher, _Keyeq, _Alloc>;

template <class _Kty,
          class _Ty,
          class _Hasher = std::hash<_Kty>,
          class _Keyeq = std::equal_to<_Kty>,
          class _Alloc = allocator<std::pair<const _Kty, _Ty>>>
using unordered_multimap = std::unordered_multimap<_Kty, _Ty, _Hasher, _Keyeq, _Alloc>;

template <class _Kty,
          class _Hasher = std::hash<_Kty>,
          class _Keyeq = std::equal_to<_Kty>,
          class _Alloc = allocator<_Kty>>
using unordered_multiset = std::unordered_multiset<_Kty, _Hasher, _Keyeq, _Alloc>;

template <class _Kty, class _Ty, class _Pr = std::less<_Kty>, class _Alloc = allocator<std::pair<const _Kty, _Ty>>>
using multimap = std::multimap<_Kty, _Ty, _Pr, _Alloc>;

template <class _Kty,
          class _Hasher = std::hash<_Kty>,
          class _Keyeq = std::equal_to<_Kty>,
          class _Alloc = allocator<_Kty>>
using unordered_set = std::unordered_set<_Kty, _Hasher, _Keyeq, _Alloc>;

#ifdef _DENSE_HASH_MAP_H_
template <class Key,
          class T,
          class HashFcn = std::hash<Key>,
          class EqualKey = std::equal_to<Key>,
          class Alloc = allocator<std::pair<const Key, T>>>
using dense_hash_map = google::dense_hash_map<Key, T, HashFcn, EqualKey, Alloc>;
#endif

#ifdef _DENSE_HASH_SET_H_
template <class Value,
          class HashFcn = std::hash<Value>,
          class EqualKey = std::equal_to<Value>,
          class Alloc = allocator<Value>>
using dense_hash_set = google::dense_hash_set<Value, HashFcn, EqualKey, Alloc>;
#endif

template <class _Kty, class _Ty, class _Pr = std::less<_Kty>, class _Alloc = allocator<std::pair<const _Kty, _Ty>>>
using map = std::map<_Kty, _Ty, _Pr, _Alloc>;

template <class _Kty, class _Pr = std::less<_Kty>, class _Alloc = allocator<_Kty>>
using set = std::set<_Kty, _Pr, _Alloc>;

template <class _Ty,
          class _Container = std::vector<_Ty, allocator<_Ty>>,
          class _Pr = std::less<typename _Container::value_type>>
using priority_queue = std::priority_queue<_Ty, _Container, _Pr>;

template <class _Ty, class _Alloc = allocator<_Ty>>
using deque = std::deque<_Ty, _Alloc>;

template <class _Ty, class _Container = std::deque<_Ty, allocator<_Ty>>>
using queue = std::queue<_Ty, _Container>;

template <class _Ty, class _Container = std::deque<_Ty, allocator<_Ty>>>
using stack = std::stack<_Ty, _Container>;

template <class _Ty, class _Alloc = allocator<_Ty>>
using list = std::list<_Ty, _Alloc>;

template <class _Ty, class _Alloc = allocator<_Ty>>
using forward_list = std::forward_list<_Ty, _Alloc>;

template <class _Elem, class _Traits = std::char_traits<_Elem>, class _Alloc = allocator<_Elem>>
using basic_string = std::basic_string<_Elem, _Traits, _Alloc>;
// template <class _Elem, class _Traits = std::char_traits<_Elem>, class _Alloc = allocator<_Elem>>
// struct basic_string : public std::basic_string<_Elem, _Traits, _Alloc> {
//     /* thanks for
//     https://qiita.com/rinse_/items/a6b4c543c7bc1d44b536
//     https://amachang.hatenablog.com/entry/20090430/1241071098
//     */
//     using base = std::basic_string<_Elem, _Traits, _Alloc>;
//     using base::base;
//     template <class U, class V>
//     operator std::basic_string<_Elem, U, V>() {
//         return this->data();
//     }
// };

using string = basic_string<char>;
using wstring = basic_string<wchar_t>;
#ifdef __cpp_lib_char8_t
using u8string = basic_string<char8_t>;
#endif  // __cpp_lib_char8_t
using u16string = basic_string<char16_t>;
using u32string = basic_string<char32_t>;

template <class _Elem, class _Traits, class _Alloc = allocator<_Elem>>
using basic_istringstream = std::basic_istringstream<_Elem, _Traits, _Alloc>;
using istringstream = basic_istringstream<char, std::char_traits<char>>;
using wistringstream = basic_istringstream<wchar_t, std::char_traits<wchar_t>>;

template <class _Elem, class _Traits, class _Alloc = allocator<_Elem>>
using basic_ostringstream = std::basic_ostringstream<_Elem, _Traits, _Alloc>;
using ostringstream = basic_ostringstream<char, std::char_traits<char>>;
using wostringstream = basic_ostringstream<wchar_t, std::char_traits<wchar_t>>;

template <class _Elem, class _Traits, class _Alloc = allocator<_Elem>>
using basic_stringstream = std::basic_stringstream<_Elem, _Traits, _Alloc>;
using stringstream = basic_stringstream<char, std::char_traits<char>>;
using wstringstream = basic_stringstream<wchar_t, std::char_traits<wchar_t>>;

template <class _Elem, class _Traits, class _Alloc = allocator<_Elem>>
using basic_stringbuf = std::basic_stringbuf<_Elem, _Traits, _Alloc>;
using stringbuf = basic_stringbuf<char, std::char_traits<char>>;
using wstringbuf = basic_stringbuf<wchar_t, std::char_traits<wchar_t>>;

/*  Stack Allocator very small data.
 */

namespace stk {

template <class T>
class StackAllocator {
   public:
    typedef T value_type;
    typedef std::size_t size_type;
    typedef ptrdiff_t difference_type;

    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;

    StackAllocator(){};
    StackAllocator(const StackAllocator&) {}
    ~StackAllocator() {}

    pointer allocate(size_type n, const_pointer = 0) {
        if(n > (DEFAULT_STACK_UNIT_LENGTH / sizeof(T)))
            throw std::bad_array_new_length();
        return static_cast<pointer>(
#if _WIN32 || _WIN64
            _alloca
#else
            alloca
#endif
            (n * sizeof(value_type)));
    }
    void deallocate(pointer p, size_type) {}

    pointer address(reference r) const { return &r; }
    const_pointer address(const_reference r) const { return &r; }

    size_type max_size() const { return static_cast<size_type>(-1) / sizeof(value_type); }

    void construct(pointer p, const value_type& val) { new(p) value_type(val); }
    void destroy(pointer p) { p->~value_type(); }

    template <class U>
    StackAllocator(const StackAllocator<U>&) {}

    template <class U>
    struct rebind {
        typedef StackAllocator<U> other;
    };

    inline bool operator==(const StackAllocator<T>&) { return true; }

    inline bool operator!=(const StackAllocator<T>&) { return false; }
};

template <class _Elem, class _Traits = std::char_traits<_Elem>, class _Alloc = StackAllocator<_Elem>>
using basic_string = std::basic_string<_Elem, _Traits, _Alloc>;
using string = basic_string<char>;
using wstring = basic_string<wchar_t>;
#ifdef __cpp_lib_char8_t
using u8string = basic_string<char8_t>;
#endif  // __cpp_lib_char8_t
using u16string = basic_string<char16_t>;
using u32string = basic_string<char32_t>;

template <class _Elem, class _Traits, class _Alloc = StackAllocator<_Elem>>
using basic_istringstream = std::basic_istringstream<_Elem, _Traits, _Alloc>;
using istringstream = basic_istringstream<char, std::char_traits<char>>;
using wistringstream = basic_istringstream<wchar_t, std::char_traits<wchar_t>>;

template <class _Elem, class _Traits, class _Alloc = StackAllocator<_Elem>>
using basic_ostringstream = std::basic_ostringstream<_Elem, _Traits, _Alloc>;
using ostringstream = basic_ostringstream<char, std::char_traits<char>>;
using wostringstream = basic_ostringstream<wchar_t, std::char_traits<wchar_t>>;

template <class _Elem, class _Traits, class _Alloc = StackAllocator<_Elem>>
using basic_stringstream = std::basic_stringstream<_Elem, _Traits, _Alloc>;
using stringstream = basic_stringstream<char, std::char_traits<char>>;
using wstringstream = basic_stringstream<wchar_t, std::char_traits<wchar_t>>;


};  // namespace stk

};  // namespace gm

template <typename T>
struct nohash {
    using size_type = std::size_t;
    constexpr T operator()(const T& s) const noexcept { return s; }
};

namespace std {

/* thanks for
   https://stackoverflow.com/questions/8157937/how-to-specialize-stdhashkeyoperator-for-user-defined-type-in-unordered
*/
template <>
struct hash<gm::string> {
    std::size_t operator()(const gm::string& x) const { return std::_Hash_array_representation(x.c_str(), x.size()); }
};

template <>
struct hash<gm::wstring> {
    std::size_t operator()(const gm::wstring& x) const { return std::_Hash_array_representation(x.c_str(), x.size()); }
};

#ifdef __cpp_lib_char8_t
template <>
struct hash<gm::char8_t> {
    std::size_t operator()(const gm::char8_t& x) const { return std::_Hash_array_representation(x.c_str(), x.size()); }
};
#endif  // __cpp_lib_char8_t

template <>
struct hash<gm::u16string> {
    std::size_t operator()(const gm::u16string& x) const {
        return std::_Hash_array_representation(x.c_str(), x.size());
    }
};

template <>
struct hash<gm::u32string> {
    std::size_t operator()(const gm::u32string& x) const {
        return std::_Hash_array_representation(x.c_str(), x.size());
    }
};

#ifdef Py_PYTHON_H

template <>
struct hash<PyObject*> {
    constexpr std::size_t operator()(PyObject* x) const noexcept {
        Py_hash_t hash = -1;

        if(!PyUnicode_CheckExact(x))
            hash = PyObject_Hash(x);
        else if((hash = ((PyASCIIObject*)x)->hash) == -1)
            hash = PyObject_Hash(x);

        if(hash == -1) {
            PyObject* tup = PySequence_Tuple(x);
            hash = PyObject_Hash(tup);
            Py_XDECREF(tup);
            PyErr_Clear();
        }
        return static_cast<std::size_t>(hash);
    }
};

#endif

}  // namespace std

#endif /* CONTAINER_HPP */
