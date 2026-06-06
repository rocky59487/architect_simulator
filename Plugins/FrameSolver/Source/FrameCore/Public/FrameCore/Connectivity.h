#pragma once
//
// #6 Collapse / connectivity (PFSFv2-to-UE5 §3 port). Union-Find with a REVERSIBLE
// JOURNAL: place operations unite cells; undo rolls back to a saved marker. Plus a
// grounded-mask query (which nodes connect to the ground) used after a block is removed
// to find the pieces that detach.
//
// Discipline (PFSF §3.3): connectivity is a SUPPORTING check, it must NOT pre-empt the
// stress solve. The engineering verdict (collapse) comes from the solver (singular K /
// D-C > 1); connectivity then tells you which detached pieces fall.
//
#include "FrameCore/FrameTypes.h"
#include <vector>
#include <utility>

namespace frame { namespace conn {

// Union-Find by size with NO path compression, so every unite() can be undone. The
// journal records each reparented root; rollback(marker) restores the state that existed
// when marker() was taken.
class FRAMECORE_API RollbackUnionFind {
public:
    explicit RollbackUnionFind(int n);

    int  find(int x) const;                                  // representative (walks up; no compression)
    bool connected(int a, int b) const { return find(a) == find(b); }
    bool unite(int a, int b);                                // true if a real merge happened
    int  marker() const { return static_cast<int>(journal_.size()); }
    void rollback(int marker);                               // undo unites back to the marker
    int  componentCount() const { return comps_; }

private:
    std::vector<int> parent_;
    std::vector<int> size_;
    std::vector<std::pair<int, int>> journal_;   // (reparented root b, its new parent a)
    int comps_ = 0;
};

// Which of the n nodes are connected (through `edges`) to ANY ground node. Rebuilt from
// scratch over the CURRENT edge set, so it is correct after a node/edge removal (pass the
// reduced edge list). O((n + e) * alpha).
FRAMECORE_API std::vector<bool> groundedMask(int n,
                                             const std::vector<std::pair<int, int>>& edges,
                                             const std::vector<int>& groundNodes);

}} // namespace frame::conn
