# i-love-cpp - Function Reference #

Detailed description, parameters, behaviour and examples for every function in `ilc.hpp`. For a quick overview of what is available, see [README.md](README.md).

Every example assumes the library header plus the standard headers the snippets use, and, where output is shown, that the result is printed:

```cpp
#include "ilc.hpp"
#include <iostream>   // std::cout
#include <string>
#include <vector>
#include <cstdlib>    // std::getenv
#include <limits>     // std::numeric_limits
```

## General Utilities (`ilc::`) ##

> ℹ️ **INFO** Every whitespace check in the library uses the same fixed `ASCII` set *(space, `\n`, `\r`, `\t`, `\v`, `\f`)* and **never** consults the locale. Whitespace outside `ASCII`, such as a `UTF-8` no-break space, is **not** recognised.

### `split` ###

> ℹ️ **INFO** Partial standard library equivalent: `std::views::split` since `C++20`.

```cpp
std::vector<std::string> split(const std::string &text,
                               char delimiter = ' ',
                               bool allow_empty = true);
```

Splits a string into tokens using a single-character delimiter.

Parameters:

- `text` - the string to split.
- `delimiter` - the separator character *(default: space)*.
- `allow_empty` - whether empty segments are kept as tokens *(default: `true`)*.

Returns a `std::vector<std::string>` of tokens.

With `allow_empty` set to **true**, an element is returned for **every** segment between delimiters, including empty ones, so the token count is always **one more** than the number of delimiters. An empty input is one empty segment, giving *[""]*. With `allow_empty` set to **false**, empty segments are skipped instead, so consecutive delimiters act as a single separator and leading or trailing delimiters are ignored.

**Throws:** `std::bad_alloc` if allocation fails.

```cpp
ilc::split("a,,b", ',', true);       // -> {"a", "", "b"}
ilc::split(",a,", ',', true);        // -> {"", "a", ""}
ilc::split("", ',', true);           // -> {""}

ilc::split("a,,b", ',', false);      // -> {"a", "b"}
ilc::split("  one  two ", ' ', false); // -> {"one", "two"}
ilc::split(",", ',', false);         // -> {}
```

### `clamp` ###

> ℹ️ **INFO** Standard library equivalent: `std::clamp` since `C++17`.

```cpp
template <typename T>
T clamp(T value, T min_val, T max_val);
```

Restricts a numeric value to a given inclusive range `[min_val, max_val]`. Takes part in overload resolution only when `T` is a number *(including bool and char)* type.

Parameters:

- `value` - the value to clamp.
- `min_val` - lower bound *(inclusive)*.
- `max_val` - upper bound *(inclusive)*.

Returns the **lower** bound if `value` is below the range and the **upper** bound if it is above, otherwise `value` unchanged. The bounds are taken **after** any internal swap, so when the arguments arrive swapped the value that comes back is **not** necessarily the parameter of the matching name. For example *`clamp(-1, 10, 0)`* returns *0*, the lower bound, even though it was passed as `max_val`.

Swapped bounds are accepted and swapped internally, so the function is order-agnostic. This differs from `std::clamp` *(added in `C++17`)*, where passing `min_val > max_val` is undefined behaviour.

> ℹ️ **INFO** A `NaN` value is returned unchanged, and so is any value when one of the bounds is `NaN`, since `NaN` has no ordering and cannot form a range. Infinities are ordinary values here: an infinite `value` is clamped to a finite bound like any other number, and an infinite bound never limits the value on that side. `-0.0` and `+0.0` compare **equal**, so neither counts as outside a range bounded by the other, and the sign of a zero result is whatever the returned argument carried. A zero `value` inside the range comes back with its own sign, and a clamped result takes the sign of the bound. Subnormal numbers need no special handling and are compared like any other value.

```cpp
ilc::clamp(15, 0, 10);   // -> 10
ilc::clamp(-3, 0, 10);   // -> 0
ilc::clamp(5, 0, 10);    // -> 5
ilc::clamp(5, 10, 0);    // -> 5  (bounds swapped internally)

const double inf = std::numeric_limits<double>::infinity();
const double nan = std::numeric_limits<double>::quiet_NaN();

ilc::clamp(inf, 0.0, 10.0);     // -> 10
ilc::clamp(-inf, 0.0, 10.0);    // -> 0
ilc::clamp(5.0, 0.0, inf);      // -> 5    (infinite bound never limits)
ilc::clamp(-1.0, inf, 0.0);     // -> 0    (bounds swapped internally)
ilc::clamp(nan, 0.0, 10.0);     // -> NaN  (NaN value returned unchanged)
ilc::clamp(15.0, 0.0, nan);     // -> 15   (NaN bound, value returned unchanged)
ilc::clamp(-0.0, 0.0, 10.0);    // -> -0.0 (equal to +0.0, so inside the range)
ilc::clamp(-1.0, -0.0, 10.0);   // -> -0.0 (clamped result takes the bound's sign)
```

