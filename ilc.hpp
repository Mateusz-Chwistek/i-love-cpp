// Version: 0.1.0

#ifndef I_LOVE_CPP_HPP
#define I_LOVE_CPP_HPP

#if defined(_WIN32) || defined(_WIN64)
#define OS_WINDOWS 1
#elif defined(__linux__)
#define OS_LINUX 1
#endif

#include <algorithm>
#include <cerrno>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <type_traits>
#include <utility>
#include <vector>
#include <stdexcept>
#include <cctype>

#ifdef OS_LINUX
#include <unistd.h>
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
inline void fixMinMax(T& min_val, T& max_val) {
    if (max_val < min_val) {
        std::swap(min_val, max_val);
    }
}

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
        std::size_t start_pos = 0;
        while (true) {
            std::size_t end_pos = text.find(delimiter, start_pos);
            if (end_pos == std::string::npos) {
                // add the last token (can be empty, including trailing
                // delimiter case)
                result.emplace_back(text.data() + start_pos,
                                    text_length - start_pos);
                break;
            }

            // add token even if empty (end_pos == start_pos)
            result.emplace_back(text.data() + start_pos, end_pos - start_pos);
            start_pos = end_pos + 1;
        }
    } else {
        std::size_t start_pos = 0, end_pos = 0;
        while ((end_pos = text.find(delimiter, start_pos)) !=
               std::string::npos) {
            // Skip empty token (end_pos == start_pos), e.g. for ",a", "a,",
            // "a,,b"
            if (end_pos > start_pos) {
                result.emplace_back(text.data() + start_pos,
                                    end_pos - start_pos);
            }
            start_pos = end_pos + 1;
        }

        // add the last token only if non-empty (also skips trailing delimiter
        // case)
        if (start_pos < text_length) {
            result.emplace_back(text.data() + start_pos,
                                text_length - start_pos);
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
 * @throws None (does not allocate; uses iterator-based erase).
 */
inline void trim(std::string &text) {
    // Manual lambda check, faster than std::isspace, but less reliable
    bool (*isNotSpace)(char) = [](char character) -> bool {
        return !(character == ' ' || character == '\n' || character == '\r' ||
                 character == '\t' || character == '\v' || character == '\f');
    };

    // Find the first non-whitespace character from the beginning
    std::string::iterator start_iterator =
        std::find_if(text.begin(), text.end(), isNotSpace);

    // If the string contains only whitespaces or is empty, clear it
    if (start_iterator == text.end()) {
        text.clear();
        return;
    }

    // Find the first non-whitespace character from the end
    // The base() method converts reverse iterator to normal
    std::string::iterator end_iterator =
        std::find_if(text.rbegin(), text.rend(), isNotSpace).base();

    text.erase(end_iterator, text.end());
    text.erase(text.begin(), start_iterator);
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

    // Avoid memory reallocations in string
    if (old_sub_len == new_sub_len) {
        std::size_t pos = 0;

        // Replace all occurrences of old_substr with new_substr.
        while ((pos = text.find(old_substr, pos)) != std::string::npos) {
            text.replace(pos, old_sub_len, new_substr);
            pos += new_sub_len;
        }
        return;
    }

    std::string result;
    std::size_t reserve_size = text_length, start_pos = 0, match_pos = 0;
    if (text_length >= 4096) {
        size_t occurrences = 0;
        size_t pos = 0;

        while ((pos = text.find(old_substr, pos)) != std::string::npos) {
            occurrences++;
            pos += old_sub_len;
        }

        // Lazy evaluation: if substring is not found, do nothing and exit
        // early
        if (occurrences == 0) {
            return;
        }

        if (new_sub_len > old_sub_len) {
            size_t extra_space = occurrences * (new_sub_len - old_sub_len);
            reserve_size = text_length + extra_space;
        }
        result.reserve(reserve_size);

        // Build the new string with replacements
        while ((match_pos = text.find(old_substr, start_pos)) !=
               std::string::npos) {
            result.append(text, start_pos, match_pos - start_pos);
            result.append(new_substr);
            start_pos = match_pos + old_sub_len;
        }
    } else {
        match_pos = text.find(old_substr, 0);
        if (match_pos == std::string::npos) {
            return; // Substring not found
        }
        // Allocate an extra 50% of memory only if the new substring is
        // longer
        if (new_sub_len > old_sub_len) {
            std::size_t extra_space = text_length / 2;
            reserve_size = (result.max_size() - text_length < extra_space)
                               ? result.max_size()
                               : text_length + extra_space;
        }
        result.reserve(reserve_size);

        do {
            result.append(text, start_pos, match_pos - start_pos);
            result.append(new_substr);
            start_pos = match_pos + old_sub_len;

            match_pos = text.find(old_substr, start_pos);
        } while (match_pos != std::string::npos);
    }

    // Append any remaining characters after the last match
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
inline void toLower(std::string& text) {
    if (text.empty()) {
        return;
    }

    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
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
    NotFound,         ///< The path does not exist.
    File,             ///< A regular file.
    Directory,        ///< A directory.
    Symlink,          ///< A symbolic link.
    BrokenSymlink,    ///< A symbolic link that points to a non-existent target.
    SymlinkLoop,      ///< A symbolic link loop was detected during resolution.
    CharDevice,       ///< A character special file (device).
    BlockDevice,      ///< A block special file (device).
    Pipe,             ///< A FIFO special file (named pipe).
    Socket,           ///< A local (UNIX domain) socket.
    Other,            ///< An unknown or unsupported file type.
    Error,            ///< A general system error occurred during the check.
    PermissionError   ///< Access denied (insufficient permissions).
};

#ifdef OS_WINDOWS
#elif OS_LINUX

/**
 * @brief Retrieves the file system path type.
 *
 * Uses stat/lstat to determine if the given path is a file, directory, symlink,
 * or other specialized file type. It also detects broken symlinks, symlink loops,
 * and permission errors.
 *
 * @param path The file system path to check.
 * @param follow_symlink If true, follows symlinks to check the target's type.
 * If false, returns PathType::Symlink for symbolic links.
 *
 * @return PathType representing the type of the path or the specific error encountered.
 */
inline PathType getType(const std::string &path,
                        const bool follow_symlink = false) {
    struct stat file_info {};

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
 * @param strict Determines whether to follow symlinks (passed as `follow_symlink` to `getType`).
 *
 * @return true if the path exists, false if it is not found.
 *
 * @throws std::runtime_error If a file system error occurs (e.g., permission denied, broken symlink, loop).
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
