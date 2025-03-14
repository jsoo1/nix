#pragma once

#include "types.hh"
#include "error.hh"
#include "logging.hh"
#include "ansicolor.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>

#include <functional>
#include <map>
#include <sstream>
#include <optional>

#ifndef HAVE_STRUCT_DIRENT_D_TYPE
#define DT_UNKNOWN 0
#define DT_REG 1
#define DT_LNK 2
#define DT_DIR 3
#endif

namespace nix {

struct Sink;
struct Source;


/* The system for which Nix is compiled. */
extern const std::string nativeSystem;


/* Return an environment variable. */
std::optional<std::string> getEnv(const std::string & key);

/* Get the entire environment. */
std::map<std::string, std::string> getEnv();

/* Clear the environment. */
void clearEnv();

/* Return an absolutized path, resolving paths relative to the
   specified directory, or the current directory otherwise.  The path
   is also canonicalised. */
Path absPath(Path path,
    std::optional<Path> dir = {},
    bool resolveSymlinks = false);

/* Canonicalise a path by removing all `.' or `..' components and
   double or trailing slashes.  Optionally resolves all symlink
   components such that each component of the resulting path is *not*
   a symbolic link. */
Path canonPath(const Path & path, bool resolveSymlinks = false);

/* Return the directory part of the given canonical path, i.e.,
   everything before the final `/'.  If the path is the root or an
   immediate child thereof (e.g., `/foo'), this means `/'
   is returned.*/
Path dirOf(const Path & path);

/* Return the base name of the given canonical path, i.e., everything
   following the final `/' (trailing slashes are removed). */
std::string_view baseNameOf(std::string_view path);

/* Check whether 'path' is a descendant of 'dir'. */
bool isInDir(const Path & path, const Path & dir);

/* Check whether 'path' is equal to 'dir' or a descendant of 'dir'. */
bool isDirOrInDir(const Path & path, const Path & dir);

/* Get status of `path'. */
struct stat lstat(const Path & path);

/* Return true iff the given path exists. */
bool pathExists(const Path & path);

/* Read the contents (target) of a symbolic link.  The result is not
   in any way canonicalised. */
Path readLink(const Path & path);

bool isLink(const Path & path);

/* Read the contents of a directory.  The entries `.' and `..' are
   removed. */
struct DirEntry
{
    string name;
    ino_t ino;
    unsigned char type; // one of DT_*
    DirEntry(const string & name, ino_t ino, unsigned char type)
        : name(name), ino(ino), type(type) { }
};

typedef vector<DirEntry> DirEntries;

DirEntries readDirectory(const Path & path);

unsigned char getFileType(const Path & path);

/* Read the contents of a file into a string. */
string readFile(int fd);
string readFile(const Path & path);
void readFile(const Path & path, Sink & sink);

/* Write a string to a file. */
void writeFile(const Path & path, std::string_view s, mode_t mode = 0666);

void writeFile(const Path & path, Source & source, mode_t mode = 0666);

/* Read a line from a file descriptor. */
string readLine(int fd);

/* Write a line to a file descriptor. */
void writeLine(int fd, string s);

/* Delete a path; i.e., in the case of a directory, it is deleted
   recursively. It's not an error if the path does not exist. The
   second variant returns the number of bytes and blocks freed. */
void deletePath(const Path & path);

void deletePath(const Path & path, uint64_t & bytesFreed);

std::string getUserName();

/* Return $HOME or the user's home directory from /etc/passwd. */
Path getHome();

/* Return $XDG_CACHE_HOME or $HOME/.cache. */
Path getCacheDir();

/* Return $XDG_CONFIG_HOME or $HOME/.config. */
Path getConfigDir();

/* Return the directories to search for user configuration files */
std::vector<Path> getConfigDirs();

/* Return $XDG_DATA_HOME or $HOME/.local/share. */
Path getDataDir();

/* Create a directory and all its parents, if necessary.  Returns the
   list of created directories, in order of creation. */
Paths createDirs(const Path & path);

/* Create a symlink. */
void createSymlink(const Path & target, const Path & link,
    std::optional<time_t> mtime = {});

/* Atomically create or replace a symlink. */
void replaceSymlink(const Path & target, const Path & link,
    std::optional<time_t> mtime = {});


/* Wrappers arount read()/write() that read/write exactly the
   requested number of bytes. */
void readFull(int fd, char * buf, size_t count);
void writeFull(int fd, std::string_view s, bool allowInterrupts = true);

MakeError(EndOfFile, Error);


/* Read a file descriptor until EOF occurs. */
string drainFD(int fd, bool block = true, const size_t reserveSize=0);

void drainFD(int fd, Sink & sink, bool block = true);


/* Automatic cleanup of resources. */


class AutoDelete
{
    Path path;
    bool del;
    bool recursive;
public:
    AutoDelete();
    AutoDelete(const Path & p, bool recursive = true);
    ~AutoDelete();
    void cancel();
    void reset(const Path & p, bool recursive = true);
    operator Path() const { return path; }
};


class AutoCloseFD
{
    int fd;
public:
    AutoCloseFD();
    AutoCloseFD(int fd);
    AutoCloseFD(const AutoCloseFD & fd) = delete;
    AutoCloseFD(AutoCloseFD&& fd);
    ~AutoCloseFD();
    AutoCloseFD& operator =(const AutoCloseFD & fd) = delete;
    AutoCloseFD& operator =(AutoCloseFD&& fd);
    int get() const;
    explicit operator bool() const;
    int release();
    void close();
};


/* Create a temporary directory. */
Path createTempDir(const Path & tmpRoot = "", const Path & prefix = "nix",
    bool includePid = true, bool useGlobalCounter = true, mode_t mode = 0755);

/* Create a temporary file, returning a file handle and its path. */
std::pair<AutoCloseFD, Path> createTempFile(const Path & prefix = "nix");


class Pipe
{
public:
    AutoCloseFD readSide, writeSide;
    void create();
    void close();
};


struct DIRDeleter
{
    void operator()(DIR * dir) const {
        closedir(dir);
    }
};

typedef std::unique_ptr<DIR, DIRDeleter> AutoCloseDir;


class Pid
{
    pid_t pid = -1;
    bool separatePG = false;
    int killSignal = SIGKILL;
public:
    Pid();
    Pid(pid_t pid);
    ~Pid();
    void operator =(pid_t pid);
    operator pid_t();
    int kill();
    int wait();