### `isInRange` ###

> ℹ️ **INFO** No standard library equivalent.

```cpp
template <typename T>
bool isInRange(T value, T min_val, T max_val);
```

Checks whether a value lies inside a given inclusive range `[min_val, max_val]`, with the same swapped-bounds handling as `clamp`. Takes part in overload resolution only when `T` is an arithmetic type.

Parameters:

- `value` - the value to test.
- `min_val` - lower bound *(inclusive)*.
- `max_val` - upper bound *(inclusive)*.

Returns `true` when `value` is within the range, `false` otherwise.

> ℹ️ **INFO** A `NaN` value or bound always yields `false`. An infinite bound is inclusive, so `+Infinity` is in range `[0, +Infinity]` but not in `[0, 10]`.

```cpp
ilc::isInRange(5, 0, 10);    // -> true
ilc::isInRange(15, 0, 10);   // -> false
ilc::isInRange(0, 0, 10);    // -> true  (inclusive)
ilc::isInRange(5, 10, 0);    // -> true  (bounds swapped internally)

const double inf = std::numeric_limits<double>::infinity();
const double nan = std::numeric_limits<double>::quiet_NaN();

ilc::isInRange(inf, 0.0, inf);       // -> true  (infinite bound is inclusive)
ilc::isInRange(inf, 0.0, 10.0);      // -> false
ilc::isInRange(1e308, -inf, inf);    // -> true
ilc::isInRange(5.0, inf, 0.0);       // -> true  (bounds swapped internally)
ilc::isInRange(nan, -inf, inf);      // -> false (NaN value)
ilc::isInRange(5.0, 0.0, nan);       // -> false (NaN bound)
ilc::isInRange(-0.0, 0.0, 10.0);     // -> true  (-0.0 equals +0.0)
```

### `trim` / `ltrim` / `rtrim` ###

> ℹ️ **INFO** No standard library equivalent.

```cpp
void trim(std::string &text);   // both ends
void ltrim(std::string &text);  // leading end only
void rtrim(std::string &text);  // trailing end only
```

Remove `ASCII` whitespace from a string **in-place**. A string made only of whitespace becomes empty.

Parameters:

- `text` - the string to trim, modified in place.

```cpp
std::string s = "  hello  ";
ilc::trim(s);    // s == "hello"

std::string l = "  hello  ";
ilc::ltrim(l);   // l == "hello  "

std::string r = "  hello  ";
ilc::rtrim(r);   // r == "  hello"
```

### `replaceAll` ###

> ℹ️ **INFO** No standard library equivalent.

```cpp
void replaceAll(std::string &text,
                const std::string &old_substr,
                const std::string &new_substr);
```

Replaces all non-overlapping occurrences of a substring **in-place**, scanning left to right.

Parameters:

- `text` - the string to modify in place.
- `old_substr` - the substring to search for, must be non-empty to have any effect.
- `new_substr` - the replacement, may be empty to delete every match.

Each match is consumed whole, so the search resumes past it rather than inside it, so *"aaa"* holds one *"aa"*, not two. Nothing happens when `old_substr` or `text` is empty, or when `old_substr` is not found. Both `old_substr` and `new_substr` are safe to alias `text` itself.

**Throws:** `std::bad_alloc` if allocation fails, or `std::length_error` if the result would exceed `max_size()`.

```cpp
std::string s = "one,two,three";
ilc::replaceAll(s, ",", " | ");   // s == "one | two | three"

std::string a = "aaa";
ilc::replaceAll(a, "aa", "b");    // a == "ba"  (first "aa" -> "b", trailing "a" left as is)

std::string d = "a.b.c";
ilc::replaceAll(d, ".", "");      // d == "abc" (empty replacement deletes matches)
```

### `isNullOrEmpty` ###

> ℹ️ **INFO** No standard library equivalent.

```cpp
bool isNullOrEmpty(const std::string *text);
bool isNullOrEmpty(const char *text);
bool isNullOrEmpty(const char *text, std::size_t buffer_size);
```

Checks whether a string is null or empty.

Parameters:

- `text` - pointer to the string to check.
- `buffer_size` - number of bytes that may be read through `text` *(third overload only)*.

Returns `true` when the pointer is null or the string is empty, `false` otherwise.

