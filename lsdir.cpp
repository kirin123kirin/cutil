/* globber.hpp | MIT License | https://github.com/kirin123kirin/ccore/raw/LICENSE */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <iterator>
#include <type_traits>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <dirent.h>
#include <glob.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#endif

#include "argparser.hpp"

using Traits = std::char_traits<char>;

static void i_to_s(std::size_t n, char* buf, std::size_t keta) {
    std::size_t unit[9] = {1000000000, 100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10};
    for(std::size_t i = 0; i < keta; ++i) {
        buf[i] = (n / unit[i]) + 0x30;
        n %= unit[i];
    }
}

static char* DATETIME_FORMAT = (char*)"%Y%m%dT%H:%M:%S";
static char* DEFAULT_SEPARATOR = (char*)"\t";
static char* DEFAULT_DISPLAYORDER = (char*)"pugsamcdbf";

class LSDirectory {
    std::size_t moji;
    std::size_t n;
    std::size_t keta;
    char* pos;
    char newform[100];
    std::size_t flen;

   public:
    bool header;
    bool follow_symlink;
    char* format;
    char* sep;
    std::size_t maxdepth;
    bool disp_files;
    bool disp_dirs;
    char* display_order;

    LSDirectory()
        : moji(0),
          n(0),
          keta(0),
          pos(0),
          newform(),
          header(true),
          follow_symlink(false),
          format(DATETIME_FORMAT),
          sep(DEFAULT_SEPARATOR),
          maxdepth(-1),
          disp_files(true),
          disp_dirs(true),
          display_order(DEFAULT_DISPLAYORDER) {
        init();
    }

    void init() {
        flen = strnlen(format, 128);
        if((pos = strstr(format, "%L")) != NULL) {
            n = format - pos;  // position
            moji = 2;          // mojisuu
            keta = 3;          // ketasuu
        } else if((pos = strstr(format, "%6N")) != NULL) {
            n = format - pos;
            moji = 3;
            keta = 6;
        } else if((pos = strstr(format, "%N")) != NULL) {
            n = format - pos;
            moji = 2;
            keta = 9;
        } else if((pos = strstr(format, "%9N")) != NULL) {
            n = format - pos;
            moji = 3;
            keta = 9;
        } else if((pos = strstr(format, "%3N")) != NULL) {
            n = format - pos;
            moji = 3;
            keta = 3;
        } else {
            return;
        }
        Traits::copy(newform, format, n);
    }

#if defined(_WIN32) || defined(_WIN64)
    int recursive_ls(const char* dp, std::size_t depth = 0) {
        char fn[MAX_PATH] = {0};
        char dirname[MAX_PATH] = {0};
        std::size_t dlen = MAX_PATH;
        std::size_t fclen = MAX_PATH;
        std::size_t fnlen = MAX_PATH;
        std::size_t slen = strnlen(dp, MAX_PATH);
        if(slen == MAX_PATH) {
            fprintf(stderr, "Error: `%s` Windows Path Max Length Over.", dp);
            return -1;
        }

        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(dp, &fd);

        if(hFind == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "Error: `%s` file or Directory not found.", dp);
            throw std::runtime_error("\n");
        }

        for(dlen = slen - 1; dlen != 0; --dlen) {
            if(dp[dlen] == '\\') {
                Traits::copy(dirname, dp, ++dlen);
                break;
            } else if(dp[dlen] == '/') {
                Traits::copy(dirname, dp, dlen);
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
            if(!fc || (fclen = strnlen(fc, MAX_PATH)) == MAX_PATH)
                continue;
            if(fc[0] == '.' && (fclen == 1 || (fclen == 2 && fc[1] == '.')))
                continue;

            Traits::copy(fn, dirname, dlen);
            Traits::copy(fn + dlen, fc, fclen);
            memset(fn + dlen + fclen, 0, (MAX_PATH - dlen - fclen) * sizeof(char));

            if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                fnlen = strnlen(fn, MAX_PATH);
                if(disp_dirs)
                    FileInfo(*this, fn, dlen).print_info();
                if(depth < maxdepth) {
                    fn[fnlen] = '\\';
                    fn[fnlen + 1] = '*';
                    recursive_ls(fn, ++depth);
                }
            } else if(disp_files) {
                FileInfo(*this, fn, dlen).print_info();
            }

        } while(FindNextFileA(hFind, &fd));

