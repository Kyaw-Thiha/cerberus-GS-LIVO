#pragma once

#include "point_cloud.cuh"

#include <filesystem>
#include <vector>

namespace gs_map_io {

// Writes a 3D Gaussian Splat map to a binary little-endian PLY file.
//
// Layout per GS_point (from point_cloud.cuh:89-111, packed):
//   Point(3) + Normal(3) + Distance(3) + Quaternions(4) + Color(3) +
//   opacity(1) + index(1) + flag_in_fov(1) = 19 floats
//
// Output PLY per vertex (canonical 3DGS schema, matches the original
// inria gaussian-splatting writer so downstream viewers -- rerun,
// gsplat, the 3DGS reference impl, SplaTAM, MonoGS -- all parse it
// without manual schema translation):
//   xyz(3) + normal(3) + f_dc(3) + f_rest_0..44(45) + opacity(1) +
//   scale_0..2(3) + rot_0..3(4) = 62 floats
//
// Encoding choices (and why):
// - f_dc = (RGB - 0.5) / SH_C0 with SH_C0 = 0.28209479177387814. RGB
//   is recovered via SH2RGB(f_dc) = f_dc * SH_C0 + 0.5, so the
//   round-trip is lossless within float32 precision. This matches
//   the 3DGS reference impl's convention.
// - f_rest_0..44 = 0 (no higher-order SH coefficients -- GS-LIVO's
//   training loop only optimizes SH degree 0, per the paper §II.B.2).
// - opacity = inverse_sigmoid(_opacity), the inverse of the
//   sigmoid used when reading _opacity back into the optimizer's
//   unconstrained _opacity tensor.
// - scale_0..2 = _distance.{r1,r2,r3}. Note: _distance stores
//   exp(_scaling), not the log-scale itself. We write the actual
//   scale directly -- this is what the original 3DGS writer does
//   (scale = exp(_scaling)) and what every downstream viewer
//   expects. lib3dgs/src/gaussian.cu::Save_ply_our2 actually takes
//   log(_distance) here, which is wrong (it writes the log-scale
//   to a PLY schema that wants the actual scale). This function
//   deliberately diverges to fix that.
// - rot_0..3 = _quaternion.{qw,qx,qy,qz}. Note: lib3dgs's existing
//   Save_ply_our2 reads the wrong column indices here too (it uses
//   narrow(1, 9, 4) on a tensor whose columns 9-12 span the last
//   float of _distance plus the first three of _quaternion). This
//   function uses the actual _quaternion fields and so gets the
//   rotation even when the existing writers would not.
//
// Async-friendly: this function does no shared-state access -- the
// caller is responsible for passing a const-ref std::vector<GS_point>
// snapshot taken under whatever lock guards the live data
// (VIOManager::gs_map_mutex_ in this fork). See LIVMapper.cpp for
// the snapshot helper that pairs with this writer.
void dump_gs_map_to_ply(const std::filesystem::path& file_path,
                       const std::vector<GS_point>& gaussian_cloud);

}  // namespace gs_map_io