    void setSeparatePG(bool separatePG);
    void setKillSignal(int signal);
    pid_t release();
};


/* Kill all processes running under the specified uid by sending them
   a SIGKILL. */
void killUser(uid_t uid);


/* Fork a process that runs the given function, and return the child
   pid to the caller. */
struct ProcessOptions
{
    string errorPrefix = "";
    bool dieWithParent = true;
    bool runExitHandlers = false;
    bool allowVfork = false;
};

pid_t startProcess(std::function<void()> fun, const ProcessOptions & options = ProcessOptions());


/* Run a program and return its stdout in a string (i.e., like the
   shell backtick operator). */
string runProgram(Path program, bool searchPath = false,
    const Strings & args = Strings(),
    const std::optional<std::string> & input = {});

struct RunOptions
{
    Path program;
    bool searchPath = true;
    Strings args;
    std::optional<uid_t> uid;
    std::optional<uid_t> gid;
    std::optional<Path> chdir;
    std::optional<std::map<std::string, std::string>> environment;
    std::optional<std::string> input;
    Source * standardIn = nullptr;
    Sink * standardOut = nullptr;
    bool mergeStderrToStdout = false;
};

std::pair<int, std::string> runProgram(RunOptions && options);

void runProgram2(const RunOptions & options);


/* Change the stack size. */
void setStackSize(size_t stackSize);


/* Restore the original inherited Unix process context (such as signal
   masks, stack size, CPU affinity). */
void restoreProcessContext(bool restoreMounts = true);

/* Save the current mount namespace. Ignored if called more than
   once. */
void saveMountNamespace();

/* Restore the mount namespace saved by saveMountNamespace(). Ignored
   if saveMountNamespace() was never called. */
void restoreMountNamespace();


class ExecError : public Error
{
public:
    int status;

