#include "nanox/FileTree.h"

#include "tempdir.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <string>
#include <vector>

using namespace nanox;

namespace {

// Builds a known tree:
//   root/
//   ├── a.txt
//   ├── b/            (empty dir)
//   ├── sub/
//   │   └── x.nx
//   ├── z.nx
//   ├── .hidden.txt   (ignored)
//   └── build/        (ignored)
//       └── skip.nx
nanox_test::TempDir make_tree() {
    nanox_test::TempDir t("nanox_ft");
    t.write_file("a.txt", "a");
    t.make_dir("b");
    t.write_file("sub/x.nx", "let x = 1;\n");
    t.write_file("z.nx", "fn f() {}\n");
    t.write_file(".hidden.txt", "h");
    t.write_file("build/skip.nx", "s");
    return t;
}

}  // namespace

NX_TEST_CASE(scan_orders_directories_first_then_files) {
    const nanox_test::TempDir t = make_tree();
    const FileTree tree = FileTree::scan(t.str());

    // root, b (dir), sub (dir), sub/x.nx, a.txt, z.nx
    NX_CHECK_EQ(tree.nodes().size(), static_cast<std::size_t>(6));

    NX_CHECK(tree.nodes()[0].is_dir);
    NX_CHECK_EQ(tree.nodes()[0].depth, 0);
    NX_CHECK(tree.nodes()[0].expanded);

    NX_CHECK_EQ(tree.nodes()[1].name, std::string("b"));
    NX_CHECK(tree.nodes()[1].is_dir);
    NX_CHECK_EQ(tree.nodes()[1].depth, 1);

    NX_CHECK_EQ(tree.nodes()[2].name, std::string("sub"));
    NX_CHECK(tree.nodes()[2].is_dir);

    NX_CHECK_EQ(tree.nodes()[3].name, std::string("x.nx"));
    NX_CHECK(!tree.nodes()[3].is_dir);
    NX_CHECK_EQ(tree.nodes()[3].depth, 2);

    NX_CHECK_EQ(tree.nodes()[4].name, std::string("a.txt"));
    NX_CHECK(!tree.nodes()[4].is_dir);

    NX_CHECK_EQ(tree.nodes()[5].name, std::string("z.nx"));
    NX_CHECK(!tree.nodes()[5].is_dir);
}

NX_TEST_CASE(ignored_entries_are_skipped) {
    const nanox_test::TempDir t = make_tree();
    const FileTree tree = FileTree::scan(t.str());
    for (const TreeNode& node : tree.nodes()) {
        NX_CHECK(node.name != ".hidden.txt");
        NX_CHECK(node.name != "build");
        NX_CHECK(node.name != "skip.nx");
    }
}

NX_TEST_CASE(visible_with_expanded_root_lists_top_level_only) {
    const nanox_test::TempDir t = make_tree();
    const FileTree tree = FileTree::scan(t.str());
    // Root starts expanded: its direct children (b, sub, a.txt, z.nx) are
    // visible, sub's child x.nx is not.
    const std::vector<std::size_t> vis = tree.visible();
    NX_CHECK_EQ(vis.size(), static_cast<std::size_t>(5));  // root + 4 children
    NX_CHECK_EQ(tree.nodes()[vis[4]].name, std::string("z.nx"));
}

NX_TEST_CASE(collapsing_root_hides_everything_else) {
    nanox_test::TempDir t = make_tree();
    FileTree tree = FileTree::scan(t.str());
    tree.toggle_expanded(0);  // collapse root
    const std::vector<std::size_t> vis = tree.visible();
    NX_CHECK_EQ(vis.size(), static_cast<std::size_t>(1));
    NX_CHECK_EQ(vis[0], static_cast<std::size_t>(0));
}

NX_TEST_CASE(expanding_a_dir_reveals_its_children) {
    nanox_test::TempDir t = make_tree();
    FileTree tree = FileTree::scan(t.str());
    const std::size_t sub_index = 2;  // "sub" node
    NX_CHECK(!tree.nodes()[sub_index].expanded);
    tree.toggle_expanded(sub_index);
    const std::vector<std::size_t> vis = tree.visible();
    NX_CHECK_EQ(vis.size(), static_cast<std::size_t>(6));  // root, b, sub, x.nx, a.txt, z.nx
    NX_CHECK_EQ(tree.nodes()[vis[3]].name, std::string("x.nx"));
}

NX_TEST_CASE(toggle_is_idempotent_for_directories) {
    nanox_test::TempDir t = make_tree();
    FileTree tree = FileTree::scan(t.str());
    tree.toggle_expanded(0);
    tree.toggle_expanded(0);
    NX_CHECK(tree.nodes()[0].expanded);
    NX_CHECK_EQ(tree.visible().size(), static_cast<std::size_t>(5));
}

NX_TEST_CASE(toggle_on_file_is_a_noop) {
    nanox_test::TempDir t = make_tree();
    FileTree tree = FileTree::scan(t.str());
    const std::size_t z_index = 5;  // "z.nx"
    const bool before = tree.nodes()[z_index].expanded;
    tree.toggle_expanded(z_index);
    NX_CHECK_EQ(tree.nodes()[z_index].expanded, before);
}

NX_TEST_CASE(collapsed_parent_hides_grandchildren) {
    nanox_test::TempDir t = make_tree();
    t.write_file("deep/nested/leaf.nx", "x");
    FileTree tree = FileTree::scan(t.str());
    // root expanded, "deep" collapsed by default -> "nested" hidden
    const std::vector<std::size_t> vis = tree.visible();
    for (const std::size_t idx : vis) {
        NX_CHECK(tree.nodes()[idx].name != "nested");
        NX_CHECK(tree.nodes()[idx].name != "leaf.nx");
    }
}

NX_TEST_CASE(scan_of_missing_directory_has_root_only) {
    const FileTree tree = FileTree::scan("nanox_no_such_dir_xyz");
    NX_CHECK_EQ(tree.nodes().size(), static_cast<std::size_t>(1));
    NX_CHECK(tree.nodes()[0].is_dir);
    NX_CHECK_EQ(tree.visible().size(), static_cast<std::size_t>(1));
}

NX_TEST_CASE(is_ignored_entry_rules) {
    NX_CHECK(is_ignored_entry(".git"));
    NX_CHECK(is_ignored_entry(".vscode"));
    NX_CHECK(is_ignored_entry("build"));
    NX_CHECK(is_ignored_entry("out"));
    NX_CHECK(is_ignored_entry("cmake-build-debug"));
    NX_CHECK(!is_ignored_entry("src"));
    NX_CHECK(!is_ignored_entry("main.nx"));
    NX_CHECK(!is_ignored_entry("Build"));  // case-sensitive, not ignored
}
