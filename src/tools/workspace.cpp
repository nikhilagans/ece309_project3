#include "tools/workspace.h"

#include <string>

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path,
                        fs::path* out,
                        std::string* reason) const {

    // Reject an empty path.
    if (user_path.empty()) {
        *reason = "empty path";
        return false;
    }

    // Reject paths containing a null character.
    if (user_path.find('\0') != std::string_view::npos) {
        *reason = "path contains null character";
        return false;
    }

    // Convert the string into a filesystem path.
    fs::path path{std::string(user_path)};

    // Reject absolute paths.
    if (path.is_absolute() || path.has_root_path()) {
        *reason = "absolute paths are not allowed";
        return false;
    }

    // Simplify components like "." and "..".
    fs::path normalized = path.lexically_normal();

    // Reject paths that escape the workspace.
    if (!normalized.empty() &&
        *normalized.begin() == fs::path("..")) {
        *reason = "path escapes workspace";
        return false;
    }

    // Combine the safe relative path with the workspace root.
    *out = root() / normalized;

    reason->clear();
    return true;
}