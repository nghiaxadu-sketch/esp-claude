// Sample apps installed on first boot (they also serve as working examples of the app format).
#pragma once

#include <cstddef>

namespace builtin_apps {

struct App {
    const char *id;  // file name without extension
    const char *json;
};

extern const App kApps[];
extern const size_t kAppCount;

// A compact example included in the guide Claude follows when it writes apps.
extern const char *const kExampleForClaude;

}  // namespace builtin_apps
