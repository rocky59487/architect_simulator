#include "FrameCoreUE/FrameCoreUESectionLibrary.h"
#include "FrameCore/Section.h"

FFrameSection UFrameSectionLibrary::MakeRectangular(float Width, float Depth)
{
    // Delegate to engine factory so analytic values stay bit-identical (this is the F71
    // LibraryPresets test invariant: MakeRectangular(100,100) marshals to the same A/I/J/Z
    // tuple frame::Section::Rectangular(100,100) produces).
    const frame::Section s = frame::Section::Rectangular(static_cast<frame::real>(Width),
                                                         static_cast<frame::real>(Depth));
    FFrameSection out;
    out.A   = static_cast<float>(s.A);
    out.Iy  = static_cast<float>(s.Iy);
    out.Iz  = static_cast<float>(s.Iz);
    out.J   = static_cast<float>(s.J);
    out.Cy  = static_cast<float>(s.cy);
    out.Cz  = static_cast<float>(s.cz);
    out.Asy = static_cast<float>(s.Asy);
    out.Asz = static_cast<float>(s.Asz);
    out.Zy  = static_cast<float>(s.Zy);
    out.Zz  = static_cast<float>(s.Zz);
    out.Shape = EFrameSectionShape::Rectangular;
    return out;
}

FFrameSection UFrameSectionLibrary::MakeCircular(float Diameter)
{
    const float r = 0.5f * Diameter;
    const frame::Section s = frame::Section::Circular(static_cast<frame::real>(r));
    FFrameSection out;
    out.A   = static_cast<float>(s.A);
    out.Iy  = static_cast<float>(s.Iy);
    out.Iz  = static_cast<float>(s.Iz);
    out.J   = static_cast<float>(s.J);
    out.Cy  = static_cast<float>(s.cy);
    out.Cz  = static_cast<float>(s.cz);
    out.Asy = static_cast<float>(s.Asy);
    out.Asz = static_cast<float>(s.Asz);
    out.Zy  = static_cast<float>(s.Zy);
    out.Zz  = static_cast<float>(s.Zz);
    out.Shape = EFrameSectionShape::Circular;
    return out;
}
