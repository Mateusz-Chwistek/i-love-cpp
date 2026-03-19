// Version: 0.1.3

#ifndef I_LOVE_CPP_HPP
#define I_LOVE_CPP_HPP

#if defined(_WIN32) || defined(_WIN64)
#define OS_WINDOWS 1
#elif defined(__linux__)
#define OS_LINUX 1
#endif

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <type_traits>
#include <utility>
#include <vector>
#include <cstring>
#include <memory>

#ifdef OS_LINUX
#include <unistd.h>
#elif OS_WINDOWS
#include <windows.h>
#endif

namespace ilc {

// Common constraint alias
template <typename T>
using EnableIfArithmetic =
    typename std::enable_if<std::is_arithmetic<T>::value, int>::type;

// #############################################################
// #                   HELPERS                                 #
// #############################################################
namespace details {

/**
 * @brief Swaps the minimum and maximum values if they are in the wrong order.
 *
 * @tparam T Any arithmetic (numeric) type.
 * @param min_val Reference to the lower bound value.
 * @param max_val Reference to the upper bound value.
 */
template <typename T, EnableIfArithmetic<T> = 0>
inline void fixMinMax(T &min_val, T &max_val) {
    if (max_val < min_val) {
        std::swap(min_val, max_val);
    }
}

#ifdef OS_WINDOWS
/**
 * @brief Converts a UTF-8 encoded string to a wide (UTF-16) string.
 *
 * @param str The UTF-8 encoded source string.
 * @return std::wstring The converted UTF-16 wide string, or an empty
 * string if the input is empty.
 */
inline std::wstring toWideString(const std::string& str) {
    if (str.empty()) return L"";

    int requiredSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(requiredSize - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], requiredSize);
    return result;
}

/**
 * @brief Resolves the final path of an existing file or directory.
 *
 * Uses CreateFileW and GetFinalPathNameByHandleW to resolve symlinks,
 * junctions, and other reparse points to their actual target path.
 * The returned path is stripped of extended-length prefixes ("\\?\" and
 * "\\?\UNC\") to produce a conventional format compatible with _stat64.
 *
 * @param path The file system path to resolve (UTF-8 encoded).
 * @return std::string The resolved path in UTF-8, or an empty string on failure.
 */
inline std::string getResolvedTargetPath(const std::string& path) {
    const std::wstring widePath = details::toWideString(path);

    // RAII wrapper to guarantee handle cleanup on any exit path
    std::unique_ptr<void, decltype(&CloseHandle)> handle(
        CreateFileW(
            widePath.c_str(),
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            nullptr
        ),
        CloseHandle
    );

    if (handle.get() == INVALID_HANDLE_VALUE) {
        handle.release();
        return "";
    }

    const DWORD requiredSize = GetFinalPathNameByHandleW(
        handle.get(),
        nullptr,
        0,
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS
    );

    if (requiredSize == 0) {
        return "";
    }

    std::vector<WCHAR> buffer(requiredSize + 1, L'\0');

    const DWORD actualSize = GetFinalPathNameByHandleW(
        handle.get(),
        buffer.data(),
        static_cast<DWORD>(buffer.size()),
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS
    );

    if (actualSize == 0) {
        return "";
    }

    std::wstring resolvedPath(buffer.data(), actualSize);

    // Strip extended-length prefixes so the path works with _stat64.
    // "\\?\UNC\server\share\..." -> "\\server\share\..."
    // "\\?\C:\..." -> "C:\..."
    static const std::wstring uncPathPrefix = L"\\\\?\\UNC\\";
    static const std::wstring longPathPrefix = L"\\\\?\\";

    if (resolvedPath.compare(0, uncPathPrefix.size(), uncPathPrefix) == 0) {
        resolvedPath = L"\\\\" + resolvedPath.substr(uncPathPrefix.size());
    } else if (resolvedPath.compare(0, longPathPrefix.size(), longPathPrefix) == 0) {
        resolvedPath.erase(0, longPathPrefix.size());
    }

    const int utf8Size = WideCharToMultiByte(
        CP_UTF8,
        0,
        resolvedPath.c_str(),
        static_cast<int>(resolvedPath.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (utf8Size <= 0) {
        return "";
    }

    std::string result(static_cast<size_t>(utf8Size), '\0');

    const int convertedSize = WideCharToMultiByte(
        CP_UTF8,
        0,
        resolvedPath.c_str(),
        static_cast<int>(resolvedPath.size()),
        &result[0],
        utf8Size,
        nullptr,
        nullptr
    );

    if (convertedSize <= 0) {
        return "";
    }

    return result;
}
#endif

} // namespace details

// #############################################################
// #                   GENERAL UTILITIES                       #
// #############################################################

/**
 * @brief Splits a string into tokens by a single-character delimiter.
 *
 * @param text Input string. Safe for a single thread, or multiple threads that
 * do not modify @p text concurrently.
 * @param delimiter Separator character (default: space).
 * @param allow_empty Controls whether empty tokens are returned.
 * - If true: returns an element for every segment between delimiters, including
 *   empty ones.
 *   Example: ",a," -> ["", "a", ""], "a,,b" -> ["a", "", "b"], "," -> ["", ""]
 * - If false: skips empty segments, effectively treating consecutive delimiters
 *   as a single separator and ignoring leading/trailing delimiters.
 *   Example: ",a," -> ["a"], "a,,b" -> ["a", "b"], "," -> []
 *
 * @return Vector of tokens; returns empty vector if @p text is empty.
 *
 * @throws std::bad_alloc If vector/string allocation fails (including vector
 * reallocation).
 */
inline std::vector<std::string>
split(const std::string &text, char delimiter = ' ', bool allow_empty = true) {
    const std::size_t text_length = text.size();
    if (text_length < 1) {
        return {};
    }

    const char *const data = text.data();
    const char *const end = data + text_length;

    std::vector<std::string> result;
    if (text_length <= 4096) {
        // Heuristic capacity guess: (text_length / 7) ~ "average token length"
        // picked arbitrarily. +1 avoids the 0-capacity case for short strings
        // (e.g. 5/7 = 0).
        result.reserve((text_length / 7) + 1);
    } else {
        result.reserve(std::count(text.begin(), text.end(), delimiter) + 1);
    }

    if (allow_empty) {
        const char *segment = data;
        while (true) {
            const char *hit = static_cast<const char *>(
                std::memchr(segment, delimiter, end - segment));

            if (!hit) {
                // add the last token (can be empty, including trailing
                // delimiter case)
                result.emplace_back(segment, end);
                break;
            }

            // add token even if empty (hit == segment)
            result.emplace_back(segment, hit);
            segment = hit + 1;
        }
    } else {
        const char *segment = data;
        const char *hit;

        while ((hit = static_cast<const char *>(
                    std::memchr(segment, delimiter, end - segment)))) {

            // Skip empty token (hit == segment), e.g. for ",a", "a,",
            // "a,,b"
            if (hit > segment) {
                result.emplace_back(segment, hit);
            }
            segment = hit + 1;
        }

        // add the last token only if non-empty (also skips trailing delimiter
        // case)
        if (segment < end) {
            result.emplace_back(segment, end);
        }
    }

    return result;
}

/**
 * @brief Clamps a value to the inclusive range [min_val, max_val].
 *
 * If @p min_val is greater than @p max_val, the range endpoints are swapped
 * internally (so the function is order-agnostic).
 *
 * This overload participates in overload resolution only if @p T is an
 * arithmetic (numeric) type (integral or floating-point, including bool).
 *
 * @tparam T Any arithmetic (numeric) type (integral/floating-point, including
 * bool).
 * @param value Value to clamp.
 * @param min_val Lower bound (inclusive).
 * @param max_val Upper bound (inclusive).
 *
 * @return @p min_val if value is below the lower bound, @p max_val if value is
 * above the upper bound, otherwise returns @p value.
 *
 * @throws None unless comparisons or copying/moving @p T throws.
 */
template <typename T, EnableIfArithmetic<T> = 0>
inline T clamp(T value, T min_val, T max_val) {
    details::fixMinMax(min_val, max_val);

    if (value <= min_val) {
        return min_val;
    } else if (value >= max_val) {
        return max_val;
    }

    return value;
}

/**
 * @brief Checks whether a value is within the inclusive range [min_val,
 * max_val].
 *
 * If @p min_val is greater than @p max_val, the range endpoints are swapped
 * internally (so the function is order-agnostic).
 *
 *
 * @tparam T Any arithmetic (numeric) type (integral/floating-point, including
 * bool).
 * @param value Value to test.
 * @param min_val Lower bound (inclusive).
 * @param max_val Upper bound (inclusive).
 *
 * @return true if @p value is in range (inclusive), false otherwise.
 *
 * @throws None unless comparisons or copying/moving @p T throws.
 */
template <typename T, EnableIfArithmetic<T> = 0>
inline bool isInRange(T value, T min_val, T max_val) {
    details::fixMinMax(min_val, max_val);
    return value >= min_val && value <= max_val;
}

/**
 * @brief Trims ASCII whitespace from both ends of a string in-place.
 *
 * Removes leading and trailing characters from the following set:
 * space, \\n, \\r, \\t, \\v, \\f.
 *
 * @param text String to be trimmed. Safe for a single thread, or multiple
 * threads that do not modify @p text concurrently.
 *
 * @return None (modifies @p text in-place).
 *
 * @throws None (does not allocate; uses index-based erase).
 */
inline void trim(std::string& text) {
    // Manual lambda check, faster than std::isspace, but ignores locale
    bool (*isSpace)(char) = [](char character) -> bool {
        return character == ' ' || character == '\n' || character == '\r' ||
               character == '\t' || character == '\v' || character == '\f';
    };

    std::size_t start_index = 0;

    // Find the first non-whitespace character from the beginning
    while (start_index < text.size() && isSpace(text[start_index])) {
        start_index++;
    }

    // If the string contains only whitespaces or is empty, clear it
    if (start_index == text.size()) {
        text.clear();
        return;
    }

    std::size_t end_index = text.size();

    // Find the last non-whitespace character from the end
    while (end_index > start_index && isSpace(text[end_index - 1])) {
        end_index--;
    }

    // Remove trailing and leading whitespaces
    text.erase(end_index);
    text.erase(0, start_index);
}

/**
 * @brief Replaces all occurrences of a substring in a string (in-place).
 *
 * Searches @p text for non-overlapping occurrences of @p old_substr and
 * replaces them with @p new_substr, scanning left-to-right.
 *
 * Notes:
 * - If @p old_substr is empty or @p text is empty, the function does nothing.
 * - If @p old_substr is not found, @p text is left unchanged.
 * - @p new_substr may be empty (effectively deleting occurrences).
 *
 * @param text String to modify. Safe for a single thread, or multiple threads
 * that do not modify @p text concurrently.
 * @param old_substr Substring to search for. Must be non-empty to have any
 * effect.
 * @param new_substr Replacement substring.
 *
 * @return None (modifies @p text in-place).
 *
 * @throws std::bad_alloc If allocation of the temporary buffer fails (or if any
 * std::string operation allocates and fails). May also throw std::length_error
 * if the resulting string would exceed max_size().
 */
inline void replaceAll(std::string &text, const std::string &old_substr,
                          const std::string &new_substr) {
    if (old_substr.empty() || text.empty()) {
        return;
    }

    std::size_t text_length = text.size(), old_sub_len = old_substr.size(),
                new_sub_len = new_substr.size();

    // Locates substr within data using memchr for the first byte,
    // then memcmp to verify the remaining bytes.
    auto memFind = [](const char *data, std::size_t data_len,
                      const char *substr,
                      std::size_t substr_len) -> const char * {
        if (substr_len > data_len) {
            return nullptr;
        }
        const char *end = data + data_len - substr_len + 1;
        const char *pos = data;
        while (pos < end) {
            pos = static_cast<const char *>(
                std::memchr(pos, substr[0], static_cast<std::size_t>(end - pos)));
            if (!pos) {
                return nullptr;
            }
            if (std::memcmp(pos, substr, substr_len) == 0) {
                return pos;
            }
            ++pos;
        }
        return nullptr;
    };

    const char *text_data = text.data();
    const char *old_data = old_substr.data();

    // Equal-length fast path: replace in-place without allocating a new string
    if (old_sub_len == new_sub_len) {
        const char *found = text_data;

        while ((found = memFind(found, text_length - (found - text_data),
                                old_data, old_sub_len)) != nullptr) {
            std::size_t pos = static_cast<std::size_t>(found - text_data);
            text.replace(pos, old_sub_len, new_substr);
            // .replace() may reallocate, so refresh the pointer
            text_data = text.data();
            found = text_data + pos + new_sub_len;
        }
        return;
    }

    std::string result;
    std::size_t reserve_size = text_length, start_pos = 0;
    const char *found = nullptr;

    // Large text path: count occurrences first to reserve exact memory
    if (text_length >= 4096) {
        size_t occurrences = 0;
        const char *scan = text_data;

        while ((scan = memFind(scan, text_length - (scan - text_data),
                               old_data, old_sub_len)) != nullptr) {
            occurrences++;
            scan += old_sub_len;
        }

        if (occurrences == 0) {
            return;
        }

        if (new_sub_len > old_sub_len) {
            size_t extra_space = occurrences * (new_sub_len - old_sub_len);
            reserve_size = text_length + extra_space;
        }
        result.reserve(reserve_size);

        // Build the result by appending segments between matches
        while ((found = memFind(text_data + start_pos,
                                text_length - start_pos,
                                old_data, old_sub_len)) != nullptr) {
            std::size_t match_pos = static_cast<std::size_t>(found - text_data);
            result.append(text, start_pos, match_pos - start_pos);
            result.append(new_substr);
            start_pos = match_pos + old_sub_len;
        }
    } else {
        // Small text path: skip counting, reserve with 50% overhead instead
        found = memFind(text_data, text_length, old_data, old_sub_len);
        if (!found) {
            return;
        }
        if (new_sub_len > old_sub_len) {
            std::size_t extra_space = text_length / 2;
            reserve_size = (result.max_size() - text_length < extra_space)
                               ? result.max_size()
                               : text_length + extra_space;
        }
        result.reserve(reserve_size);

        do {
            std::size_t match_pos = static_cast<std::size_t>(found - text_data);
            result.append(text, start_pos, match_pos - start_pos);
            result.append(new_substr);
            start_pos = match_pos + old_sub_len;

            found = memFind(text_data + start_pos, text_length - start_pos,
                            old_data, old_sub_len);
        } while (found != nullptr);
    }

    // Append the tail after the last match
    result.append(text, start_pos, std::string::npos);

    // Transfer the new string data back to the original text variable
    text.swap(result);
}

/**
 * @brief Converts all characters in a string to lowercase in-place.
 *
 * This function uses std::tolower, which is locale-aware and supports
 * extended character sets depending on the current global locale.
 *
 * @param text String to be modified in-place. Safe for a single thread,
 * or multiple threads that do not modify `text` concurrently.
 */
inline void toLower(std::string &text) {
    if (text.empty()) {
        return;
    }

    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
}

/**
 * @brief Returns a lowercase copy of the given string.
 *
 * Creates a copy of the input string and converts each character to
 * lowercase using std::tolower.
 *
 * @param text String to copy and convert to lowercase.
 * @return Lowercase copy of the input string.
 */
inline std::string toLowerCopy(std::string text) {
    if (text.empty()) {
        return text;
    }

    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });

