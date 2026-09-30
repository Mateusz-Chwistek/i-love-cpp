// i-love-cpp (ilc) - https://github.com/Mateusz-Chwistek/i-love-cpp
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mateusz-Chwistek
//
// Version: 1.0.0
// Full docs: https://github.com/Mateusz-Chwistek/i-love-cpp/blob/v1.0.0/DOCS.md

#if !defined(I_LOVE_CPP_HPP)
#define I_LOVE_CPP_HPP

#define ILC_VERSION_MAJOR 1
#define ILC_VERSION_MINOR 0
#define ILC_VERSION_PATCH 0

// Single number for easy comparison, e.g. #if ILC_VERSION >= 10200
#define ILC_VERSION                                                            \
    (ILC_VERSION_MAJOR * 10000 + ILC_VERSION_MINOR * 100 + ILC_VERSION_PATCH)

#if defined(_WIN32) || defined(_WIN64)
#define ILC_OS_WINDOWS 1
#elif defined(__linux__)
#define ILC_OS_LINUX 1
#else
#error "Unsupported platform error. i-love-cpp supports Linux and Windows only"
#endif

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(ILC_OS_WINDOWS)
// Stop windows.h from defining the min/max macros.
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#define ILC_UNDEF_WIN32_LEAN_AND_MEAN
#endif

#if !defined(NOMINMAX)
#define NOMINMAX
#define ILC_UNDEF_NOMINMAX
#endif

#include <windows.h>

#if defined(ILC_UNDEF_WIN32_LEAN_AND_MEAN)
#undef WIN32_LEAN_AND_MEAN
#undef ILC_UNDEF_WIN32_LEAN_AND_MEAN
#endif

#if defined(ILC_UNDEF_NOMINMAX)
#undef NOMINMAX
#undef ILC_UNDEF_NOMINMAX
#endif

#include <io.h>
#include <memory>
#endif

