// RhinoBaker.cs -- writes a LinearResult/ModalResult/DynCollapseResult into Rhino layer tree
// following the S6c § ⑦ convention:
//
//   FrameCore / Model / { Nodes, Members, Shells, Supports }
//   FrameCore / Loads / { Nodal, UDL, Pressure }
//   FrameCore / Results / { Deformed, BMD, SFD, NFD, UtilFringe, Modes / Mode-i }
//
// IDEMPOTENT — calling Bake() twice updates the same layers rather than duplicating; a
// subsequent bake first clears the geometry inside each child layer it intends to write to.
// User-added geometry in those layers is preserved if it isn't of the expected type
// (e.g. a manually placed annotation curve survives a member-curve bake of the same layer).

using System.Collections.Generic;
using System.Drawing;
using Rhino;
using Rhino.DocObjects;
using Rhino.Geometry;

namespace FrameCore.Gh.Common
{
    public static class RhinoBaker
    {
        private const string Root = "FrameCore";

        /// <summary>Ensure a "/"-separated layer path exists; return its layer index.</summary>
        public static int EnsureLayer(RhinoDoc doc, string path, Color? color = null)
        {
            var parts = path.Split('/');
            int parentIdx = -1;
            int idx = -1;
            for (int i = 0; i < parts.Length; ++i)
            {
                var fullPath = string.Join("::", parts, 0, i + 1);
                idx = doc.Layers.FindByFullPath(fullPath, -1);
                if (idx < 0)
                {
                    var layer = new Layer { Name = parts[i], ParentLayerId = System.Guid.Empty };
                    if (parentIdx >= 0) layer.ParentLayerId = doc.Layers[parentIdx].Id;
                    if (color is { } c) layer.Color = c;
                    idx = doc.Layers.Add(layer);
                }
                parentIdx = idx;
            }
            return idx;
        }

        /// <summary>Remove every object on this layer whose object-type matches mask.</summary>
        public static void ClearLayer(RhinoDoc doc, int layerIdx, ObjectType mask)
        {
            var settings = new ObjectEnumeratorSettings {
                LayerIndexFilter = layerIdx,
                ObjectTypeFilter = mask,
                IncludeLights    = false,
                IncludeGrips     = false,
            };
            var stale = new List<System.Guid>();
            foreach (var o in doc.Objects.GetObjectList(settings)) stale.Add(o.Id);
            foreach (var id in stale) doc.Objects.Delete(id, true);
        }

        public static int BakeMembers(RhinoDoc doc, IReadOnlyList<Curve> curves, IReadOnlyList<double>? dcRatios = null)
        {
            var layer = EnsureLayer(doc, $"{Root}::Model::Members");
            ClearLayer(doc, layer, ObjectType.Curve);
            int count = 0;
            for (int i = 0; i < curves.Count; ++i)
            {
                var attr = new ObjectAttributes { LayerIndex = layer };
                if (dcRatios is not null && i < dcRatios.Count)
                    attr.ObjectColor = ColorRamps.Viridis(dcRatios[i]);
                attr.ColorSource = ObjectColorSource.ColorFromObject;
                doc.Objects.AddCurve(curves[i], attr);
                ++count;
            }
            return count;
        }

        public static int BakeDeformed(RhinoDoc doc, Mesh deformed)
        {
            var layer = EnsureLayer(doc, $"{Root}::Results::Deformed");
            ClearLayer(doc, layer, ObjectType.Mesh);
            doc.Objects.AddMesh(deformed, new ObjectAttributes { LayerIndex = layer });
            return 1;
        }

        public static int BakeBmd(RhinoDoc doc, IReadOnlyList<Polyline> polylines, IReadOnlyList<Mesh> filled)
        {
            var lcurve = EnsureLayer(doc, $"{Root}::Results::BMD");
            ClearLayer(doc, lcurve, ObjectType.Curve | ObjectType.Mesh);
            int n = 0;
            foreach (var p in polylines) { doc.Objects.AddPolyline(p, new ObjectAttributes { LayerIndex = lcurve }); ++n; }
            foreach (var m in filled)    { doc.Objects.AddMesh(m,      new ObjectAttributes { LayerIndex = lcurve }); ++n; }
            return n;
        }

        public static int BakeMode(RhinoDoc doc, int modeIdx, Mesh shape)
        {
            var layer = EnsureLayer(doc, $"{Root}::Results::Modes::Mode-{modeIdx + 1}");
            ClearLayer(doc, layer, ObjectType.Mesh);
            doc.Objects.AddMesh(shape, new ObjectAttributes { LayerIndex = layer });
            return 1;
        }

        public static int BakeUtilFringe(RhinoDoc doc, Mesh fringe)
        {
            var layer = EnsureLayer(doc, $"{Root}::Results::UtilFringe");
            ClearLayer(doc, layer, ObjectType.Mesh);
            doc.Objects.AddMesh(fringe, new ObjectAttributes { LayerIndex = layer });
            return 1;
        }
    }
}
