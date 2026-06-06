#include "FrameCore/Node.h"
// Node is a header-only POD; this TU exists to match the build file list and to
// assert the header compiles standalone.
namespace frame { static_assert(sizeof(Node) > 0, "Node must be a complete type"); }
