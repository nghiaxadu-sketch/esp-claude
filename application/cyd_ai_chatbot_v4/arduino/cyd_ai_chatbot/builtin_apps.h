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

// Samples no longer shipped (dice, counter): deleted from a board only while they are exactly as shipped,
// so an app the user changed with Claude stays. One entry per shipped version.
extern const App kRetired[];
extern const size_t kRetiredCount;

// A compact example included in the guide Claude follows when it writes apps.
extern const char *const kExampleForClaude;

}  // namespace builtin_apps
