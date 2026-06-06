// M7 #6 collapse / connectivity (mirrors standalone F14): grounded-mask after an edge
// removal detaches the disconnected piece, and the reversible journal rolls back unites
// to a saved marker (so place/undo is bit-consistent).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/Connectivity.h"

#include <vector>
#include <utility>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreConnectivityTest,
	"FrameCore.Connectivity.GroundedRollback",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FFrameCoreConnectivityTest::RunTest(const FString&)
{
	using namespace frame::conn;

	const std::vector<int> ground = { 0 };
	const std::vector<std::pair<int, int>> full = { {0,1},{1,2},{2,3},{3,4} };
	const std::vector<bool> g0 = groundedMask(5, full, ground);
	TestTrue(TEXT("full chain all grounded"), g0[0] && g0[1] && g0[2] && g0[3] && g0[4]);

	// remove the (2,3) link -> {3,4} detach from ground
	const std::vector<std::pair<int, int>> cut = { {0,1},{1,2},{3,4} };
	const std::vector<bool> g1 = groundedMask(5, cut, ground);
	TestTrue(TEXT("cut: 0,1,2 grounded"), g1[0] && g1[1] && g1[2]);
	TestTrue(TEXT("cut: 3,4 detached"), !g1[3] && !g1[4]);

	RollbackUnionFind uf(5);
	uf.unite(0, 1); uf.unite(1, 2);
	const int mk = uf.marker();
	uf.unite(2, 3); uf.unite(3, 4);
	TestTrue(TEXT("after unites 0~4 connected"), uf.connected(0, 4));
	uf.rollback(mk);
	TestTrue(TEXT("rollback: 0~2 still connected"), uf.connected(0, 2));
	TestTrue(TEXT("rollback: 0 !~ 3 (undone)"), !uf.connected(0, 3));
	TestTrue(TEXT("rollback: 2 !~ 3 (undone)"), !uf.connected(2, 3));
	TestEqual(TEXT("rollback restores component count"), uf.componentCount(), 3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