        FindClose(hFind);
        return 0;
    }

#else

    int recursive_ls(const char* _dp, std::size_t depth = 0) {
        if(_dp == NULL)
            return -1;

        glob_t globbuf;
        struct stat st;

        if(glob(_dp, 0, NULL, &globbuf))
            fprintf(stderr, "Not Found File or Directory: %s\n", _dp);

        for(std::size_t i = 0, end = globbuf.gl_pathc; i < end; i++) {
            char* dp = globbuf.gl_pathv[i];
            if(stat(dp, &st) == 0 && (st.st_mode & S_IFMT) != S_IFDIR) {
                FileInfo(*this, DT_REG, dp, strnlen(dp, PATH_MAX)).print_info();
                continue;
            }

            DIR* const dir = opendir(dp);

            if(dir == NULL) {
                fprintf(stderr, "open failed: %s\n", dp);
                goto error;
            }

            try {
                char fp[PATH_MAX] = {0};

                std::size_t dlen = strnlen(dp, PATH_MAX);
                Traits::copy(fp, dp, dlen);

                if(dlen == PATH_MAX)
                    goto error;

                fp[dlen] = '/';

                for(dirent* entry = readdir(dir); entry != NULL; entry = readdir(dir)) {
                    const char* name = entry->d_name;
                    if(name[0] == 0 || (name[0] == '.' && name[1] == 0) ||
                       (name[0] == '.' && name[1] == '.' && name[2] == 0))
                        continue;

                    Traits::copy(fp + dlen + 1, name, sizeof(dirent::d_name));
                    if(entry->d_type == DT_DIR) {
                        if(disp_dirs)
                            FileInfo(*this, entry->d_type, fp, dlen).print_info();
                        if(depth < maxdepth)
                            recursive_ls(fp, ++depth);
                    } else if(disp_files) {
                        FileInfo(*this, entry->d_type, fp, dlen).print_info();
                    }
                }
                closedir(dir);
                continue;
            } catch(...) {
                goto error;
            }
        error:
            closedir(dir);
            globfree(&globbuf);
            return -1;
        }
        globfree(&globbuf);
        return 0;
    }