    template<typename... Args>
    ExecError(int status, const Args & ... args)
        : Error(args...), status(status)
    { }
};

/* Convert a list of strings to a null-terminated vector of char
   *'s. The result must not be accessed beyond the lifetime of the
   list of strings. */
std::vector<char *> stringsToCharPtrs(const Strings & ss);

/* Close all file descriptors except those listed in the given set.
   Good practice in child processes. */
void closeMostFDs(const set<int> & exceptions);

/* Set the close-on-exec flag for the given file descriptor. */
void closeOnExec(int fd);


/* User interruption. */

extern std::atomic<bool> _isInterrupted;

extern thread_local std::function<bool()> interruptCheck;

void setInterruptThrown();

void _interrupted();

void inline checkInterrupt()
{
    if (_isInterrupted || (interruptCheck && interruptCheck()))
        _interrupted();
}

MakeError(Interrupted, BaseError);


MakeError(FormatError, Error);


/* String tokenizer. */
template<class C> C tokenizeString(std::string_view s, const string & separators = " \t\n\r");


/* Concatenate the given strings with a separator between the
   elements. */
template<class C>
string concatStringsSep(const string & sep, const C & ss)
{
    string s;
    for (auto & i : ss) {
        if (s.size() != 0) s += sep;
        s += i;
    }
    return s;
}


/* Add quotes around a collection of strings. */
template<class C> Strings quoteStrings(const C & c)
{
    Strings res;
    for (auto & s : c)
        res.push_back("'" + s + "'");
    return res;
}


/* Remove trailing whitespace from a string. FIXME: return
   std::string_view. */
string chomp(std::string_view s);


/* Remove whitespace from the start and end of a string. */
string trim(const string & s, const string & whitespace = " \n\r\t");


/* Replace all occurrences of a string inside another string. */
string replaceStrings(std::string_view s,
    const std::string & from, const std::string & to);


std::string rewriteStrings(const std::string & s, const StringMap & rewrites);


/* Convert the exit status of a child as returned by wait() into an
   error string. */
string statusToString(int status);

bool statusOk(int status);


/* Parse a string into an integer. */
template<class N>
std::optional<N> string2Int(const std::string & s)
{
    if (s.substr(0, 1) == "-" && !std::numeric_limits<N>::is_signed)
        return std::nullopt;
    std::istringstream str(s);
    N n;
    str >> n;
    if (str && str.get() == EOF) return n;
    return std::nullopt;
}

/* Like string2Int(), but support an optional suffix 'K', 'M', 'G' or
   'T' denoting a binary unit prefix. */
template<class N>
N string2IntWithUnitPrefix(std::string s)
{
    N multiplier = 1;
    if (!s.empty()) {
        char u = std::toupper(*s.rbegin());
        if (std::isalpha(u)) {
            if (u == 'K') multiplier = 1ULL << 10;
            else if (u == 'M') multiplier = 1ULL << 20;
            else if (u == 'G') multiplier = 1ULL << 30;
            else if (u == 'T') multiplier = 1ULL << 40;
            else throw UsageError("invalid unit specifier '%1%'", u);
            s.resize(s.size() - 1);
        }
    }
    if (auto n = string2Int<N>(s))
        return *n * multiplier;
    throw UsageError("'%s' is not an integer", s);
}

/* Parse a string into a float. */
template<class N>
std::optional<N> string2Float(const string & s)
{
    std::istringstream str(s);
    N n;
    str >> n;
    if (str && str.get() == EOF) return n;
    return std::nullopt;
}


/* Return true iff `s' starts with `prefix'. */
bool hasPrefix(std::string_view s, std::string_view prefix);


/* Return true iff `s' ends in `suffix'. */
bool hasSuffix(std::string_view s, std::string_view suffix);


/* Convert a string to lower case. */
std::string toLower(const std::string & s);


/* Escape a string as a shell word. */
std::string shellEscape(const std::string & s);


/* Exception handling in destructors: print an error message, then
   ignore the exception. */
void ignoreException();



/* Tree formatting. */
constexpr char treeConn[] = "├───";
constexpr char treeLast[] = "└───";
constexpr char treeLine[] = "│   ";
constexpr char treeNull[] = "    ";

/* Determine whether ANSI escape sequences are appropriate for the
   present output. */
bool shouldANSI();

/* Truncate a string to 'width' printable characters. If 'filterAll'
   is true, all ANSI escape sequences are filtered out. Otherwise,
   some escape sequences (such as colour setting) are copied but not
   included in the character count. Also, tabs are expanded to
   spaces. */
std::string filterANSIEscapes(const std::string & s,
    bool filterAll = false,
    unsigned int width = std::numeric_limits<unsigned int>::max());


/* Base64 encoding/decoding. */
string base64Encode(std::string_view s);
string base64Decode(std::string_view s);


/* Remove common leading whitespace from the lines in the string
   's'. For example, if every line is indented by at least 3 spaces,
   then we remove 3 spaces from the start of every line. */
std::string stripIndentation(std::string_view s);


/* Get a value for the specified key from an associate container. */
template <class T>
std::optional<typename T::mapped_type> get(const T & map, const typename T::key_type & key)
{
    auto i = map.find(key);
    if (i == map.end()) return {};
    return std::optional<typename T::mapped_type>(i->second);
}


/* Remove and return the first item from a container. */
template <class T>
std::optional<typename T::value_type> remove_begin(T & c)
{
    auto i = c.begin();
    if (i == c.end()) return {};
    auto v = std::move(*i);
    c.erase(i);
    return v;
}


/* Remove and return the first item from a container. */
template <class T>
std::optional<typename T::value_type> pop(T & c)
{
    if (c.empty()) return {};
    auto v = std::move(c.front());
    c.pop();
    return v;
}


template<typename T>
class Callback;


/* Start a thread that handles various signals. Also block those signals
   on the current thread (and thus any threads created by it). */
void startSignalHandlerThread();

struct InterruptCallback
{
    virtual ~InterruptCallback() { };
};

/* Register a function that gets called on SIGINT (in a non-signal
   context). */
std::unique_ptr<InterruptCallback> createInterruptCallback(
    std::function<void()> callback);

void triggerInterrupt();

/* A RAII class that causes the current thread to receive SIGUSR1 when
   the signal handler thread receives SIGINT. That is, this allows
   SIGINT to be multiplexed to multiple threads. */
struct ReceiveInterrupts
{
    pthread_t target;
    std::unique_ptr<InterruptCallback> callback;

