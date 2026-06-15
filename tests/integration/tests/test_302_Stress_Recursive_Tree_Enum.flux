struct TreeNode {
    value: Double,
    left_empty: Double,
    right_empty: Double,
    left_val: Double,
    right_val: Double,
    left_left: Double,
    left_right: Double,
    right_left: Double,
    right_right: Double
}
def make_leaf(v: Double) -> TreeNode {
    TreeNode { value: v, left_empty: 1.0, right_empty: 1.0, left_val: 0.0, right_val: 0.0, left_left: 0.0, left_right: 0.0, right_left: 0.0, right_right: 0.0 }
}
def make_node(v: Double, lv: Double, rv: Double) -> TreeNode {
    TreeNode { value: v, left_empty: 0.0, right_empty: 0.0, left_val: lv, right_val: rv, left_left: 0.0, left_right: 0.0, right_left: 0.0, right_right: 0.0 }
}
def tree_sum(t: TreeNode) -> Double {
    let left_sum = if t.left_empty == 1.0 then 0.0 else t.left_val
    let right_sum = if t.right_empty == 1.0 then 0.0 else t.right_val
    t.value + left_sum + right_sum
}
def tree_depth(t: TreeNode) -> Double {
    let left_d = if t.left_empty == 1.0 then 0.0 else 1.0
    let right_d = if t.right_empty == 1.0 then 0.0 else 1.0
    if left_d > right_d then left_d + 1.0 else right_d + 1.0
}
def main() -> Double {
    let leaf = make_leaf(42.0)
    assert(tree_sum(leaf) == 42.0, "leaf sum")
    assert(tree_depth(leaf) == 1.0, "leaf depth")
    let node = make_node(10.0, 20.0, 30.0)
    assert(tree_sum(node) == 60.0, "node sum")
    assert(tree_depth(node) == 2.0, "node depth")
    let deep = make_node(1.0, 2.0, 3.0)
    assert(tree_sum(deep) == 6.0, "deep sum")
    var total = 0.0
    var i = 0.0
    while i < 100.0 do {
        let t = make_leaf(i)
        total = total + tree_sum(t)
        i = i + 1.0
    }
    assert(total == 4950.0, "tree loop wrong")
    1.0
}
main()
