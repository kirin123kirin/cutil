/* argparser.hpp | MIT License | https://github.com/kirin123kirin/cutil/raw/main/LICENSE */
#ifndef _ARGPARSE_HPP_
#define _ARGPARSE_HPP_

#include <string.h>
#include <initializer_list>
#include <stdexcept>
#include <typeinfo>
#include <iterator>
#include <type_traits>
#include <string>

#define _NmDefault 256

enum class TP { INTEGER, STRING, BOOLEAN, NONE };

template <typename CharT = char>
struct Option {
    char** data_ = nullptr;
    CharT shortName = 0;
    const CharT* longName = nullptr;
    const CharT* description = nullptr;
    int16_t required = 0;
    TP type = TP::NONE;

    Option() {}
    Option(std::nullptr_t) {}

    template <class T>
    Option(T* _data,
           CharT _shortName = 0,
           const CharT* _longName = NULL,
           const CharT* _description = NULL,
           char _required = 0) {
        operator()(_data, _shortName, _longName, _description, _required);
    }
    template <class T>
    void operator()(T* _data,
                    CharT _shortName = 0,
                    const CharT* _longName = NULL,
                    const CharT* _description = NULL,
                    char _required = 0,
                    TP _type = TP::NONE) {
        data_ = (char**)_data;
        shortName = _shortName;
        longName = _longName;
        description = _description;
        if(_type == TP::NONE) {
            type = typeid(*_data) == typeid(bool) ? TP::BOOLEAN : std::is_integral<T>::value ? TP::INTEGER : TP::STRING;
        } else {
            type = _type;
        }

        if(_required == 1 || _required == 0x31 || _required == 0x2B)
            required = 1;
        else if(_required == 0 || _required == 0x30 || _required == 0x2A)
            required = 0;
        else if(_required == 0x3F)
            required = INT16_MIN;
        else
            required = 0;
    }
    bool operator==(std::nullptr_t) {
        return type == TP::NONE && data_ == nullptr && shortName == 0 && longName == nullptr &&
               description == nullptr && required == 0;
    }
    bool operator!=(std::nullptr_t) { return operator==(nullptr); }
    operator bool() const noexcept {
        if(data_ == nullptr)
            return false;
        else if(type == TP::BOOLEAN)
            return (bool)*data_;
        else if(type == TP::INTEGER)
            return (bool)(int64_t)*data_;
        else if(type == TP::STRING)
            return (bool)(const CharT*)*data_;
        else
            return (bool)*data_;
    }
    operator const CharT*() const noexcept { return (const CharT*)*data_; }
    operator CharT*() const noexcept { return (CharT*)*data_; }
    operator int8_t() const noexcept { return (int8_t)*data_; }
    operator int16_t() const noexcept { return (int16_t)*data_; }
    operator int32_t() const noexcept { return (int32_t)*data_; }
    operator int64_t() const noexcept { return (int64_t)*data_; }
    operator uint8_t() const noexcept { return (uint8_t)*data_; }
    operator uint16_t() const noexcept { return (uint16_t)*data_; }
    operator uint32_t() const noexcept { return (uint32_t)*data_; }
    operator uint64_t() const noexcept { return (uint64_t)*data_; }
};

template <std::size_t _Nm = _NmDefault, typename CharT = char>
struct Options {
    typedef Option<CharT> value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef value_type* iterator;
    typedef const value_type* const_iterator;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
    typedef std::char_traits<CharT> Traits;

   private:
    value_type _M_elems[_Nm];
    iterator _M_ptr;

    void logger(const char* format, const char arg) {
        if(format && arg)
            fprintf(stderr, format, arg);
    }

    void logger(const char* format, const char* arg) {
        if(format && arg)
            fprintf(stderr, format, arg);
    }

    void logger(const char* format, const wchar_t* arg) {
        if(!format || !arg)
            return;
        wchar_t* wformat = NULL;
        if((mbstowcs(wformat, format, strnlen(format, 128))) == -1)
            throw std::runtime_error("Failed converting to wchar_t.\n");
        fwprintf(stderr, wformat, arg);
    }

    void logger(const wchar_t* format, const wchar_t* arg) {
        if(format && arg)
            fwprintf(stderr, format, arg);
    }

    int64_t as_int64(const char* v) { return atoll(v); }
    int64_t as_int64(const wchar_t* v) {
        std::size_t len = std::char_traits<wchar_t>::length(v);
        wchar_t* end = (wchar_t*)(v + len);
        return wcstoll(v, &end, 10);
    }

    iterator find(const CharT __u) {
        for(iterator it = begin(); it != _M_ptr; ++it) {
            if(it->shortName == __u)
                return it;
        }
        return nullptr;
    }
    const_iterator find(const CharT __u) const noexcept { return this->find(__u); }