    ReceiveInterrupts()
        : target(pthread_self())
        , callback(createInterruptCallback([&]() { pthread_kill(target, SIGUSR1); }))
    { }
};



/* A RAII helper that increments a counter on construction and
   decrements it on destruction. */
template<typename T>
struct MaintainCount
{
    T & counter;
    long delta;
    MaintainCount(T & counter, long delta = 1) : counter(counter), delta(delta) { counter += delta; }
    ~MaintainCount() { counter -= delta; }
};


/* Return the number of rows and columns of the terminal. */
std::pair<unsigned short, unsigned short> getWindowSize();


/* Used in various places. */
typedef std::function<bool(const Path & path)> PathFilter;

extern PathFilter defaultPathFilter;

/* Common initialisation performed in child processes. */
void commonChildInit(Pipe & logPipe);

/* Create a Unix domain socket. */
AutoCloseFD createUnixDomainSocket();

/* Create a Unix domain socket in listen mode. */
AutoCloseFD createUnixDomainSocket(const Path & path, mode_t mode);

/* Bind a Unix domain socket to a path. */
void bind(int fd, const std::string & path);

/* Connect to a Unix domain socket. */
void connect(int fd, const std::string & path);


// A Rust/Python-like enumerate() iterator adapter.
// Borrowed from http://reedbeta.com/blog/python-like-enumerate-in-cpp17.
template <typename T,
          typename TIter = decltype(std::begin(std::declval<T>())),
          typename = decltype(std::end(std::declval<T>()))>
constexpr auto enumerate(T && iterable)
{
    struct iterator
    {
        size_t i;
        TIter iter;
        bool operator != (const iterator & other) const { return iter != other.iter; }
        void operator ++ () { ++i; ++iter; }
        auto operator * () const { return std::tie(i, *iter); }
    };

    struct iterable_wrapper
    {
        T iterable;
        auto begin() { return iterator{ 0, std::begin(iterable) }; }
        auto end() { return iterator{ 0, std::end(iterable) }; }
    };

    return iterable_wrapper{ std::forward<T>(iterable) };
}


// C++17 std::visit boilerplate
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;


std::string showBytes(uint64_t bytes);


}