namespace ilc {

// #############################################################
// #                   HELPERS                                 #
// #############################################################
namespace details {

/**
 * @brief Restricts a template to all arithmetic types
 *        (including bool and char).
 *
 * @tparam T Type to check.
 */
template <typename T>
using EnableIfArithmetic =
    typename std::enable_if<std::is_arithmetic<T>::value, int>::type;

/**
 * @brief Checks whether every type in a pack converts to
 *        std::string.
 *
 * @tparam Args Types to check.
 */
template <typename... Args> struct AllConvertibleToString;

/**
 * @brief Empty pack - always true, ends the recursion.
 */
template <> struct AllConvertibleToString<> : std::true_type {};

/**
 * @brief Checks the first type and recurses on the rest.
 *
 * @tparam First Type checked in this step.
 * @tparam Rest Remaining types.
 */
template <typename First, typename... Rest>
struct AllConvertibleToString<First, Rest...>
    : std::integral_constant<
          bool, std::is_convertible<typename std::decay<First>::type,
                                    std::string>::value &&
                    AllConvertibleToString<Rest...>::value> {};

/**
 * @brief Restricts a template to arguments that convert to std::string.
 *
 * @tparam Args Types to check.
 */
template <typename... Args>
using EnableIfConvertibleToString =
    typename std::enable_if<AllConvertibleToString<Args...>::value, int>::type;

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

/**
 * @brief Checks whether a character is ASCII whitespace (space, \\n, \\r, \\t,
 * \\v, \\f).
 *
 * @param character Character to check.
 * @return True if the character is ASCII whitespace; otherwise false.
 */
inline bool isAsciiSpace(char character) {
    return character == ' ' || character == '\n' || character == '\r' ||
           character == '\t' || character == '\v' || character == '\f';
}

/**
 * @brief Appends a single text element to result, adding the separator only
 *        where one is actually needed.
 *
 * @param separator Delimiter inserted before the element when result is not
 * empty.
 * @param result Accumulator string to append to.
 * @param text Text element to append.
 */
inline void joinImplAppend(const std::string &separator, std::string &result,
                           const std::string &text) {
    if (!text.empty()) {
        if (!result.empty()) {
            result += separator;
        }
        result += text;
    }
}

/**
 * @brief Base case for the join recursion - no elements left to append.
 *
 * @param separator Delimiter the other overloads insert; unused here.
 * @param result Accumulator string; left unchanged.
 */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
inline void joinImpl(const std::string &separator, const std::string &result) {
    /* Both parameters are needed only to match the recursive call's shape.
        Casting to void marks them as deliberately unused, which silences
        -Wunused-parameter */
    (void)separator;
    (void)result;
}

/**
 * @brief Recursively joins text elements into result, separated by separator.
 *        Empty strings are skipped.
 *
 * @tparam Args Types convertible to std::string.
 * @param separator Delimiter inserted between non-empty elements.
 * @param result Accumulator string to append to.
 * @param text Current text element to process.
 * @param rest Remaining text elements.
 */
template <typename... Args, EnableIfConvertibleToString<Args...> = 0>
inline void joinImpl(const std::string &separator, std::string &result,
                     const std::string &text, const Args &...rest) {
    joinImplAppend(separator, result, text);
    joinImpl(separator, result, rest...);
}

/**
 * @brief Finds the first occurrence of a byte sequence inside another one.
 *
 * @param haystack Buffer to search in.
 * @param haystack_len Number of bytes in haystack.
 * @param needle Byte sequence to look for; must be non-empty.
 * @param needle_len Number of bytes in needle.
 * @return Pointer to the first match inside haystack, or nullptr when there is
 * none.
 */
inline const char *memFind(const char *haystack, std::size_t haystack_len,
                           const char *needle, std::size_t needle_len) {
    if (needle_len > haystack_len) {
        return nullptr;
    }

    // Past this point a match can no longer fit in what is left of haystack
    const char *const end = haystack + haystack_len - needle_len + 1;
    const char *pos = haystack;

    while (pos < end) {
        pos = static_cast<const char *>(
            std::memchr(pos, needle[0], static_cast<std::size_t>(end - pos)));

        if (pos == nullptr) {
            return nullptr;
        }
        if (std::memcmp(pos, needle, needle_len) == 0) {
            return pos;
        }
        ++pos;
    }

    return nullptr;
}

/**
 * @brief Counts non-overlapping occurrences of a pattern in a text.
 *
 * @param text Text to scan.
 * @param pattern Pattern to count; must be non-empty.
 * @param first_match_out Optional output for the first occurrence found,
 * set to nullptr when there is none.
 * @return Number of non-overlapping occurrences.
 */
inline std::size_t countOccurrences(const std::string &text,
                                    const std::string &pattern,
                                    const char **first_match_out = nullptr) {
    const char *const end = text.data() + text.size();
    const char *scan =
        memFind(text.data(), text.size(), pattern.data(), pattern.size());

    if (first_match_out != nullptr) {
        *first_match_out = scan;
    }

    std::size_t occurrences = 0;

    /* The next search starts past the current match, so the first one is
        already in hand and the loop only has to advance */
    while (scan != nullptr) {
        occurrences++;
        scan += pattern.size();
        scan = memFind(scan, static_cast<std::size_t>(end - scan),
                       pattern.data(), pattern.size());
    }

    return occurrences;
}

/**
 * @brief Computes base_length + occurrences * delta, capped at the longest
 *        string the implementation can hold.
 *
 * @param base_length Length of the text before the replacement.
 * @param occurrences Number of replacements to account for; must not be zero.
 * @param delta Extra bytes each replacement adds.
 * @return The requested capacity, or the maximum string size when it would not
 * fit.
 */
inline std::size_t cappedReserveSize(std::size_t base_length,
                                     std::size_t occurrences,
                                     std::size_t delta) {
    const std::size_t max_size = std::string().max_size();

    if (delta > (max_size - base_length) / occurrences) {
        return max_size;
    }

    return base_length + occurrences * delta;
}

/**
 * @brief Replaces every occurrence of a substring with one of the same length,
 *        without reallocating.
 *
 * @param text Text to modify in place; must be non-empty.
 * @param old_substr Substring to replace; must be non-empty.
 * @param new_substr Replacement, exactly as long as old_substr.
 */
inline void replaceSameLength(std::string &text, const std::string &old_substr,
                              const std::string &new_substr) {
    /* &text[0] instead of text.data(), which only returns a writable pointer
        from C++17 on */
    char *const data = &text[0]; // NOLINT(readability-container-data-pointer)
    const std::size_t length = text.size();
    const std::size_t sub_len = old_substr.size();

    std::size_t start_pos = 0;
    const char *found = nullptr;

    while ((found = memFind(data + start_pos, length - start_pos,
                            old_substr.data(), sub_len)) != nullptr) {
        const std::size_t match_pos = static_cast<std::size_t>(found - data);
        /* used char_traits::move instead of memmove as it copies the same way
           but leaves the string's terminator alone */
        std::char_traits<char>::move(data + match_pos, new_substr.data(),
                                     sub_len);
        start_pos = match_pos + sub_len;
    }
}

/**
 * @brief Builds a copy of a text with every occurrence of a substring replaced.
 *
 * @param text Source text; left untouched.
 * @param old_substr Substring to replace; must be non-empty.
 * @param new_substr Replacement substring.
 * @param reserve_size Capacity reserved for the result before building it.
 * @param first_match Pointer to the first occurrence of old_substr inside
 * text; must not be null.
 * @return A new string with all occurrences replaced.
 * @throw std::bad_alloc If allocation of the result fails.
 */
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
inline std::string buildReplaced(const std::string &text,
                                 const std::string &old_substr,
                                 const std::string &new_substr,
                                 std::size_t reserve_size,
                                 const char *first_match) {
    // NOLINTEND(bugprone-easily-swappable-parameters)
    const char *const data = text.data();
    const std::size_t length = text.size();
    const std::size_t old_sub_len = old_substr.size();

    std::string result;
    result.reserve(reserve_size);

    std::size_t start_pos = 0;
    const char *found = first_match;

    while (found != nullptr) {
        const std::size_t match_pos = static_cast<std::size_t>(found - data);
        result.append(text, start_pos, match_pos - start_pos);
        result.append(new_substr);
        start_pos = match_pos + old_sub_len;

        found = memFind(data + start_pos, length - start_pos, old_substr.data(),
                        old_sub_len);
    }

    // Append the tail after the last match
    result.append(text, start_pos, std::string::npos);

    return result;
}

#if defined(ILC_OS_WINDOWS)
/**
 * @brief Converts a UTF-8 encoded string to a wide (UTF-16) string.
 *
 * @param str The UTF-8 encoded source string.
 * @return The converted wide string, or empty if the input is empty or the
 * conversion failed (e.g. @p str is not valid UTF-8).
 */
inline std::wstring toWideString(const std::string &str) {
    if (str.empty()) {
        return {};
    }

    int required_size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                            str.c_str(), -1, nullptr, 0);