    iterator find(const CharT* __u) {
        if(__u == nullptr)
            return nullptr;
        std::size_t len = Traits::length(__u);
        for(iterator it = begin(); it != _M_ptr; ++it) {
            if((it->longName && Traits::compare(it->longName, __u, len) == 0))
                return it;
        }
        return nullptr;
    }
    const_iterator find(const CharT* __u) const noexcept { return this->find(__u); }

    bool is_help_flag(const CharT* p) const {
        return (p[1] == 0x68 && p[2] == 0) ||
               (p[1] == 0x2d && p[2] == 0x68 && p[3] == 0x65 && p[4] == 0x6c && p[5] == 0x70 && p[6] == 0);
    }

   public:
    const CharT* name;
    const CharT* desc;
    CharT* usage = nullptr;

    Options() : _M_elems(), _M_ptr(&_M_elems[0]){};
    Options(int) : _M_elems(), _M_ptr(&_M_elems[0]){};
    Options(const CharT* program_name, const CharT* program_description)
        : _M_elems(), _M_ptr(&_M_elems[0]), name(program_name), desc(program_description){};
    Options(std::nullptr_t) : _M_elems(), _M_ptr(&_M_elems[0]){};
    Options(std::initializer_list<value_type> init) : _M_elems(), _M_ptr(&_M_elems[0]) {
        for(auto&& it : init)
            this->append(it);
    }

    void setname(const CharT* program_name, const CharT* program_description = nullptr) {
        if(program_name)
            name = program_name;
        if(program_description)
            desc = program_description;
    };

    void operator()(std::initializer_list<value_type> init) {
        for(auto&& it : init)
            this->append(it);
    }

    template <class T>
    void append(T* data = nullptr,
                CharT shortName = 0,
                const CharT* longName = nullptr,
                const CharT* description = nullptr,
                char required = 0) {
        _M_ptr++->operator()(data, shortName, longName, description, required,
                             typeid(*data) == typeid(bool) ? TP::BOOLEAN
                             : std::is_integral<T>::value  ? TP::INTEGER
                                                           : TP::STRING);
    }
    void append(value_type _o) {
        _M_ptr++->operator()(_o.data_, _o.shortName, _o.longName, _o.description, _o.required, _o.type);
    }

    int parse(int argc, CharT** argv) { return parse(argc, (const CharT**)argv); }

    int parse(int argc, const CharT** argv) {
        for(const CharT **p = argv + 1, **end = argv + argc; *p && p < end; ++p) {
            const CharT *sep, *value;

            /* no hyphen = positional argument. */
            if(**p != 0x2d) {
                _M_ptr++->data_ = const_cast<char**>(p);
                continue;
            }

            /* find help Option */
            if(is_help_flag(*p))
                return this->print_help();

            bool is_short = (*p)[1] != 0x2d;

            iterator opt = is_short ? this->find((*p)[1]) : this->find((*p) + 2);

            /* find defined option */
            if(opt == nullptr || opt->type == TP::NONE) {
                logger("[[Parser Error]]: `%s` is not defined option.\n\n", *p);
                this->print_help();
                return -1;
            }

            if(opt->type == TP::BOOLEAN) {
                opt->data_ = (char**)!(bool)opt->data_; /* if `flag` boolean */
                if(is_short) {
                    /* boolean option continus check*/
                    for(const CharT* q = (*p) + 2; *q && opt->type == TP::BOOLEAN; ++q) {
                        if((opt = find(*q)) != nullptr)
                            opt->data_ = (char**)!(bool)opt->data_;
                    }
                }
                opt->required--;
                continue;
            }

            /* `Traits::find(*p, len, 0x3D)` is next value after of `=` separator */
            sep = Traits::find(*p, Traits::length(*p), 0x3D);
            if((value = sep ? sep : *++p) == nullptr) {
                logger("[[Parser Error]]: `%s` is unset value.\n\n", sep ? *p : *(p - 1));
                this->print_help();
                return -1;
            }

            if(opt->type == TP::INTEGER)
                *opt->data_ = (char*)as_int64(value);
            else
                *opt->data_ = const_cast<char*>(value);
            opt->required--;
        }

        for(auto opt = begin(); opt != _M_ptr; ++opt) {
            if(opt->required > 0) {
                logger("[[Parser Error]]: options argument `%s` is required\nOr `%s` is no value.\n\n", opt->longName);
                this->print_help();
                return -1;
            }
        }

        return 0;
    }