The `std::string *` overload decides emptiness from `size()`, so a string holding only `\0` bytes is **not** empty. The `const char *` overloads decide it from the first `\0` byte instead, so `&text` and `text.c_str()` are **not** interchangeable here. The buffer-size overload never reads past `buffer_size` bytes, and a size of **0** counts as empty without the buffer being touched. Use it when the input may not be null-terminated at all.


```cpp
// The real use: a pointer you don't control, such as a function argument, a C API
// return, a config lookup. One check covers both "null" and "empty".
void greet(const char *name) {
    if (ilc::isNullOrEmpty(name)) {   // name may be nullptr or ""
        name = "stranger";
    }
    std::cout << "Hello, " << name << "!\n";
}

// std::string* vs const char* disagree on the same bytes:
std::string zeros("\0\0", 2);        // size() == 2
ilc::isNullOrEmpty(&zeros);          // -> false (size() is 2)
ilc::isNullOrEmpty(zeros.c_str());   // -> true  (first byte is '\0')

// Buffer overload, for input that may not be null-terminated:
char buf[3] = {'a', 'b', 'c'};
ilc::isNullOrEmpty(buf, 3);          // -> false
ilc::isNullOrEmpty(buf, 0);          // -> true  (size 0 counts as empty)
```

### `isNullOrWhiteSpace` ###

> ℹ️ **INFO** No standard library equivalent.

```cpp
bool isNullOrWhiteSpace(const std::string *text);
bool isNullOrWhiteSpace(const char *text, std::size_t buffer_size);
bool isNullOrWhiteSpace(const char *text);
```

Checks whether a string is null, empty or made up only of `ASCII` whitespace.

Parameters:

- `text` - pointer to the string to check.
- `buffer_size` - number of bytes that may be read through `text` *(second overload only)*.

Returns `true` when the pointer is null or every byte examined is `ASCII` whitespace, `false` otherwise.

The `std::string *` overload examines every byte up to `size()`, and an embedded `\0` is **not** whitespace and does **not** end the string, so such a string returns `false`. The buffer-size overload instead stops at the first `\0` or after `buffer_size` bytes, whichever comes first, and never reads past a `\0`. The bare `const char *` overload is safe on a possibly-null pointer such as the result of `std::getenv`, because the null check runs before the length is measured.

```cpp
ilc::isNullOrWhiteSpace("   \t\n");   // -> true
ilc::isNullOrWhiteSpace("  x ");      // -> false

// getenv returns nullptr when unset, so null, empty and whitespace-only are caught in one check:
const char *home = std::getenv("HOME");
if (ilc::isNullOrWhiteSpace(home)) {
    // unset, empty, or only whitespace -> not usable
}
```

### `join` / `joinCopy` ###

> ℹ️ **INFO** Partial standard library equivalent: `std::views::join_with` since `C++23`.

```cpp
void join(std::string &text1, const std::string &separator, Args... texts);
std::string joinCopy(const std::string &separator, Args... texts);
```

Join text elements with a separator, skipping empty elements. `join` appends to an existing string, while `joinCopy` builds and returns a new one. Every element must be convertible to `std::string`.

Parameters:

- `text1` - the string to append to, modified in place *(`join` only)*.
- `separator` - the delimiter inserted between non-empty elements.
- `texts` - the elements to join. With none supplied, nothing is appended.

`joinCopy` returns the joined string.

Only elements with **no bytes at all** count as empty. An element made purely of whitespace is content and is joined as-is. The separator is inserted whenever the target string is already non-empty, **regardless of what it currently ends with**, so appending *"b"* to *"a,"* with *","* as the separator yields *"a,,b"*. Only the elements joined within one call are guaranteed to be singly separated. The target passed to `join` may safely appear among the elements, since every element is taken by value and copied before anything is appended.

```cpp
std::string out = "a";
ilc::join(out, ",", "b", "", "c");   // out == "a,b,c"  (empty element skipped)

std::string csv = ilc::joinCopy(", ", "one", "two", "three");
// csv == "one, two, three"
```

## File Utilities (`ilc::files::`) ##

### `PathType` ###

```cpp
enum class PathType : std::uint8_t {
    NOT_FOUND,
    FILE,
    DIRECTORY,
    SYMLINK,
    JUNCTION,
    BROKEN_SYMLINK,
    SYMLINK_LOOP,
    CHAR_DEVICE,
    BLOCK_DEVICE,
    PIPE,
    SOCKET,
    OTHER,
    SYSTEM_ERROR,
    PERMISSION_ERROR
};
```

Describes what a path points to, or the error met while checking it. Returned by `getType`.