    if (required_size < 1) {
        return {};
    }

    std::wstring result(static_cast<std::size_t>(required_size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), -1,
                        &result[0], required_size);
    return result;
}

/**
 * @brief Converts a wide (UTF-16) string to a UTF-8 encoded string.
 *
 * @param wstr The UTF-16 wide source string.
 * @return The converted UTF-8 string, or empty if the input is empty or the
 * conversion failed (e.g. @p wstr holds an unpaired surrogate).
 */
inline std::string toNormalString(const std::wstring &wstr) {
    if (wstr.empty()) {
        return {};
    }

    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstr.data(),
                                   static_cast<int>(wstr.size()), nullptr, 0,
                                   nullptr, nullptr);

    if (size < 1) {
        return {};
    }

    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstr.data(),
                        static_cast<int>(wstr.size()), &result[0], size,
                        nullptr, nullptr);

    return result;
}

/**
 * @brief Resolves a symlink, junction or other reparse point to its target
 *        path, stripped of extended-length prefixes.
 *
 * @param path The file system path to resolve (UTF-8 encoded).
 * @param error_out Optional output for the Win32 error on failure; set to
 * ERROR_SUCCESS when the path resolves.
 * @return The resolved path in UTF-8, or empty on failure.
 */
inline std::string getResolvedTargetPath(const std::string &path,
                                         DWORD *error_out = nullptr) {
    DWORD discarded_error = ERROR_SUCCESS;
    if (error_out == nullptr) {
        error_out = &discarded_error;
    }
    *error_out = ERROR_SUCCESS;

    const std::wstring wide_path = details::toWideString(path);

    const HANDLE raw_handle = CreateFileW(
        wide_path.c_str(), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);

    if (raw_handle == INVALID_HANDLE_VALUE) {
        *error_out = GetLastError();
        return {};
    }

    /* RAII wrapper to guarantee handle cleanup on any exit path. Invalid
        handle never reaches it */
    std::unique_ptr<void, decltype(&CloseHandle)> handle(raw_handle,
                                                         CloseHandle);

    const DWORD required_size = GetFinalPathNameByHandleW(
        handle.get(), nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);

    if (required_size == 0) {
        *error_out = GetLastError();
        return {};
    }

    std::vector<WCHAR> buffer(required_size + 1, L'\0');

    const DWORD actual_size = GetFinalPathNameByHandleW(
        handle.get(), buffer.data(), static_cast<DWORD>(buffer.size()),
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);

    if (actual_size == 0) {
        *error_out = GetLastError();
        return {};
    }

    std::wstring resolved_path(buffer.data(), actual_size);

    /* Strip extended-length prefixes so the path works with _stat64.
        "\\?\UNC\server\share\..." -> "\\server\share\..."
        "\\?\C:\..." -> "C:\..." */
    static const std::wstring unc_path_prefix = L"\\\\?\\UNC\\";
    static const std::wstring long_path_prefix = L"\\\\?\\";

    if (resolved_path.compare(0, unc_path_prefix.size(), unc_path_prefix) ==
        0) {
        resolved_path = L"\\\\" + resolved_path.substr(unc_path_prefix.size());
    } else if (resolved_path.compare(0, long_path_prefix.size(),
                                     long_path_prefix) == 0) {
        resolved_path.erase(0, long_path_prefix.size());
    }

    return toNormalString(resolved_path);
}
#endif

} // namespace details

