#include "nanox/FileTree.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace nanox {

namespace fs = std::filesystem;

namespace {

// Appends the entries of `dir` (sorted: directories first, then by name) and
// recurses into subdirectories. Ignored entries are skipped entirely.
void scan_dir(const fs::path& dir, std::vector<TreeNode>& nodes, int depth) {
    std::vector<fs::directory_entry> entries;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir, ec)) {
        entries.push_back(entry);
    }
    if (ec) {
        return;  // unreadable directory: show nothing below it
    }

    std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a,
                                                 const fs::directory_entry& b) {
        std::error_code ea;
        std::error_code eb;
        const bool a_dir = a.is_directory(ea);
        const bool b_dir = b.is_directory(eb);
        if (a_dir != b_dir) {
            return a_dir;  // directories first
        }
        return a.path().filename().string() < b.path().filename().string();
    });

    for (const fs::directory_entry& entry : entries) {
        const std::string name = entry.path().filename().string();
        if (is_ignored_entry(name)) {
            continue;
        }

        std::error_code status_ec;
        const bool is_dir = entry.is_directory(status_ec);
        if (status_ec) {
            continue;
        }

        TreeNode node;
        node.name = name;
        node.path = entry.path().string();
        node.is_dir = is_dir;
        node.depth = depth;
        nodes.push_back(node);

        if (is_dir) {
            scan_dir(entry.path(), nodes, depth + 1);
        }
    }
}

}  // namespace

FileTree FileTree::scan(const std::string& root) {
    FileTree tree;
    tree.root_ = root;

    TreeNode root_node;
    const fs::path root_path(root);
    root_node.name = root_path.filename().string();
    if (root_node.name.empty()) {
        root_node.name = root;  // e.g. root is "C:\" or "/"
    }
    root_node.path = root;
    root_node.is_dir = true;
    root_node.expanded = true;
    root_node.depth = 0;
    tree.nodes_.push_back(root_node);

    std::error_code ec;
    if (fs::is_directory(root_path, ec) && !ec) {
        scan_dir(root_path, tree.nodes_, 1);
    }
    return tree;
}

std::vector<std::size_t> FileTree::visible() const {
    std::vector<std::size_t> result;

    // Index of the most recently seen node at each depth (DFS order means
    // that node is the parent of the next node at depth+1).
    std::vector<std::size_t> last_at_depth;

    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        const TreeNode& node = nodes_[i];

        bool is_visible = false;
        if (node.depth == 0) {
            is_visible = true;
        } else if (static_cast<std::size_t>(node.depth - 1) < last_at_depth.size()) {
            const TreeNode& parent = nodes_[last_at_depth[static_cast<std::size_t>(node.depth - 1)]];
            is_visible = parent.is_dir && parent.expanded;
        }

        if (static_cast<std::size_t>(node.depth) < last_at_depth.size()) {
            last_at_depth[static_cast<std::size_t>(node.depth)] = i;
        } else {
            last_at_depth.resize(static_cast<std::size_t>(node.depth) + 1, i);
        }

        if (is_visible) {
            result.push_back(i);
        }
    }
    return result;
}

void FileTree::toggle_expanded(std::size_t index) {
    if (index < nodes_.size() && nodes_[index].is_dir) {
        nodes_[index].expanded = !nodes_[index].expanded;
    }
}

}  // namespace nanox
