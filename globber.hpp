/* globber.hpp | MIT License | https://github.com/kirin123kirin/cutil/raw/main/LICENSE */
#ifndef GLOBBER_HPP
#define GLOBBER_HPP

#include <vector>
#include <string>
#include <stdexcept>

namespace gammy {

#if _WIN32 || _WIN64
#include <Windows.h>

using ret_t = std::vector<std::pair<std::string, struct _stat> >;
using wret_t = std::vector<std::pair<std::wstring, struct _stat> >;

/*
search_name : wildcard pathname
mode      : filter file type mode
(https://docs.microsoft.com/ja-jp/cpp/c-runtime-library/stat-structure-st-mode-field-constants?view=msvc-170#remarks)
recursive : do recursive search?
maxdepth  : when recursize search , then maxdepth level.(zero current)
_depth    : internal attribute.
 */
ret_t globber(const char* search_name,
                    int mode = _S_IFDIR | _S_IFCHR | _S_IFIFO | _S_IFREG | _S_IREAD | _S_IWRITE | _S_IEXEC,
                    bool recursive = true,
                    int maxdepth = INT16_MAX,
                    int _depth = 0) {
    const std::size_t BUFSIZE = 519;
    char fn[BUFSIZE] = {0};
    char dirname[BUFSIZE] = {0};
    struct _stat sb;
    std::size_t slen = BUFSIZE;
    std::size_t dlen = BUFSIZE;
    std::size_t fclen = BUFSIZE;
    std::size_t fnlen = BUFSIZE;
    ret_t files = {};

    HANDLE hFind;
    WIN32_FIND_DATAA fd;

    if((hFind = FindFirstFileA(search_name, &fd)) == INVALID_HANDLE_VALUE)
        throw std::runtime_error("file not found");

    if((slen = strnlen(search_name, BUFSIZE)) == BUFSIZE)
        goto done;

    for(dlen = slen - 1; dlen != 0; --dlen) {
        if(search_name[dlen] == '\\') {
            strncpy_s(dirname, BUFSIZE, search_name, ++dlen);
            break;
        } else if (search_name[dlen] == '/'){
            strncpy_s(dirname, BUFSIZE, search_name, dlen);
            dirname[dlen++] = '\\';
            break;
        }
    }
    if(dlen == 0) {
        dirname[0] = '.';
        dirname[1] = '\\';
        dlen = 2;
    }

    do {
        const char* fc = fd.cFileName;
        if(!fc || (fclen = strnlen(fc, BUFSIZE)) == BUFSIZE)
            continue;
        if(fc[0] == '.' && (fclen == 1 || (fclen == 2 && fc[1] == '.')))
            continue;
        strncpy_s(fn, BUFSIZE, dirname, dlen);
        strncpy_s(fn + dlen, BUFSIZE - dlen, fc, fclen);
        memset(fn + dlen + fclen, 0, (BUFSIZE - dlen - fclen) * sizeof(char));

        if(_stat(fn, &sb))
            continue;

        auto mask = sb.st_mode & _S_IFMT;

        if(mask & mode)
            files.emplace_back(fn, sb);

        if(recursive && fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            fnlen = strnlen(fn, BUFSIZE);
            fn[fnlen] = '\\';
            fn[fnlen + 1] = '*';
            auto child_files = globber(fn, mode, recursive, maxdepth, ++_depth);
            files.insert(files.end(), child_files.begin(), child_files.end());
        }
    } while(FindNextFileA(hFind, &fd));

done:
    FindClose(hFind);
    return files;
}

wret_t globber(const wchar_t* search_name,
                    int mode = _S_IFDIR | _S_IFCHR | _S_IFIFO | _S_IFREG | _S_IREAD | _S_IWRITE | _S_IEXEC,
                    bool recursive = true,
                    int maxdepth = INT16_MAX,
                    int _depth = 0) {
    const std::size_t BUFSIZE = 519;
    wret_t files = {};
    wchar_t fn[BUFSIZE] = {0};
    wchar_t dirname[BUFSIZE] = {0};
    struct _stat sb;
    std::size_t slen = BUFSIZE;
    std::size_t dlen = BUFSIZE;
    std::size_t fclen = BUFSIZE;
    std::size_t fnlen = BUFSIZE;

    HANDLE hFind;
    WIN32_FIND_DATAW fd;

    if((hFind = FindFirstFileW(search_name, &fd)) == INVALID_HANDLE_VALUE)
        throw std::runtime_error("file not found");

    if((slen = wcsnlen(search_name, BUFSIZE)) == BUFSIZE)
        goto done;

    for(dlen = slen - 1; dlen != 0; --dlen) {
        if(search_name[dlen] == '\\') {
            wcsncpy_s(dirname, BUFSIZE, search_name, ++dlen);
            break;
        } else if (search_name[dlen] == '/'){
            wcsncpy_s(dirname, BUFSIZE, search_name, dlen);
            dirname[dlen++] = '\\';
            break;
        }
    }

    if(dlen == 0) {
        dirname[0] = L'.';
        dirname[1] = L'\\';
        dlen = 2;
    }

    do {
        const wchar_t* fc = fd.cFileName;
        if(!fc || (fclen = wcsnlen(fc, BUFSIZE)) == BUFSIZE)
            continue;
        if(fc[0] == L'.' && (fclen == 1 || (fclen == 2 && fc[1] == L'.')))
            continue;

        wcsncpy_s(fn, BUFSIZE, dirname, dlen);
        wcsncpy_s(fn + dlen, BUFSIZE - dlen, fc, fclen);
        memset(fn + dlen + fclen, 0, (BUFSIZE - dlen - fclen) * sizeof(char));

        if(_wstat(fn, &sb))
            continue;

        auto mask = sb.st_mode & _S_IFMT;

        if(mask & mode)
            files.emplace_back(fn, sb);

        if(recursive && fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            fnlen = wcsnlen(fn, BUFSIZE);
            fn[fnlen] = L'\\';
            fn[fnlen + 1] = L'*';
            auto child_files = globber(fn, mode, recursive, maxdepth, ++_depth);
            files.insert(files.end(), child_files.begin(), child_files.end());
        }
    } while(FindNextFileW(hFind, &fd));

done:
    FindClose(hFind);
    return files;
}

ret_t globber(const std::string& search_name,
                    int mode = NULL,
                    bool recursive = true,
                    int maxdepth = INT16_MAX,
                    int _depth = 0) {
    return globber(search_name.c_str(), mode, recursive, maxdepth, _depth);
}

wret_t globber(const std::wstring& search_name,
                    int mode = NULL,
                    bool recursive = true,
                    int maxdepth = INT16_MAX,
                    int _depth = 0) {
    return globber(search_name.c_str(), mode, recursive, maxdepth, _depth);
}

#else
using ret_t = std::vector<std::pair<std::string, struct stat> >;
#include <glob.h>
#include <sys/stat.h>
#include <string.h>

/*
search_name : wildcard pathname
mode      : filter file type mode (https://linuxjm.osdn.jp/html/LDP_man-pages/man2/stat.2.html)
recursive : do recursive search?
maxdepth  : when recursize search , then maxdepth level.(zero current)
_depth    : internal attribute.
 */
ret_t globber(const char* search_name,
                    int mode = S_IFMT | S_IFDIR | S_IFCHR | S_IFBLK | S_IFREG | S_IFIFO,
                    bool recursive = true,
                    int maxdepth = INT16_MAX,
                    int _depth = 0) {
    glob_t globbuf;
    ret_t files = {};
    char fn[1024] = {0};

    glob(search_name, 0, NULL, &globbuf);

    for(std::size_t i = 0; i < globbuf.gl_pathc; ++i) {
        if(strncpy(fn, globbuf.gl_pathv[i], 1024) == NULL)
            goto done;
        int fnlen = strnlen(fn, 1024);

        struct stat sb;

        if(lstat(fn, &sb) == -1)
            continue;

        auto mask = sb.st_mode & S_IFMT;

        if(mask & mode)
            files.emplace_back(fn, sb);

        if(recursive && mask == S_IFDIR) {
            fn[fnlen] = '/';
            fn[fnlen + 1] = '*';
            auto child_files = globber(fn, mode, recursive, maxdepth, ++_depth);
            files.insert(files.end(), child_files.begin(), child_files.end());
        }
    }
done:
    globfree(&globbuf);
    return files;
}

ret_t globber(const std::string& search_name,
                    int mode = S_IFMT | S_IFDIR | S_IFCHR | S_IFBLK | S_IFREG | S_IFIFO,
                    bool recursive = true,
                    int maxdepth = INT16_MAX,
                    int _depth = 0) {
    return globber(search_name.c_str(), mode, recursive, maxdepth, _depth);
}

#endif

};  // namespace kirin

#endif /* GLOBBER_HPP */