// #############################################################
// #                   GENERAL UTILITIES                       #
// #############################################################

/**
 * @brief Splits a string into tokens by a single-character delimiter.
 *
 * @param text Input string.
 * @param delimiter Separator character (default: space).
 * @param allow_empty If true, keeps empty tokens between delimiters; if false,
 * skips them.
 * @return Vector of tokens.
 * @throws std::bad_alloc If allocation fails.
 */
inline std::vector<std::string>
split(const std::string &text, char delimiter = ' ', bool allow_empty = true) {
    const std::size_t text_length = text.size();
    if (text_length < 1) {
        if (allow_empty) {
            return {""};
        }
        return {};
    }

    const char *const data = text.data();
    const char *const end = data + text_length;

    std::vector<std::string> result;
    if (text_length <= 4096) {
        /* Heuristic capacity guess: (text_length / 7) ~ "average token length"
            picked arbitrarily. +1 avoids the 0-capacity case for short strings
            (e.g. 5/7 = 0) */
        result.reserve((text_length / 7) + 1);
    } else {
        result.reserve(static_cast<std::size_t>(
                           std::count(text.begin(), text.end(), delimiter)) +
                       1);
    }

    if (allow_empty) {
        const char *segment = data;
        while (true) {
            // segment never passes end, so the distance is never negative
            const char *hit = static_cast<const char *>(std::memchr(
                segment, delimiter, static_cast<std::size_t>(end - segment)));

            if (hit == nullptr) {
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

        while ((hit = static_cast<const char *>(std::memchr(
                    segment, delimiter,
                    static_cast<std::size_t>(end - segment)))) != nullptr) {

            // Skip empty token (hit == segment), e.g. for ",a", "a,", "a,,b"
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
 * @tparam T Arithmetic (numeric) type.
 * @param value Value to clamp.
 * @param min_val Lower bound (inclusive).
 * @param max_val Upper bound (inclusive).
 * @return value restricted to [min_val, max_val].
 */
template <typename T, details::EnableIfArithmetic<T> = 0>
inline T clamp(T value, T min_val, T max_val) {
    /* A NaN bound carries no ordering, so neither a range nor the endpoint
        swap below can be formed from it. Handing the value back keeps the
        result independent of the side the NaN arrived on */
    if (std::isnan(min_val) || std::isnan(max_val)) {
        return value;
    }

    details::fixMinMax(min_val, max_val);

    if (value < min_val) {
        return min_val;
    }
    if (value > max_val) {
        return max_val;
    }

    return value;
}

/**
 * @brief Checks whether a value is within the inclusive range [min_val,
 *        max_val].
 *
 * @tparam T Arithmetic (numeric) type.
 * @param value Value to test.
 * @param min_val Lower bound (inclusive).
 * @param max_val Upper bound (inclusive).
 * @return true if value is in range (inclusive), false otherwise.
 */
template <typename T, details::EnableIfArithmetic<T> = 0>
inline bool isInRange(T value, T min_val, T max_val) {
    details::fixMinMax(min_val, max_val);
    return value >= min_val && value <= max_val;
}

/**
 * @brief Trims ASCII whitespace from both ends of a string in-place.
 *
 * @param text String to be trimmed.
 */
inline void trim(std::string &text) {

    std::size_t start_index = 0;

    // Find the first non-whitespace character from the beginning
    while (start_index < text.size() &&
           details::isAsciiSpace(text[start_index])) {
        start_index++;
    }

    // If the string contains only whitespaces or is empty, clear it
    if (start_index == text.size()) {
        text.clear();
        return;
    }

    std::size_t end_index = text.size();

    // Find the last non-whitespace character from the end
    while (end_index > start_index &&
           details::isAsciiSpace(text[end_index - 1])) {
        end_index--;
    }

    // Remove trailing and leading whitespaces
    text.erase(end_index);
    text.erase(0, start_index);
}

/**
 * @brief Trims leading ASCII whitespace from a string in-place.
 *
 * @param text String to be trimmed.
 */
inline void ltrim(std::string &text) {

    std::size_t start_index = 0;

    // Find the first non-whitespace character from the beginning
    while (start_index < text.size() &&
           details::isAsciiSpace(text[start_index])) {
        start_index++;
    }

    // If the string contains only whitespaces or is empty, clear it
    if (start_index == text.size()) {
        text.clear();
        return;
    }

    text.erase(0, start_index);
}

/**
 * @brief Trims trailing ASCII whitespace from a string in-place.
 *
 * @param text String to be trimmed.
 */
inline void rtrim(std::string &text) {
    std::size_t end_index = text.size();

    // Find the last non-whitespace character from the end
    while (end_index > 0 && details::isAsciiSpace(text[end_index - 1])) {
        end_index--;
    }

    if (end_index == 0) {
        text.clear();
        return;
    }
    // Remove trailing and leading whitespaces
    text.erase(end_index);
}

/**
 * @brief Replaces all non-overlapping occurrences of a substring in a string,
 *        in-place, scanning left-to-right.
 *
 * @param text String to modify.
 * @param old_substr Substring to search for. Must be non-empty to have any
 * effect.
 * @param new_substr Replacement substring.
 * @throws std::bad_alloc If allocation fails, or std::length_error if the
 * result would exceed max_size().
 */
inline void replaceAll(std::string &text, const std::string &old_substr,
                       const std::string &new_substr) {
    if (old_substr.empty() || text.empty()) {
        return;
    }

    const std::size_t text_length = text.size();
    const std::size_t old_sub_len = old_substr.size();
    const std::size_t new_sub_len = new_substr.size();

    // Equal-length doesn't require memory reallocation
    if (old_sub_len == new_sub_len) {
        details::replaceSameLength(text, old_substr, new_substr);
        return;
    }

    /* Only a longer replacement can make the text grow, so for a shorter one
        the current length is already an upper bound on the result */
    std::size_t reserve_size = text_length;

    /* Both paths have to locate the first occurrence to know whether there is
        anything to do */
    const char *first_match = nullptr;

    if (text_length >= 4096) {
        /* Large text path: one counting pass buys an exact capacity, which
            beats letting a long result reallocate several times while growing
         */
        const std::size_t occurrences =
            details::countOccurrences(text, old_substr, &first_match);

        if (occurrences == 0) {
            return;
        }
        if (new_sub_len > old_sub_len) {
            reserve_size = details::cappedReserveSize(
                text_length, occurrences, new_sub_len - old_sub_len);
        }
    } else {
        // Small text path: skip counting, reserve with 50% overhead instead
        first_match = details::memFind(text.data(), text_length,
                                       old_substr.data(), old_sub_len);

        if (first_match == nullptr) {
            return;
        }
        if (new_sub_len > old_sub_len) {
            reserve_size =
                details::cappedReserveSize(text_length, 1, text_length / 2);
        }
    }

    std::string result = details::buildReplaced(text, old_substr, new_substr,
                                                reserve_size, first_match);

    // Transfer the new string data back to the original text variable
    text.swap(result);
}

/**
 * @brief Checks whether the given string pointer is null or points to an empty
 *        string.
 *
 * @param text Pointer to the string to check.
 * @return True if the pointer is null or the string holds no bytes; otherwise
 * false.
 */
inline bool isNullOrEmpty(const std::string *text) {
    return text == nullptr || text->empty();
}

/**
 * @brief Checks whether the given C string is null or empty.
 *
 * @param text Null-terminated string to check.
 * @return True if the pointer is null or the string is empty; otherwise false.
 */
inline bool isNullOrEmpty(const char *text) {
    return text == nullptr || text[0] == '\0';
}

/**
 * @brief Checks whether the given C string is null or empty, without reading
 *        past buffer_size bytes.
 *
 * @param text String to check; a terminator is not required.
 * @param buffer_size Number of bytes that can be read through @p text.
 * @return True if the pointer is null, the buffer holds no bytes, or the
 * string is empty; otherwise false.
 */
inline bool isNullOrEmpty(const char *text, std::size_t buffer_size) {
    return text == nullptr || buffer_size < 1 || text[0] == '\0';
}

/**
 * @brief Checks whether the given string pointer is null or contains only
 *        ASCII whitespace characters.
 *
 * @param text Pointer to the string to check.
 * @return True if the pointer is null or every byte the string holds is ASCII
 * whitespace; otherwise false.
 */
inline bool isNullOrWhiteSpace(const std::string *text) {
    return text == nullptr ||
           std::all_of(text->begin(), text->end(),
                       [](char ch) { return details::isAsciiSpace(ch); });
}

/**
 * @brief Checks whether the given C string is null, empty or made up only of
 *        ASCII whitespace, without reading past buffer_size bytes.
 *
 * @param text String to check; a terminator is not required.
 * @param buffer_size Number of bytes that can be read through @p text.
 * @return True if the pointer is null, the buffer holds no bytes, or every
 * character examined is ASCII whitespace; otherwise false.
 */
inline bool isNullOrWhiteSpace(const char *text, std::size_t buffer_size) {
    if (isNullOrEmpty(text, buffer_size)) {
        return true;
    }

    for (std::size_t index = 0; index < buffer_size; index++) {
        if (text[index] == '\0') {
            return true;
        }
        if (!details::isAsciiSpace(text[index])) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Checks whether the given C string is null, empty or made up only of
 *        ASCII whitespace.
 *
 * @param text Null-terminated string to check.
 * @return True if the pointer is null or every character before the
 * terminator is ASCII whitespace; otherwise false.
 */
inline bool isNullOrWhiteSpace(const char *text) {
    if (text == nullptr) {
        return true;
    }

    return isNullOrWhiteSpace(text, std::strlen(text));
}

/**
 * @brief Appends text elements to an existing string, separated by separator.
 *        Empty elements are skipped.
 *
 * @tparam Args Types convertible to std::string.
 * @param text1 String to append to.
 * @param separator Delimiter inserted between non-empty elements.
 * @param texts Text elements to join; with none, @p text1 is left unchanged.
 */
template <typename... Args, details::EnableIfConvertibleToString<Args...> = 0>
inline void join(std::string &text1, const std::string &separator,
                 Args... texts) {
    details::joinImpl(separator, text1, texts...);
}

/**
 * @brief Creates a new string by joining text elements with a separator.
 *        Empty elements are skipped.
 *
 * @tparam Args Types convertible to std::string.
 * @param separator Delimiter inserted between non-empty elements.
 * @param texts Text elements to join; with none, the result is empty.
 * @return Joined string.
 */
template <typename... Args, details::EnableIfConvertibleToString<Args...> = 0>
inline std::string joinCopy(const std::string &separator, Args... texts) {
    std::string result;
    details::joinImpl(separator, result, texts...);
    return result;
}

// #############################################################
// #               FILE UTILITIES                              #
// #############################################################
namespace files {

/**
 * @brief Represents the type or status of a file system path.
 */
enum class PathType : std::uint8_t {
    NOT_FOUND,       ///< The path does not exist.
    FILE,            ///< A regular file.
    DIRECTORY,       ///< A directory.
    SYMLINK,         ///< A symbolic link.
    JUNCTION,        ///< A Windows directory junction or volume mount point.
    BROKEN_SYMLINK,  ///< A symbolic link that points to a non-existent target.
    SYMLINK_LOOP,    ///< A symbolic link loop was detected during resolution.
    CHAR_DEVICE,     ///< A character special file (device).
    BLOCK_DEVICE,    ///< A block special file (device).
    PIPE,            ///< A FIFO special file (named pipe).
    SOCKET,          ///< A local (UNIX domain) socket.
    OTHER,           ///< An unknown or unsupported file type.
    SYSTEM_ERROR,    ///< A general system error occurred during the check.
    PERMISSION_ERROR ///< Access denied (insufficient permissions).
};

#if defined(ILC_OS_WINDOWS)
/**
 * @brief Retrieves the file system path type.
 *
 * @param path The file system path to check (UTF-8 encoded).
 * @param follow_symlink If true, resolves symbolic links and junctions to the
 * target's type, which is also when PathType::BROKEN_SYMLINK and
 * PathType::SYMLINK_LOOP can be returned. If false, every link is reported as
 * PathType::SYMLINK or PathType::JUNCTION.
 * @return PathType representing the type of the path or the specific error
 * encountered.
 */
inline PathType getType(const std::string &path,
                        const bool follow_symlink = false) {
    const std::wstring path_wide = details::toWideString(path);

    // Covers both an empty path and one that is not valid UTF-8
    if (path_wide.empty()) {
        return PathType::NOT_FOUND;
    }

    WIN32_FIND_DATAW find_data{};
    const HANDLE find_handle = FindFirstFileW(path_wide.c_str(), &find_data);
    bool is_symlink = false;

    if (find_handle != INVALID_HANDLE_VALUE) {
        FindClose(find_handle);

        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            if (find_data.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT) {
                if (!follow_symlink) {
                    return PathType::JUNCTION;
                }

                /* A junction is a directory carrying a reparse point, so
                    _wstat64 describes the junction itself instead of following
                    it. Resolving it explicitly, exactly like a symlink, is the
                    only way to reach the target */
                is_symlink = true;
            } else {
                is_symlink = (find_data.dwReserved0 == IO_REPARSE_TAG_SYMLINK);
            }
        }
    }

    if (!follow_symlink && is_symlink) {
        return PathType::SYMLINK;
    } else if (is_symlink) {
        DWORD resolve_error = ERROR_SUCCESS;
        const std::string target_path =
            details::getResolvedTargetPath(path, &resolve_error);
        if (isNullOrEmpty(&target_path)) {
            /* Windows reports a link that loops as ERROR_CANT_RESOLVE_FILENAME
                and one whose target is gone as ERROR_FILE_NOT_FOUND, for both
                symlinks and junctions alike */
            if (resolve_error == ERROR_CANT_RESOLVE_FILENAME) {
                return PathType::SYMLINK_LOOP;
            }

            return PathType::BROKEN_SYMLINK;
        }

        return getType(target_path, true);
    }

    struct _stat64 file_info {};
    const int stat_result = _wstat64(path_wide.c_str(), &file_info);

    if (stat_result < 0) {
        const int err = errno;
        if (err == ENOENT) {
            return PathType::NOT_FOUND;
        } else if (_waccess(path_wide.c_str(), 0) != 0) {
            return PathType::PERMISSION_ERROR;
        }

        return PathType::SYSTEM_ERROR;
    }

    switch (file_info.st_mode & S_IFMT) {
    case S_IFREG:
        return PathType::FILE;
    case S_IFDIR:
        return PathType::DIRECTORY;
    case S_IFIFO:
        return PathType::PIPE;
    case S_IFCHR:
        return PathType::CHAR_DEVICE;
    default:
        return PathType::OTHER;
    }
}

#endif

#if defined(ILC_OS_LINUX)

/**
 * @brief Retrieves the file system path type.
 *
 * @param path The file system path to check.
 * @param follow_symlink If true, resolves symlinks to the target's type,
 * which is also when PathType::BROKEN_SYMLINK and PathType::SYMLINK_LOOP can
 * be returned. If false, every symbolic link is reported as
 * PathType::SYMLINK.
 * @return PathType representing the type of the path or the specific error
 * encountered.
 */
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
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
            return PathType::SYMLINK_LOOP;
        }
        if (err != ENOENT && err != ENOTDIR) {
            if (err == EACCES || err == EPERM) {
                return PathType::PERMISSION_ERROR;
            }
            return PathType::SYSTEM_ERROR;
        }

        /* If follow_symlink was true and it got ENOENT, it might be a broken
            symlink. It is verified by checking the path again without following
            symlinks */
        if (follow_symlink) {
            const PathType type = getType(path, false);
            if (type == PathType::SYMLINK) {
                return PathType::BROKEN_SYMLINK;
            }
            if (type == PathType::PERMISSION_ERROR ||
                type == PathType::SYSTEM_ERROR ||
                type == PathType::SYMLINK_LOOP) {
                return type;
            }
        }

        return PathType::NOT_FOUND;
    }

// Determine specific file type from the stat mode
#if defined(S_ISLNK)
    if (S_ISLNK(file_info.st_mode)) {
        return PathType::SYMLINK;
    }
#endif

    if (S_ISREG(file_info.st_mode)) {
        return PathType::FILE;
    }
    if (S_ISDIR(file_info.st_mode)) {
        return PathType::DIRECTORY;
    }
    if (S_ISCHR(file_info.st_mode)) {
        return PathType::CHAR_DEVICE;
    }
    if (S_ISBLK(file_info.st_mode)) {
        return PathType::BLOCK_DEVICE;
    }
    if (S_ISFIFO(file_info.st_mode)) {
        return PathType::PIPE;
    }

#if defined(S_ISSOCK)
    if (S_ISSOCK(file_info.st_mode)) {
        return PathType::SOCKET;
    }
#endif

    return PathType::OTHER;
}
#endif

/**
 * @brief Checks if a given path exists on the file system.
 *
 * @param path The file system path to check.
 * @param follow_symlink If true, the check applies to the symlink target; if
 * false, to the link itself.
 * @return true if the path exists, false if it is not found.
 * @throw std::runtime_error If the check itself could not be completed. The
 * message carries the path and the reason.
 */
inline bool exists(const std::string &path, const bool follow_symlink = false) {
    const PathType type = getType(path, follow_symlink);

    switch (type) {
    case PathType::NOT_FOUND:
    case PathType::BROKEN_SYMLINK:
        return false;
    case PathType::PERMISSION_ERROR:
        throw std::runtime_error("Cannot check \"" + path +
                                 "\": permission denied");
    case PathType::SYMLINK_LOOP:
        throw std::runtime_error("Cannot check \"" + path +
                                 "\": symbolic link loop");
    case PathType::SYSTEM_ERROR:
        throw std::runtime_error("Cannot check \"" + path +
                                 "\": file system error");
    default:
        return true;
    }
}

} // namespace files

} // namespace ilc

#undef ILC_OS_WINDOWS
#undef ILC_OS_LINUX

#endif // I_LOVE_CPP_HPP