- `NOT_FOUND` - the path does not exist.
- `FILE` - a regular file.
- `DIRECTORY` - a directory.
- `SYMLINK` - a symbolic link.
- `JUNCTION` - a `Windows` directory junction or volume mount point.
- `BROKEN_SYMLINK` - a symbolic link whose target does not exist.
- `SYMLINK_LOOP` - a symbolic link loop was detected while resolving.
- `CHAR_DEVICE` - a character special file *(device)*.
- `BLOCK_DEVICE` - a block special file *(device)*.
- `PIPE` - a `FIFO` special file *(named pipe)*.
- `SOCKET` - a local *(`UNIX` domain)* socket.
- `OTHER` - an unknown or unsupported file type.
- `SYSTEM_ERROR` - a general system error occurred during the check.
- `PERMISSION_ERROR` - access was denied.

> ℹ️ **INFO** `JUNCTION` can only be returned on `Windows`. `BLOCK_DEVICE` and `SOCKET` can only be returned on `Linux`.

### `getType` ###

> ℹ️ **INFO** Standard library equivalent: `std::filesystem::status` / `std::filesystem::symlink_status` since `C++17`.

```cpp
PathType getType(const std::string &path, bool follow_symlink = false);
```

Returns the `PathType` of a filesystem path.

Parameters:

- `path` - the filesystem path to inspect *(`UTF-8` encoded on `Windows`)*.
- `follow_symlink` - whether to resolve links to their target *(default: `false`)*.

With `follow_symlink` set to **false** *(the default)* a link is reported as `SYMLINK`, or `JUNCTION` on `Windows`, without inspecting its target. With it set to **true** the target is resolved instead, which is also the only case in which `BROKEN_SYMLINK` and `SYMLINK_LOOP` can be returned.

Neither platform applies `Unicode` normalisation, so a precomposed *"é"* and an *"e"* followed by a combining accent are different names. They **do** differ on `UTF-8` validation: `Windows` rejects an invalid `UTF-8` path and returns `NOT_FOUND`, while `Linux` passes the bytes to the system unchanged and never validates them. Both platforms return `NOT_FOUND` for a path that cannot name anything: an empty string, a regular file followed by a trailing separator *(e.g. `/etc/hosts/`)*, or a path continuing below a regular file.

> ⚠️ **WARNING** Directory permissions are read differently per platform. On `Linux`, inspecting a path needs **search** *(execute, `x`)* permission on each parent directory, and without it the result is `PERMISSION_ERROR`. Removing only **read** *(`r`, the right to list)* does **not** cause an error, and a file inside is still reported as its real type. On `Windows`, a file whose directory denies listing is also reported correctly, since reading its attributes does not require listing the directory.

> ℹ️ **INFO** `Windows` recognises both symbolic links and directory junctions, and `follow_symlink` resolves either the same way. Other reparse points *(OneDrive placeholders, deduplicated or compressed files, Store app aliases)* are reported as their underlying file type.

```cpp
using ilc::files::PathType;

if (ilc::files::getType("/etc/hosts") == PathType::FILE) {
    // regular file
}

// A symlink to a directory, inspected both ways:
ilc::files::getType("/link");        // -> PathType::SYMLINK    (the link itself)
ilc::files::getType("/link", true);  // -> PathType::DIRECTORY  (its target)
```

### `exists` ###

> ℹ️ **INFO** Standard library equivalent: `std::filesystem::exists` since `C++17`.

```cpp
bool exists(const std::string &path, bool follow_symlink = false);
```

Checks whether a path exists.

Parameters:

- `path` - the filesystem path to check.
- `follow_symlink` - whether the check applies to the link target *(true)* or to the link itself *(false, the default)*.

Returns `true` when the path exists, `false` when it is not found. Every **non-error** type other than `NOT_FOUND` and `BROKEN_SYMLINK` counts as existing, including pipes, sockets and devices. The error states `PERMISSION_ERROR`, `SYMLINK_LOOP` and `SYSTEM_ERROR` do **not** return a value at all, they throw *(see below)*.

A broken symlink is where `follow_symlink` matters: with the default `false` the question is about the link, which is there, so the result is `true`. With `true` it is about the target, which is not, so the result is `false`. In both cases a broken link is a correctly read filesystem state, not a failure.

**Throws:** `std::runtime_error` when the check itself could not be completed: denied access, another system error, or a symlink **loop met while resolving**. A loop only surfaces with `follow_symlink = true`. With `follow_symlink = false` the question is about the link itself, which exists, so *`exists(loop, false)`* returns `true` while *`exists(loop, true)`* throws. The message carries the path and the reason. A plain absence is never an error and returns `false`.

```cpp
if (ilc::files::exists("/tmp")) {
    // path is present
}

// Broken symlink: the link is there, the target is gone.
ilc::files::exists("/broken_link");        // -> true  (the link exists)
ilc::files::exists("/broken_link", true);  // -> false (the target does not)
```