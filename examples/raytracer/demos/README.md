# 3D Raytracer (FluxScript)

A minimal 3D raytracer written in FluxScript, demonstrating struct types, module imports, Phong shading, recursive reflections, and PPM output.

## Files

| File | Purpose |
|------|---------|
| `veclib.flux` | `Vec3` struct + 9 vector operations (add, sub, scale, dot, length, normalize, cross, reflect) |
| `scene.flux` | Scene/render functions: `ray_sphere`, `closest_hit`, `shade`, `build_camera`, `build_scene` |
| `run.flux` | Entry point — 500×375 render loop, outputs PPM to stdout |

## Running

```bash
cd examples/raytracer/demos
rm -rf ~/.cache/fluxscript
../../build/flux --cache=0 run.flux > output.ppm
```

## Scene

- **Camera**: eye at (0, 0.8, -1), look_at (0, 0, -3), 60° FOV
- **Light**: normalized direction (0, 4, 3)
- **7 spheres** with varying positions, radii, and colors
- **Shading**: ambient + diffuse + Blinn-Phong specular
- **Reflections**: recursive up to 3 bounces
- **Shadows**: shadow feelers toward light source
- **Sky**: gradient background based on ray direction Y component

## Module Structure

`run.flux` imports `veclib` (Vec3 operations) and `scene` (renderer). `scene.flux` imports `veclib`. All Vec3 structs are passed directly as function parameters (supported since BUG #7 fix).
