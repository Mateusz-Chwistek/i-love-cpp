# I love C++ (i-love-cpp)

**Version:** 0.1.0

A lightweight, header-only C++ utility library with compatibility down to C++11.

It collects small, practical helpers that tend to go missing in older C++ standards and end up being rewritten in project after project.

## Features

- Header-only
- C++11 compatible
- Small and easy to integrate
- Cross-platform utilities

## Compatibility

- **Language standard:** C++11+
- **Tested on:** Linux
- **Windows support:** Partial (filesystem support is still incomplete)

## Integration

Just copy `ilc.hpp` into your project's include directory and include it where needed:

```cpp
#include "ilc.hpp"
```

No separate build step or linking is required.

## Namespaces

* `ilc::` - general-purpose utilities
* `ilc::files::` - filesystem and path-related helpers

## Example

```cpp
#include <iostream>
#include <string>
#include <vector>
#include "ilc.hpp"

int main() {
    std::string text = "   one,two,,three   ";
    ilc::trim(text);

    std::vector<std::string> parts = ilc::split(text, ',', true);
    ilc::toLower(text);

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

## Available Functions

### General Utilities (`ilc::`)

* **`split`** - Splits a string into tokens using a single-character delimiter.
* **`clamp`** - Restricts a numeric value to a given inclusive range.
* **`isInRange`** - Checks whether a numeric value is inside a given inclusive range.
* **`trim`** - Removes ASCII whitespace from both ends of a string in-place.
* **`replaceAll`** - Replaces all occurrences of a substring in-place.
* **`toLower`** - Converts a string to lowercase in-place.

### File Utilities (`ilc::files::`)

* **`getType`** - Returns the detected type or status of a filesystem path.
* **`exists`** - Checks whether a path exists and reports filesystem errors via exceptions.

## Project Status

This is an early release and the library will grow over time. The current API is intentionally small and focused on utilities that are useful in everyday projects.

## Contributing

Bug reports, improvements, and pull requests are welcome.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.