    return text;
}

/**
 * @brief Checks whether the given string pointer is null or points to an empty string.
 *
 * @param text Pointer to the string to check.
 * @return True if the pointer is null or the string is empty; otherwise false.
 */
inline bool isNullOrEmpty(const std::string* text) {
    return text == nullptr || text->empty();
}

/**
 * @brief Checks whether the given string is empty.
 *
 * @param text String to check.
 * @return True if the string is empty; otherwise false.
 */
inline bool isNullOrEmpty(const std::string& text) {
    return text.empty();
}

/**
 * @brief Checks whether the given string pointer is null or contains only whitespace characters.
 *
 * @param text Pointer to the string to check.
 * @return True if the pointer is null or the string contains only whitespace characters; otherwise false.
 */
inline bool isNullOrWhiteSpace(const std::string* text) {
    return text == nullptr || std::all_of(text->begin(), text->end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
}

/**
 * @brief Checks whether the given string contains only whitespace characters.
 *
 * @param text String to check.
 * @return True if the string is empty or contains only whitespace characters; otherwise false.
 */
inline bool isNullOrWhiteSpace(const std::string& text) {
    return std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
}

// #############################################################
// #               FILE UTILITIES                              #
// #############################################################
namespace files {

/**
 * @brief Represents the type or status of a file system path.
 */
enum class PathType {
    NotFound,       ///< The path does not exist.
    File,           ///< A regular file.
    Directory,      ///< A directory.
    Symlink,        ///< A symbolic link.
    BrokenSymlink,  ///< A symbolic link that points to a non-existent target.
    SymlinkLoop,    ///< A symbolic link loop was detected during resolution.
    CharDevice,     ///< A character special file (device).
    BlockDevice,    ///< A block special file (device).
    Pipe,           ///< A FIFO special file (named pipe).
    Socket,         ///< A local (UNIX domain) socket.
    Other,          ///< An unknown or unsupported file type.
    Error,          ///< A general system error occurred during the check.
    PermissionError ///< Access denied (insufficient permissions).
};

#ifdef OS_WINDOWS
/**
 * @brief Retrieves the file system path type.
 *
 * Uses _stat64 and FindFirstFileW to determine if the given path is a
 * file, directory, symlink, or other specialized file type. It also detects
 * broken symlinks and permission errors.
 *
 * @note This function doesn't detect BlockDevice, Socket and SymlinkLoop.
 *
 * @param path The file system path to check.
 * @param follow_symlink If true, follows symlinks to check the target's type.
 * If false, returns PathType::Symlink for symbolic links.
 *
 * @return PathType representing the type of the path or the specific error
 * encountered.
 */
inline PathType getType(const std::string &path,
                        const bool follow_symlink = false) {
    struct _stat64 file_info{};
    const int stat_result = _stat64(path.c_str(), &file_info);

    if (stat_result < 0) {
        const int err = errno;
        if (err == ENOENT) {
            return PathType::NotFound;
        } else if (_access(path.c_str(), 0) != 0) {
            return PathType::PermissionError;
        }

        return PathType::Error;
    }

    std::wstring path_wide = details::toWideString(path);
    WIN32_FIND_DATAW find_data{};
    const HANDLE find_handle = FindFirstFileW(path_wide.c_str(), &find_data);
    bool is_symlink = false;
    
     if (find_handle != INVALID_HANDLE_VALUE) {
        FindClose(find_handle);
        is_symlink = (find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 && find_data.dwReserved0 == IO_REPARSE_TAG_SYMLINK;
    }

    if (!follow_symlink && is_symlink) {
            return PathType::Symlink;
    } else if (is_symlink) {
        const std::string targetPath = details::getResolvedTargetPath(path);
        if (isNullOrEmpty(targetPath)) {
            return PathType::BrokenSymlink;
        }

        return getType(targetPath, true);
    }

    switch (file_info.st_mode & S_IFMT) {
        case S_IFREG:  return PathType::File;
        case S_IFDIR:  return PathType::Directory;
        case S_IFIFO:  return PathType::Pipe;
        case S_IFCHR:  return PathType::CharDevice;
        default:       return PathType::Other;
    }
}

#elif OS_LINUX

/**
 * @brief Retrieves the file system path type.
 *
 * Uses stat/lstat to determine if the given path is a file, directory, symlink,
 * or other specialized file type. It also detects broken symlinks, symlink
 * loops, and permission errors.
 *
 * @param path The file system path to check.
 * @param follow_symlink If true, follows symlinks to check the target's type.
 * If false, returns PathType::Symlink for symbolic links.
 *
 * @return PathType representing the type of the path or the specific error
 * encountered.
 */
inline PathType getType(const std::string &path,
                        const bool follow_symlink = false) {
    struct stat file_info{};

    // Use stat() to follow symlinks, lstat() to check the link itself
    const int stat_result = follow_symlink ? ::stat(path.c_str(), &file_info)
                                           : ::lstat(path.c_str(), &file_info);

    // Handle stat/lstat errors
    if (stat_result < 0) {
        const int err = errno; // Collect stat/lstat error
        if (err == ELOOP) {
            return PathType::SymlinkLoop;
        } else if (err != ENOENT && err != ENOTDIR) {
            if (err == EACCES || err == EPERM) {
                return PathType::PermissionError;
            }
            return PathType::Error;
        }

        // If follow_symlink was true and we got ENOENT, it might be a broken
        // symlink. We verify this by checking the path again without following
        // symlinks.
        if (follow_symlink) {
            const PathType type = getType(path, false);
            if (type == PathType::Symlink) {
                return PathType::BrokenSymlink;
            } else if (type == PathType::PermissionError ||
                       type == PathType::Error ||
                       type == PathType::SymlinkLoop) {
                return type;
            }
        }

        return PathType::NotFound;
    }

// Determine specific file type from the stat mode
#ifdef S_ISLNK
    if (S_ISLNK(file_info.st_mode))
        return PathType::Symlink;
#endif

    if (S_ISREG(file_info.st_mode))
        return PathType::File;
    if (S_ISDIR(file_info.st_mode))
        return PathType::Directory;
    if (S_ISCHR(file_info.st_mode))
        return PathType::CharDevice;
    if (S_ISBLK(file_info.st_mode))
        return PathType::BlockDevice;
    if (S_ISFIFO(file_info.st_mode))
        return PathType::Pipe;

#ifdef S_ISSOCK
    if (S_ISSOCK(file_info.st_mode))
        return PathType::Socket;
#endif

    return PathType::Other;
}
#endif

/**
 * @brief Checks if a given path exists on the file system.
 *
 * Returns false only when the path does not exist. For certain filesystem
 * errors, such as permission issues, broken symlinks, or symlink loops,
 * the function throws an exception instead of silently returning false.
 *
 * @param path The file system path to check.
 * @param strict Determines whether to follow symlinks (passed as
 * `follow_symlink` to `getType`).
 *
 * @return true if the path exists, false if it is not found.
 *
 * @throws std::runtime_error If a file system error occurs (e.g., permission
 * denied, broken symlink, loop).
 */
inline bool exists(const std::string &path, const bool strict = false) {
    PathType type = getType(path, strict);
    if (type == PathType::NotFound) {
        return false;
    } else if (type == PathType::Error || type == PathType::PermissionError ||
               type == PathType::BrokenSymlink ||
               type == PathType::SymlinkLoop) {
        throw std::runtime_error("File check failed");
    }

    return true;
}

} // namespace files

// #############################################################
// #                   NETWORK UTILITIES                       #
// #############################################################

} // namespace ilc

#endif // I_LOVE_CPP_HPP
