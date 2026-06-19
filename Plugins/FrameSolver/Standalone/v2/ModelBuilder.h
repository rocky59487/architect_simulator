// ModelBuilder.h -- shared JSON -> frame::FrameModel converter for v2 dispatcher handlers.
//
// Mirror of frame_cli_core.cpp::buildModel but reads from MiniJson instead of a text protocol.
// Inline so the v2 DLL can pick it up without a separate translation unit. Header-only keeps
// it co-located with the dispatcher TU and avoids touching build_capi_v2.bat's source list.
//
// SCHEMA (v2 model.set body)
//   {
//     "materials":      [ { "E", "G", "rho"?, "nu"?, "fy"?,
//                           "cap"? { "comp", "tens", "shear" } }, ... ],
//     "sections":       [ { "A", "Iy", "Iz", "J", "cy", "cz",
//                           "Asy"?, "Asz"?, "Zy"?, "Zz"?,
//                           "shape"? "rectangular" | "circular" }, ... ],
//     "nodes":          [ { "id", "x", "y", "z",
//                           "fixed"?     [bool x6],
//                           "prescribed"? [real x6] }, ... ],
//     "members":        [ { "id", "i", "j", "mat", "sec",
//                           "ref"?         [real x3],   // refVec; default {0,0,1}
//                           "tensionOnly"? bool,
//                           "active"?      bool,
//                           "release"?     [bool x12] }, ... ],
//     "shells":         [ { "id", "nodes" [int x4], "mat", "t", "active"? bool }, ... ],
//     "nodalLoads":     [ { "node", "comp" [real x6] }, ... ],
//     "memberUDLs":     [ { "member", "w" [real x3] }, ... ],   // local coords
//     "shellPressures": [ { "shell", "p" }, ... ],
//     "hinges":         [ { "member", "dof", "Mp" }, ... ]
//   }
//
// Returns false + fills `err` on malformed input. The dispatcher then sends a
// VALIDATION_FAILED error frame back to the client. structural sanity (positive areas, ids
// resolve etc.) is left to frame::FrameModel::validate(), which the caller drives after the
// build.

#pragma once

#include "MiniJson.h"

#include "FrameCore/FrameModel.h"
#include "FrameCore/Material.h"
#include "FrameCore/Section.h"
#include "FrameCore/Member.h"
#include "FrameCore/Node.h"
#include "FrameCore/Shell.h"
#include "FrameCore/Load.h"
#include "FrameCore/Hinge.h"

#include <string>