    int print_help() {
        logger("\n[Name]\n  %s\n\n", name);
        logger("[Description]\n  %s\n\n", desc);
        fprintf(stderr, "[Options]\n");

        int pos = 1;
        for(auto opt = begin(); opt != _M_ptr; ++opt) {
            fprintf(stderr, "  ");
            if(opt->shortName)
                logger("-%c", opt->shortName);
            else
                fprintf(stderr, "  ");
            fprintf(stderr, "  ");
            if(opt->longName)
                logger("--%s", opt->longName);
            else
                fprintf(stderr, "  ");
            if(opt->longName) {
                for(size_type i = 0, end = 15 - Traits::length(opt->longName); i < end; ++i)
                    fprintf(stderr, " ");
            } else {
                fprintf(stderr, "\r      positional arg %d ", pos++);
            }

            if(opt->type == TP::STRING)
                fprintf(stderr, "[string ] ");
            else if(opt->type == TP::INTEGER)
                fprintf(stderr, "[integer] ");
            else if(opt->type == TP::BOOLEAN)
                fprintf(stderr, "[ flag  ] ");
            else
                fprintf(stderr, "[  any  ] ");

            logger("  %s ", opt->required > 0 ? "(REQUIRED)" : "");
            logger("%s", opt->description);
            fprintf(stderr, "\n");
        }

        fprintf(stderr, "\n");
        return -1;
    }

    iterator begin() noexcept { return iterator(data()); }
    const_iterator begin() const noexcept { return const_iterator(data()); }
    iterator end() noexcept { return iterator(_M_ptr); }
    const_iterator end() const noexcept { return const_iterator(_M_ptr); }
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_iterator cbegin() const noexcept { return const_iterator(data()); }
    const_iterator cend() const noexcept { return const_iterator(_M_ptr); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }
    constexpr size_type size() const noexcept { return _M_ptr - begin(); }
    constexpr size_type max_size() const noexcept { return _Nm; }
    constexpr bool empty() const noexcept { return size() == 0; }

    CharT* operator[](size_type __u) noexcept {
        size_type end = size();
        if(__u < end) {
            for(size_type i = 0, u = 0; i < end; ++i) {
                if(_M_elems[i].type == TP::NONE && u++ == __u)
                    return *_M_elems[i].data_;
            }
        }
        return nullptr;
    }
    const_reference operator[](const value_type& __u) noexcept { return this->operator[](__u); }
    const_reference operator[](CharT __u) noexcept { return this->find(__u); }
    const_reference operator[](const CharT* __u) noexcept { return this->find(__u); }

    constexpr const CharT* operator[](size_type __u) const noexcept {
        size_type end = size();
        if(__u < end) {
            for(size_type i = 0, u = 0; i < end; ++i) {
                if(_M_elems[i].type == TP::NONE && u++ == __u)
                    return *_M_elems[i].data_;
            }
        }
        return nullptr;
    }
    constexpr const_reference operator[](const value_type& __u) const noexcept {
        auto r = this->find(__u->longName);
        return r ? r : this->find(__u.shortName);
    }
    constexpr const_reference operator[](CharT __u) const noexcept { return this->find(__u); }
    constexpr const_reference operator[](const CharT* __u) const noexcept { return this->find(__u); }

    pointer data() noexcept { return const_cast<pointer>(_M_elems); }
    const_pointer data() const noexcept { return const_cast<pointer>(_M_elems); }
};

/*
    ### Usage Example.
    ```
    auto ap = ArgumentParser("ExampleProgram");
    ```

    ### Options
        program_name : help display program name.

    ### Example Codes.
    ```
    # run command example
    $ ./hoge -t "I am foo."

    ```

    ```c++
    // source code example.
    #include <iostream>
    #include "argparser.hpp"

    int main(int argc, const char** argv) {
      char* test_arg = "!!default value!!";

      auto ap = ArgumentParser("hogeProgram");
      ap.append(&test_arg, 't', "test_args", "this is test parse.", 0);

      std::cout << test_arg << std::endl; // -> !!default value!!
      ap.parse(argc, argv);
      std::cout << test_arg << std::endl; // -> I am foo.
      return 0;
    }
    ```
 */
template <std::size_t N = _NmDefault, typename CharT = char>
Options<N, CharT> ArgumentParser(const CharT* program_name) {
    Options<N, CharT> ps;
    ps.name = program_name;
    return ps;
}

template <std::size_t N = _NmDefault, typename CharT = char>
Options<N, CharT> ArgumentParser(const CharT* program_name, const CharT* program_description) {
    Options<N, CharT> ps(program_name, program_description);
    return ps;
}

template <std::size_t N = _NmDefault, typename CharT = char>
Options<N, CharT> ArgumentParser(const CharT* program_name, std::initializer_list<Option<CharT>> init) {
    Options<N, CharT> ps(init);
    ps.name = program_name;
    return ps;
}

template <std::size_t N = _NmDefault, typename CharT = char>
Options<N, CharT> ArgumentParser(const CharT* program_name,
                                 const CharT* program_description,
                                 std::initializer_list<Option<CharT>> init) {
    Options<N, CharT> ps(init);
    ps.setname(program_name, program_description);
    return ps;
}

template <std::size_t N = _NmDefault, typename CharT = char>
Options<N, CharT> ArgumentParser(std::initializer_list<Option<CharT>> init) {
    Options<N, CharT> ps(init);
    return ps;
}

#endif /* _ARGPARSE_HPP_ */