#endif

    void print_header() {
        char* p = display_order;
        int i = 0;
        while(*p) {
            if(i != 0)
                printf("%s", sep);
            if(*p == 'a')
                printf("%s", "atime");
            else if(*p == 'b')
                printf("%s", "basename");
            else if(*p == 'c')
                printf("%s", "ctime");
            else if(*p == 'd')
                printf("%s", "dirname");
            else if(*p == 'f')
                printf("%s", "fullpath");
            else if(*p == 'g')
                printf("%s", "gid");
            else if(*p == 'm')
                printf("%s", "mtime");
            else if(*p == 'p')
                printf("%s", "permission");
            else if(*p == 's')
                printf("%s", "size");
            else if(*p == 'u')
                printf("%s", "uid");
            ++p, ++i;
        }
        printf("\n");
    }

    struct FileInfo {
        LSDirectory& lsdir;
        unsigned char tp;
        char* fp;
        std::size_t dlen;

#if defined(_WIN32) || defined(_WIN64)

        struct _stat s;

        FileInfo(LSDirectory& _lsdir, char* _fp, std::size_t _dlen) : lsdir(_lsdir), tp(8), fp(_fp), dlen(_dlen) {
            _stat(fp, &s);
        }

        void print_permission() {
            unsigned int m = s.st_mode;
            printf("%c", _S_IFDIR & m ? 'd' : _S_IFCHR & m ? 'c' : _S_IFIFO & m ? 'p' : _S_IFREG & m ? '-' : '?');
            printf("%c", m & _S_IREAD ? 'r' : '-');
            printf("%c", m & _S_IWRITE ? 'w' : '-');
            printf("%c", m & _S_IEXEC ? 'x' : '-');
            printf("%c", m & _S_IREAD ? 'r' : '-');
            printf("%c", m & _S_IWRITE ? 'w' : '-');
            printf("%c", m & _S_IEXEC ? 'x' : '-');
            printf("%c", m & _S_IREAD ? 'r' : '-');
            printf("%c", m & _S_IWRITE ? 'w' : '-');
            printf("%c", m & _S_IEXEC ? 'x' : '-');
        }
        void print_username() { printf("%c", '?'); }
        void print_groupname() { printf("%c", '?'); }
        void print_atime() { datetimestr(s.st_atime); }
        void print_mtime() { datetimestr(s.st_mtime); }
        void print_ctime() { datetimestr(s.st_ctime); }

#else
        struct stat s;

        FileInfo(LSDirectory& _lsdir, unsigned char _tp, char* _fp, std::size_t _dlen)
            : lsdir(_lsdir), tp(_tp), fp(_fp), dlen(_dlen) {
            if(lsdir.follow_symlink && tp == DT_LNK) {
                int len = 0;
                char* lp = fp + PATH_MAX;
                char* lname = lp + dlen + 1;

                if((readlink(fp, lname, PATH_MAX)) == -1) {
                    fprintf(stderr, "read link failed: %s\n", fp);
                    return;
                }
                lname[len] = 0;
                if(lname[0] == '/')
                    lp = lname;
                if(stat(lp, &s))
                    return;
            } else {
                if(stat(fp, &s))
                    return;
            }
        }

        void print_permission() {
            unsigned int m = s.st_mode;
            printf("%c", S_ISBLK(m)    ? 'b'
                         : S_ISCHR(m)  ? 'c'
                         : S_ISDIR(m)  ? 'd'
                         : S_ISREG(m)  ? '-'
                         : S_ISFIFO(m) ? 'p'
                         : S_ISLNK(m)  ? 'l'
                         : S_ISSOCK(m) ? 's'
                                       : '?');
            printf("%c", m & S_IRUSR ? 'r' : '-');
            printf("%c", m & S_IWUSR ? 'w' : '-');
            printf("%c", m & S_ISUID ? (m & S_IXUSR ? 's' : 'S') : (m & S_IXUSR ? 'x' : '-'));
            printf("%c", m & S_IRGRP ? 'r' : '-');
            printf("%c", m & S_IWGRP ? 'w' : '-');
            printf("%c", m & S_ISGID ? (m & S_IXGRP ? 's' : 'S') : (m & S_IXGRP ? 'x' : '-'));
            printf("%c", m & S_IROTH ? 'r' : '-');
            printf("%c", m & S_IWOTH ? 'w' : '-');
            printf("%c", m & S_ISVTX ? (m & S_IXOTH ? 't' : 'T') : (m & S_IXOTH ? 'x' : '-'));
        }
        void print_username() {
            struct passwd* pwd = getpwuid(s.st_uid);
            if(pwd)
                printf("%s", pwd->pw_name);
            else
                printf("%d", s.st_uid);
        }
        void print_groupname() {
            struct group* grp = getgrgid(s.st_gid);
            if(grp)
                printf("%s", grp->gr_name);
            else
                printf("%d", s.st_gid);
        }
#if defined(__APPLE__)
        void print_atime() { datetimestr(s.st_atimespec); }
        void print_mtime() { datetimestr(s.st_mtimespec); }
        void print_ctime() { datetimestr(s.st_ctimespec); }
#else
        void print_atime() { datetimestr(s.st_atim); }
        void print_mtime() { datetimestr(s.st_mtim); }
        void print_ctime() { datetimestr(s.st_ctim); }
#endif

#endif
        void print_info() {
            char* p = lsdir.display_order;
            int i = 0;
            while(*p) {
                if(i != 0)
                    printf("%s", lsdir.sep);
                if(*p == 'a')
                    print_atime();
                else if(*p == 'b')
                    print_basename();
                else if(*p == 'c')
                    print_ctime();
                else if(*p == 'd')
                    print_dirname();
                else if(*p == 'f')
                    print_fullpath();
                else if(*p == 'g')
                    print_groupname();
                else if(*p == 'm')
                    print_mtime();
                else if(*p == 'p')
                    print_permission();
                else if(*p == 's')
                    print_size();
                else if(*p == 'u')
                    print_username();
                ++p, ++i;
            }
            printf("\n");
        }

        void print_size() { printf("%lu", s.st_size); }
        void print_dirname() {
            for(int i = 0; i < dlen - 1; ++i)
                printf("%c", fp[i]);  // dirname
        }
        void print_basename() { printf("%s", fp + dlen); }
        void print_fullpath() { printf("%s", fp); }

       private:
#if defined(_WIN32) || defined(_WIN64)
        int datetimestr(time_t t) {
            struct tm tmp;
            if(localtime_s(&tmp, &t))
                return -1;
#else
        int datetimestr(timespec t) {
            struct tm tmp;
            if(localtime_r(&t.tv_sec, &tmp))
                return -1;
#endif
            char outstr[128] = {0};
            int ret = -1;

            if(lsdir.pos != NULL) {
#if defined(_WIN32) || defined(_WIN64)
                i_to_s(t, lsdir.newform + lsdir.n, lsdir.keta);
#else
                i_to_s(t.tv_nsec, lsdir.newform + lsdir.n, lsdir.keta);
#endif
                Traits::copy(lsdir.newform + lsdir.n + lsdir.keta, lsdir.pos + lsdir.moji, lsdir.flen);
                ret = strftime(outstr, sizeof(outstr), lsdir.newform, &tmp);
            } else {
                ret = strftime(outstr, sizeof(outstr), lsdir.format, &tmp);
            }
            if(ret == 0)
                return -1;  //@stderr?

            printf("%s", outstr);
            return 0;
        }
    };
};