namespace frame_v2 {

namespace detail {

inline bool readReals(const Json* arr, std::size_t expectedLen, std::vector<double>& out,
                      std::string& err, const char* what) {
    if (!arr) return true;     // optional
    if (!arr->isArray()) { err = std::string(what) + " must be an array"; return false; }
    const auto& a = arr->asArray();
    if (a.size() != expectedLen) {
        err = std::string(what) + " must have " + std::to_string(expectedLen) + " entries";
        return false;
    }
    out.clear(); out.reserve(expectedLen);
    for (const auto& e : a) {
        if (!e.isNumber()) { err = std::string(what) + " entries must be numbers"; return false; }
        out.push_back(e.asDouble());
    }
    return true;
}

inline bool readBools(const Json* arr, std::size_t expectedLen, std::vector<bool>& out,
                      std::string& err, const char* what) {
    if (!arr) return true;
    if (!arr->isArray()) { err = std::string(what) + " must be an array"; return false; }
    const auto& a = arr->asArray();
    if (a.size() != expectedLen) {
        err = std::string(what) + " must have " + std::to_string(expectedLen) + " entries";
        return false;
    }
    out.clear(); out.reserve(expectedLen);
    for (const auto& e : a) {
        if (e.isBool())        out.push_back(e.asBool());
        else if (e.isNumber()) out.push_back(e.asInt() != 0);
        else { err = std::string(what) + " entries must be bool or 0/1"; return false; }
    }
    return true;
}

inline bool readInts(const Json* arr, std::size_t expectedLen, std::vector<int>& out,
                     std::string& err, const char* what) {
    if (!arr) return true;
    if (!arr->isArray()) { err = std::string(what) + " must be an array"; return false; }
    const auto& a = arr->asArray();
    if (a.size() != expectedLen) {
        err = std::string(what) + " must have " + std::to_string(expectedLen) + " entries";
        return false;
    }
    out.clear(); out.reserve(expectedLen);
    for (const auto& e : a) {
        if (!e.isNumber()) { err = std::string(what) + " entries must be numbers"; return false; }
        out.push_back(static_cast<int>(e.asInt()));
    }
    return true;
}

}  // namespace detail

/// Build a frame::FrameModel from a parsed model.set body. Returns true on success; on failure
/// returns false and writes a one-line reason into `err`. Does NOT call FrameModel::validate()
/// -- the caller drives that so the error path can distinguish a malformed JSON shape from a
/// structurally invalid (but well-formed) model.
inline bool buildModelFromJson(const Json& body, frame::FrameModel& out, std::string& err) {
    using namespace frame;

    // ---- materials ----
    if (const Json* mats = body.get("materials")) {
        if (!mats->isArray()) { err = "materials must be an array"; return false; }
        out.materials.reserve(mats->asArray().size());
        for (std::size_t k = 0; k < mats->asArray().size(); ++k) {
            const Json& m = mats->asArray()[k];
            if (!m.isObject()) { err = "materials[" + std::to_string(k) + "] must be an object"; return false; }
            const double E   = m.getDouble("E", 0.0);
            const double G   = m.getDouble("G", 0.0);
            const double rho = m.getDouble("rho", 0.0);
            Material fm(static_cast<real>(E), static_cast<real>(G), static_cast<real>(rho));
            fm.nu = static_cast<real>(m.getDouble("nu", 0.0));
            fm.fy = static_cast<real>(m.getDouble("fy", 0.0));
            if (const Json* cap = m.get("cap"); cap && cap->isObject()) {
                const double cC = cap->getDouble("comp", 0.0);
                const double cT = cap->getDouble("tens", cC);
                const double cS = cap->getDouble("shear", 0.6 * cC);
                fm.cap = Capacity::make(static_cast<real>(cC), static_cast<real>(cT), static_cast<real>(cS));
            } else {
                // mirror frame_cli_core: no cap -> default (300/300/180) so D/C screen has a value.
                fm.cap = Capacity::make(real(300), real(300), real(180));
            }
            out.materials.push_back(fm);
        }
    }

    // ---- sections ----
    if (const Json* secs = body.get("sections")) {
        if (!secs->isArray()) { err = "sections must be an array"; return false; }
        out.sections.reserve(secs->asArray().size());
        for (std::size_t k = 0; k < secs->asArray().size(); ++k) {
            const Json& s = secs->asArray()[k];
            if (!s.isObject()) { err = "sections[" + std::to_string(k) + "] must be an object"; return false; }
            Section fs;
            fs.A  = static_cast<real>(s.getDouble("A", 0.0));
            fs.Iy = static_cast<real>(s.getDouble("Iy", 0.0));
            fs.Iz = static_cast<real>(s.getDouble("Iz", 0.0));
            fs.J  = static_cast<real>(s.getDouble("J", 0.0));
            fs.cy = static_cast<real>(s.getDouble("cy", 0.0));
            fs.cz = static_cast<real>(s.getDouble("cz", 0.0));
            fs.Asy = static_cast<real>(s.getDouble("Asy", 0.0));
            fs.Asz = static_cast<real>(s.getDouble("Asz", 0.0));
            fs.Zy  = static_cast<real>(s.getDouble("Zy", 0.0));
            fs.Zz  = static_cast<real>(s.getDouble("Zz", 0.0));
            const std::string shape = s.getString("shape", "rectangular");
            fs.shape = (shape == "circular") ? Section::Shape::Circular : Section::Shape::Rectangular;
            out.sections.push_back(fs);
        }
    }

    // ---- nodes ----
    if (const Json* nodes = body.get("nodes")) {
        if (!nodes->isArray()) { err = "nodes must be an array"; return false; }
        out.nodes.reserve(nodes->asArray().size());
        for (std::size_t k = 0; k < nodes->asArray().size(); ++k) {
            const Json& n = nodes->asArray()[k];
            if (!n.isObject()) { err = "nodes[" + std::to_string(k) + "] must be an object"; return false; }
            const NodeId id = static_cast<NodeId>(n.getInt("id", 0));
            Node fn(id,
                    static_cast<real>(n.getDouble("x", 0.0)),
                    static_cast<real>(n.getDouble("y", 0.0)),
                    static_cast<real>(n.getDouble("z", 0.0)));
            std::vector<bool> fixed;
            if (!detail::readBools(n.get("fixed"), 6, fixed, err, "nodes[].fixed")) return false;
            for (std::size_t d = 0; d < fixed.size(); ++d) fn.fixed[d] = fixed[d];
            std::vector<double> prescribed;
            if (!detail::readReals(n.get("prescribed"), 6, prescribed, err, "nodes[].prescribed")) return false;
            for (std::size_t d = 0; d < prescribed.size(); ++d) fn.prescribed[d] = static_cast<real>(prescribed[d]);
            out.nodes.push_back(fn);
        }
    }

    // ---- members ----
    if (const Json* mems = body.get("members")) {
        if (!mems->isArray()) { err = "members must be an array"; return false; }
        out.members.reserve(mems->asArray().size());
        for (std::size_t k = 0; k < mems->asArray().size(); ++k) {
            const Json& m = mems->asArray()[k];
            if (!m.isObject()) { err = "members[" + std::to_string(k) + "] must be an object"; return false; }
            Member fm(static_cast<MemberId>(m.getInt("id", 0)),
                      static_cast<NodeId>(m.getInt("i", 0)),
                      static_cast<NodeId>(m.getInt("j", 0)),
                      static_cast<int>(m.getInt("mat", -1)),
                      static_cast<int>(m.getInt("sec", -1)));
            std::vector<double> refv;
            if (!detail::readReals(m.get("ref"), 3, refv, err, "members[].ref")) return false;
            if (refv.size() == 3)
                fm.refVec = Vec3(static_cast<real>(refv[0]),
                                 static_cast<real>(refv[1]),
                                 static_cast<real>(refv[2]));
            fm.tensionOnly = m.getBool("tensionOnly", false);
            fm.active      = m.getBool("active", true);
            std::vector<bool> rel;
            if (!detail::readBools(m.get("release"), 12, rel, err, "members[].release")) return false;
            for (std::size_t d = 0; d < rel.size(); ++d) fm.release[d] = rel[d];
            out.members.push_back(fm);
        }
    }

    // ---- shells ----
    if (const Json* shells = body.get("shells")) {
        if (!shells->isArray()) { err = "shells must be an array"; return false; }
        out.shells.reserve(shells->asArray().size());
        for (std::size_t k = 0; k < shells->asArray().size(); ++k) {
            const Json& s = shells->asArray()[k];
            if (!s.isObject()) { err = "shells[" + std::to_string(k) + "] must be an object"; return false; }
            std::vector<int> nids;
            if (!detail::readInts(s.get("nodes"), 4, nids, err, "shells[].nodes")) return false;
            if (nids.size() != 4) { err = "shells[].nodes must have 4 entries"; return false; }
            ShellQuad sq(static_cast<int>(s.getInt("id", 0)),
                          static_cast<NodeId>(nids[0]), static_cast<NodeId>(nids[1]),
                          static_cast<NodeId>(nids[2]), static_cast<NodeId>(nids[3]),
                          static_cast<int>(s.getInt("mat", -1)),
                          static_cast<real>(s.getDouble("t", 0.0)));
            sq.active = s.getBool("active", true);
            out.shells.push_back(sq);
        }
    }

    // ---- nodalLoads ----
    if (const Json* nl = body.get("nodalLoads")) {
        if (!nl->isArray()) { err = "nodalLoads must be an array"; return false; }
        for (std::size_t k = 0; k < nl->asArray().size(); ++k) {
            const Json& l = nl->asArray()[k];
            if (!l.isObject()) { err = "nodalLoads[" + std::to_string(k) + "] must be an object"; return false; }
            NodalLoad load;
            load.node = static_cast<NodeId>(l.getInt("node", 0));
            std::vector<double> comp;
            if (!detail::readReals(l.get("comp"), 6, comp, err, "nodalLoads[].comp")) return false;
            for (std::size_t d = 0; d < comp.size(); ++d) load.comp[d] = static_cast<real>(comp[d]);
            out.nodalLoads.push_back(load);
        }
    }

    // ---- memberUDLs ----
    if (const Json* udls = body.get("memberUDLs")) {
        if (!udls->isArray()) { err = "memberUDLs must be an array"; return false; }
        for (std::size_t k = 0; k < udls->asArray().size(); ++k) {
            const Json& u = udls->asArray()[k];
            if (!u.isObject()) { err = "memberUDLs[" + std::to_string(k) + "] must be an object"; return false; }
            MemberUDL mu;
            mu.member = static_cast<MemberId>(u.getInt("member", 0));
            std::vector<double> w;
            if (!detail::readReals(u.get("w"), 3, w, err, "memberUDLs[].w")) return false;
            if (w.size() == 3)
                mu.w_local = Vec3(static_cast<real>(w[0]),
                                  static_cast<real>(w[1]),
                                  static_cast<real>(w[2]));
            out.memberUDLs.push_back(mu);
        }
    }

    // ---- shellPressures ----
    if (const Json* sps = body.get("shellPressures")) {
        if (!sps->isArray()) { err = "shellPressures must be an array"; return false; }
        for (std::size_t k = 0; k < sps->asArray().size(); ++k) {
            const Json& s = sps->asArray()[k];
            if (!s.isObject()) { err = "shellPressures[" + std::to_string(k) + "] must be an object"; return false; }
            ShellPressure sp;
            sp.shell = static_cast<int>(s.getInt("shell", 0));
            sp.p     = static_cast<real>(s.getDouble("p", 0.0));
            out.shellPressures.push_back(sp);
        }
    }

    // ---- hinges ----
    if (const Json* hgs = body.get("hinges")) {
        if (!hgs->isArray()) { err = "hinges must be an array"; return false; }
        for (std::size_t k = 0; k < hgs->asArray().size(); ++k) {
            const Json& h = hgs->asArray()[k];
            if (!h.isObject()) { err = "hinges[" + std::to_string(k) + "] must be an object"; return false; }
            PlasticHinge ph{
                static_cast<MemberId>(h.getInt("member", 0)),
                static_cast<int>(h.getInt("dof", 0)),
                static_cast<real>(h.getDouble("Mp", 0.0))
            };
            out.hinges.push_back(ph);
        }
    }

    return true;
}

}  // namespace frame_v2
