# I love C++ (i-love-cpp) #

**Version:** `1.0.0` \
**License:** `MIT`

A lightweight, header-only `C++` utility library with compatibility down to `C++11`.

It collects small, practical helpers that tend to go missing in older `C++` standards and end up being rewritten in project after project.

## Features ##

- Header-only
- `C++11` compatible
- Small and easy to integrate
- Cross-platform utilities

## Compatibility ##

- `Language standard` - **`C++11`** or newer
- `System support` - `Linux`, `Windows`

> ⚠️ **WARNING** On any system other than `Linux` and `Windows` the header **stops the project build with an error**.

## Integration ##

Just copy `ilc.hpp` into your project's include directory and include it where needed:

```cpp
#include "ilc.hpp"
```

**No separate build step or linking** is required.

> ℹ️ **INFO** On `Windows` the header pulls in `windows.h` with `WIN32_LEAN_AND_MEAN` and `NOMINMAX` defined, then restores whatever the including code had set before. `NOMINMAX` only takes effect when `ilc.hpp` is the **first** to reach `windows.h`. Define `NOMINMAX` in your build *(e.g. `-DNOMINMAX`)* to make it order-independent.

## Version Macros ##

- `ILC_VERSION_MAJOR`, `ILC_VERSION_MINOR`, `ILC_VERSION_PATCH` - version components as separate integers.
- `ILC_VERSION` - all three combined into a single integer for comparisons.

```cpp
#if ILC_VERSION >= 10000 // requires i-love-cpp 1.0.0 or newer
...
#endif
```

## Namespaces ##

- `ilc::` - general-purpose utilities
- `ilc::files::` - filesystem and path-related helpers

## Example ##

```cpp
#include <iostream>
#include <string>
#include <vector>
#include "ilc.hpp"

int main() {
    std::string text = "   one,two,,three   ";
    ilc::trim(text);

    std::vector<std::string> parts = ilc::split(text, ',', true);

    for (const std::string& part : parts) {
        std::cout << "[" << part << "]\n";
    }

    std::cout << "Clamped value: " << ilc::clamp(15, 0, 10) << '\n';

    if (ilc::files::exists("/tmp")) {
        std::cout << "/tmp exists\n";
    }

    return 0;
}
```

## Available Functions ##

> ℹ️ **INFO** This is a quick overview. Full behaviour, edge cases and platform differences are documented in [`DOCS.md`](DOCS.md).

### General Utilities (`ilc::`) ###

- `split` - Splits a string into tokens using a single-character delimiter.
- `clamp` - Restricts a numeric value to a given inclusive range. Swapped bounds are accepted and swapped internally, **unlike `std::clamp`**, where that is undefined behaviour.
- `isInRange` - Checks whether a numeric value is inside a given inclusive range.
- `trim` - Removes `ASCII` whitespace from both ends of a string in-place.
- `ltrim` - Removes leading `ASCII` whitespace from a string in-place.
- `rtrim` - Removes trailing `ASCII` whitespace from a string in-place.
- `replaceAll` - Replaces all non-overlapping occurrences of a substring in-place.
- `isNullOrEmpty` - Checks whether a string is null or empty. Overloads take a `const std::string *`, a `const char *`, or a `const char *` together with a buffer size for input that may not be terminated.
- `isNullOrWhiteSpace` - Checks whether a string is null, empty or made up only of `ASCII` whitespace. Overloads take a `const std::string *`, a `const char *`, or a `const char *` together with a buffer size for input that may not be terminated.
- `join` - Appends text elements to an existing string with a separator, skipping empty elements.
- `joinCopy` - Creates a new string by joining text elements with a separator, skipping empty elements.

### File Utilities (`ilc::files::`) ###

- `PathType` - Enum describing what a path points to, returned by `getType`.
- `getType` - Returns the detected type or status of a filesystem path as a `PathType` value.
- `exists` - Checks whether a path exists. `follow_symlink` decides whether the check applies to the link or to its target.

## Contributing ##

Bug reports, improvements, and pull requests are welcome.

## License ##

This project is licensed under the MIT License. See the `LICENSE` file for details.