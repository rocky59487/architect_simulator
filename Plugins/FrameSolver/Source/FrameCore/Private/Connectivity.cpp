#include "FrameCore/Connectivity.h"
#include <algorithm>

namespace frame { namespace conn {

RollbackUnionFind::RollbackUnionFind(int n) : parent_(std::max(0, n)), size_(std::max(0, n), 1), comps_(std::max(0, n)) {
    for (int i = 0; i < n; ++i) parent_[i] = i;
}

int RollbackUnionFind::find(int x) const {
    while (parent_[x] != x) x = parent_[x];   // no path compression -> rollback-safe
    return x;
}

bool RollbackUnionFind::unite(int a, int b) {
    a = find(a); b = find(b);
    if (a == b) return false;
    if (size_[a] < size_[b]) std::swap(a, b);   // a = the larger root
    journal_.push_back({ b, a });               // record the reparented root
    parent_[b] = a;
    size_[a] += size_[b];
    --comps_;
    return true;
}

void RollbackUnionFind::rollback(int marker) {
    if (marker < 0) marker = 0;
    while (static_cast<int>(journal_.size()) > marker) {
        const std::pair<int, int> rec = journal_.back();
        journal_.pop_back();
        const int b = rec.first, a = rec.second;
        size_[a] -= size_[b];
        parent_[b] = b;
        ++comps_;
    }
}

std::vector<bool> groundedMask(int n,
                               const std::vector<std::pair<int, int>>& edges,
                               const std::vector<int>& groundNodes) {
    RollbackUnionFind uf(n);
    for (const auto& e : edges)
        if (e.first >= 0 && e.first < n && e.second >= 0 && e.second < n)
            uf.unite(e.first, e.second);

    std::vector<bool> groundedRoot(static_cast<size_t>(std::max(0, n)), false);
    for (int g : groundNodes)
        if (g >= 0 && g < n) groundedRoot[static_cast<size_t>(uf.find(g))] = true;

    std::vector<bool> out(static_cast<size_t>(std::max(0, n)), false);
    for (int i = 0; i < n; ++i) out[static_cast<size_t>(i)] = groundedRoot[static_cast<size_t>(uf.find(i))];
    return out;
}

}} // namespace frame::conn