int main(int argc, const char** argv) {
    auto args = ArgumentParser("lsdir",
                               "recursive output files detail infomations.\n"
                               "like `ls -lR xx` added fullpath\n");
    LSDirectory LD;
    LD.header = true;
    LD.follow_symlink = false;
    LD.maxdepth = -1;
    LD.sep = const_cast<char*>("\t");
    LD.format = DATETIME_FORMAT;
    LD.display_order = DEFAULT_DISPLAYORDER;

    char t = 'B';

    args.append(&LD.header, 'n', "noheader", "output no header\n", 0);
    args.append(&LD.follow_symlink, 'l', "follow-symlink",
                "if symbolic link file then do output readlink to file infomation.\n", 0);

    args.append(&LD.maxdepth, 'x', "maxdepth", "output recursive directory max depth level. (default nolimit)\n", 0);
    args.append(&LD.sep, 's', "separator", "output field separator\n", 0);
    args.append(&LD.format, 'f', "format-time",
                "output timeformat string. (default %Y-%m-%dT%H:%M:%S)\n"
                "see format definition.\n"
                "https://www.cplusplus.com/reference/ctime/strftime/\n",
                0);

    args.append(&t, 't', "type",
                "filtered target type category.\n"
                "   d : directories only\n"
                "   f : files only\n"
                "   B : Both directories and files(default)\n",
                0);

    args.append(&LD.display_order, 'd', "display",
                "select display target categories. (default : display output all category)\n"
                "   p : permission       (ex. -rw-------)\n"
                "   u : owner user name  (ex. root)\n"
                "   g : owner group name (ex. root)\n"
                "   s : file size        (ex. 123456)\n"
                "   a : atime            (ex. 2022/02/05 10:00:00)\n"
                "   m : mtime            (ex. 2022/02/05 10:00:00)\n"
                "   c : ctime            (ex. 2022/02/05 10:00:00)\n"
                "   d : dirname          (ex. /root/.ssh)\n"
                "   b : dirname          (ex. known_hosts)\n"
                "   f : fullpath         (ex. /root/.ssh/known_hosts)\n"
                "  Example: --display-category pugsaf\n"
                "  Output-> -rw-------  root root 123456 2022/02/05 10:00:00 /root/.ssh/known_hosts\n",
                0);

    args.parse(argc, argv);
    unescape(LD.sep);

    try {
        if(t == 'd')
            LD.disp_files = false;
        else if(t == 'f')
            LD.disp_dirs = false;
        else if(t != 'B')
            return 1;

        if(LD.header)
            LD.print_header();

        int count = 0;
        for(std::size_t i = 0, end = args.size(); i < end; ++i) {
            auto a = args[i];
            if(a && ++count) {
                if(a[0] == '.' && a[1] == 0)
                    LD.recursive_ls("./*");
                else if(a[0] == '.' && a[1] == '.' && a[2] == 0)
                    LD.recursive_ls("../*");
                else
                    LD.recursive_ls(a);
            }
        }
        if(count == 0)
            LD.recursive_ls("./*");

    } catch(std::exception& e) {
        fprintf(stderr, "%s\n\n", e.what());
        return 1;
    }

    return 0;
}
