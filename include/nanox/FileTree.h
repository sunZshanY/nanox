#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nanox {

// Returns true for directory/file names that the project tooling should not
// show or scan: dot-files/dot-directories and common build output folders.
inline bool is_ignored_entry(const std::string& name) {
    if (name.empty() || name[0] == '.') {
        return true;
    }
    if (name == "build" || name == "out") {
        return true;
    }
    return name.rfind("cmake-build", 0) == 0;
}

// One node of the project file tree.
struct TreeNode {
    std::string name;   // last path component only
    std::string path;   // full path (used to open the file)
    bool is_dir = false;
    bool expanded = false;  // directories only
    int depth = 0;          // 0 = workspace root
};

// A browsable, mutable file tree of the workspace.
//
// Design note: the tree is stored as a FLAT vector in depth-first order
// (parent before children). Expand/collapse state lives on the directory
// nodes; visible() computes the list of currently shown nodes from that
// state. This keeps the model independent of the TUI and fully unit-testable.
class FileTree {
public:
    // Scans `root` recursively. If `root` does not exist or is not a
    // directory, the tree contains only the root node itself.
    static FileTree scan(const std::string& root);

    const std::string& root() const { return root_; }
    const std::vector<TreeNode>& nodes() const { return nodes_; }

    // Indices (into nodes()) of all nodes visible with the current
    // expand/collapse state, in display order.
    std::vector<std::size_t> visible() const;

    // Flips the expanded flag of a directory node; no-op for files.
    void toggle_expanded(std::size_t index);

private:
    std::string root_;
    std::vector<TreeNode> nodes_;
};

}  // namespace nanox
