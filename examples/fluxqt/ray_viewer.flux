# Ray Viewer — raytraced scene rendered in a Qt window
# Self-contained (veclib + scene inlined)

struct Vec3 { x: Double, y: Double, z: Double }

def vec3_new(x, y, z) -> Vec3 { x: x, y: y, z: z }
def vec3_add(a: Vec3, b: Vec3) -> Vec3 { x: a.x + b.x, y: a.y + b.y, z: a.z + b.z }
def vec3_sub(a: Vec3, b: Vec3) -> Vec3 { x: a.x - b.x, y: a.y - b.y, z: a.z - b.z }
def vec3_scale(v: Vec3, s) -> Vec3 { x: v.x * s, y: v.y * s, z: v.z * s }
def vec3_dot(a: Vec3, b: Vec3) -> Double a.x*b.x + a.y*b.y + a.z*b.z
def vec3_length(v: Vec3) -> Double sqrt(v.x*v.x + v.y*v.y + v.z*v.z)

def vec3_normalize(v: Vec3) -> Vec3 {
  let len = sqrt(v.x*v.x + v.y*v.y + v.z*v.z)
  Vec3 { x: v.x / len, y: v.y / len, z: v.z / len }
}

def vec3_cross(a: Vec3, b: Vec3) -> Vec3 {
  x: a.y * b.z - a.z * b.y,
  y: a.z * b.x - a.x * b.z,
  z: a.x * b.y - a.y * b.x
}

def vec3_reflect(v: Vec3, n: Vec3) -> Vec3 {
  let dot = 2.0 * vec3_dot(v, n)
  Vec3 { x: v.x - dot * n.x, y: v.y - dot * n.y, z: v.z - dot * n.z }
}

def clamp(x) {
  if (x < 0.0) { 0.0 } else if (x > 1.0) { 1.0 } else { x }
}

def ray_sphere(origin: Vec3, dir: Vec3, center: Vec3, radius) -> Double {
  let oc = vec3_sub(origin, center)
  let a = vec3_dot(dir, dir)
  let b = 2.0 * vec3_dot(oc, dir)
  let c = vec3_dot(oc, oc) - radius * radius
  let disc = b * b - 4.0 * a * c
  if (disc <= 0.0) { -1.0 }
  else {
    let sd = sqrt(disc)
    let t0 = (-b - sd) / (2.0 * a)
    if (t0 > 0.001) { t0 }
    else {
      let t1 = (-b + sd) / (2.0 * a)
      if (t1 > 0.001) { t1 } else { -1.0 }
    }
  }
}

def build_scene() {
  var s = matrix_zeros(7.0, 8.0)
  matrix_set(s, 0.0, 0.0, -0.9);  matrix_set(s, 0.0, 1.0, 0.2)
  matrix_set(s, 0.0, 2.0, -3.0);  matrix_set(s, 0.0, 3.0, 0.7)
  matrix_set(s, 0.0, 4.0, 1.0);   matrix_set(s, 0.0, 5.0, 0.2)
  matrix_set(s, 0.0, 6.0, 0.2);   matrix_set(s, 0.0, 7.0, 0.3)
  matrix_set(s, 1.0, 0.0, 1.2);   matrix_set(s, 1.0, 1.0, -0.1)
  matrix_set(s, 1.0, 2.0, -3.5);  matrix_set(s, 1.0, 3.0, 0.8)
  matrix_set(s, 1.0, 4.0, 0.2);   matrix_set(s, 1.0, 5.0, 1.0)
  matrix_set(s, 1.0, 6.0, 0.2);   matrix_set(s, 1.0, 7.0, 0.6)
  matrix_set(s, 2.0, 0.0, 0.1);   matrix_set(s, 2.0, 1.0, -0.3)
  matrix_set(s, 2.0, 2.0, -2.5);  matrix_set(s, 2.0, 3.0, 0.4)
  matrix_set(s, 2.0, 4.0, 0.2);   matrix_set(s, 2.0, 5.0, 0.4)
  matrix_set(s, 2.0, 6.0, 1.0);   matrix_set(s, 2.0, 7.0, 0.3)
  matrix_set(s, 3.0, 0.0, 0.0);   matrix_set(s, 3.0, 1.0, 1.2)
  matrix_set(s, 3.0, 2.0, -4.0);  matrix_set(s, 3.0, 3.0, 0.6)
  matrix_set(s, 3.0, 4.0, 1.0);   matrix_set(s, 3.0, 5.0, 0.9)
  matrix_set(s, 3.0, 6.0, 0.1);   matrix_set(s, 3.0, 7.0, 0.0)
  matrix_set(s, 4.0, 0.0, -0.5);  matrix_set(s, 4.0, 1.0, 0.5)
  matrix_set(s, 4.0, 2.0, -2.0);  matrix_set(s, 4.0, 3.0, 0.35)
  matrix_set(s, 4.0, 4.0, 1.0);   matrix_set(s, 4.0, 5.0, 0.2)
  matrix_set(s, 4.0, 6.0, 0.6);   matrix_set(s, 4.0, 7.0, 0.7)
  matrix_set(s, 5.0, 0.0, 0.9);   matrix_set(s, 5.0, 1.0, -0.6)
  matrix_set(s, 5.0, 2.0, -2.2);  matrix_set(s, 5.0, 3.0, 0.4)
  matrix_set(s, 5.0, 4.0, 0.2);   matrix_set(s, 5.0, 5.0, 0.8)
  matrix_set(s, 5.0, 6.0, 0.8);   matrix_set(s, 5.0, 7.0, 0.4)
  matrix_set(s, 6.0, 0.0, 0.0);   matrix_set(s, 6.0, 1.0, -50.0)
  matrix_set(s, 6.0, 2.0, -3.0);  matrix_set(s, 6.0, 3.0, 49.5)
  matrix_set(s, 6.0, 4.0, 0.4);   matrix_set(s, 6.0, 5.0, 0.4)
  matrix_set(s, 6.0, 6.0, 0.4);   matrix_set(s, 6.0, 7.0, 0.1)
  s
}

def closest_hit(origin: Vec3, dir: Vec3, spheres, hit_out) -> Double {
  var ns = matrix_rows(spheres)
  var best_t = 1e30; var best_i = -1.0
  var si = 0.0
  while (si < ns) {
    let cx = matrix_get(spheres, si, 0.0); let cy = matrix_get(spheres, si, 1.0)
    let cz = matrix_get(spheres, si, 2.0); let sr = matrix_get(spheres, si, 3.0)
    let center = vec3_new(cx, cy, cz)
    let t = ray_sphere(origin, dir, center, sr)
    if (t > 0.0 && t < best_t) { best_t = t; best_i = si }
    si = si + 1.0
  }
  if (best_i < 0.0) { -1.0 }
  else {
    let hx = origin.x + best_t * dir.x; let hy = origin.y + best_t * dir.y; let hz = origin.z + best_t * dir.z
    let sx = matrix_get(spheres, best_i, 0.0); let sy = matrix_get(spheres, best_i, 1.0)
    let sz = matrix_get(spheres, best_i, 2.0)
    let nx = hx - sx; let ny = hy - sy; let nz = hz - sz
    let nlen = sqrt(nx*nx + ny*ny + nz*nz)
    matrix_set(hit_out, 0.0, 0.0, best_t); matrix_set(hit_out, 1.0, 0.0, best_i)
    matrix_set(hit_out, 2.0, 0.0, hx); matrix_set(hit_out, 3.0, 0.0, hy); matrix_set(hit_out, 4.0, 0.0, hz)
    matrix_set(hit_out, 5.0, 0.0, nx / nlen); matrix_set(hit_out, 6.0, 0.0, ny / nlen); matrix_set(hit_out, 7.0, 0.0, nz / nlen)
    best_t
  }
}

def shade(origin: Vec3, dir: Vec3, depth, spheres, hit, scratch, out_col) -> Double {
  let t = closest_hit(origin, dir, spheres, hit)
  if (t < 0.0) {
    let sky_t = 0.5 * (dir.y + 1.0)
    matrix_set(out_col, 0.0, 0.0, 0.5 + 0.3 * sky_t)
    matrix_set(out_col, 1.0, 0.0, 0.6 + 0.4 * sky_t)
    matrix_set(out_col, 2.0, 0.0, 0.8 + 0.2 * sky_t)
  }
  else {
    let best_i = matrix_get(hit, 1.0, 0.0)
    let hx = matrix_get(hit, 2.0, 0.0); let hy = matrix_get(hit, 3.0, 0.0); let hz = matrix_get(hit, 4.0, 0.0)
    let nx = matrix_get(hit, 5.0, 0.0); let ny = matrix_get(hit, 6.0, 0.0); let nz = matrix_get(hit, 7.0, 0.0)

    let sr = matrix_get(spheres, best_i, 4.0); let sg = matrix_get(spheres, best_i, 5.0)
    let sb = matrix_get(spheres, best_i, 6.0); let ref = matrix_get(spheres, best_i, 7.0)

    var r = 0.3 * sr; var g = 0.3 * sg; var b = 0.3 * sb

    let light = vec3_normalize(vec3_new(0.0, 4.0, 3.0))
    let hit_pt = vec3_new(hx, hy, hz)
    let normal = vec3_new(nx, ny, nz)
    let ndotl = vec3_dot(normal, light)
    if (ndotl > 0.0) {
      let sh_o = vec3_new(hx + 0.001 * light.x, hy + 0.001 * light.y, hz + 0.001 * light.z)
      let st = closest_hit(sh_o, light, spheres, scratch)
      if (st < 0.0) {
        r = r + sr * ndotl * 0.7; g = g + sg * ndotl * 0.7; b = b + sb * ndotl * 0.7
      }
    }

    let half = vec3_normalize(vec3_sub(light, dir))
    let ndoth = vec3_dot(normal, half)
    if (ndoth > 0.0 && ndotl > 0.0) {
      var spec = 1.0; var si = 0.0
      while (si < 16.0) { spec = spec * ndoth; si = si + 1.0 }
      r = r + 0.4 * spec; g = g + 0.4 * spec; b = b + 0.4 * spec
    }

    if (depth > 1.0 && ref > 0.01) {
      let refl = vec3_reflect(dir, normal)
      let r_o = vec3_new(hx + 0.001 * refl.x, hy + 0.001 * refl.y, hz + 0.001 * refl.z)
      shade(r_o, refl, depth - 1.0, spheres, hit, scratch, out_col)
      let rr = matrix_get(out_col, 0.0, 0.0); let rg = matrix_get(out_col, 1.0, 0.0); let rb = matrix_get(out_col, 2.0, 0.0)
      r = r + ref * rr; g = g + ref * rg; b = b + ref * rb
    }

    matrix_set(out_col, 0.0, 0.0, clamp(r))
    matrix_set(out_col, 1.0, 0.0, clamp(g))
    matrix_set(out_col, 2.0, 0.0, clamp(b))
  }
}

def build_camera(eye: Vec3, look: Vec3, cam_out) -> Double {
  let fwd = vec3_normalize(vec3_sub(look, eye))
  let right = vec3_normalize(vec3_new(-fwd.z, 0.0, fwd.x))
  let up = vec3_cross(right, fwd)
  matrix_set(cam_out, 0.0, 0.0, eye.x)
  matrix_set(cam_out, 1.0, 0.0, eye.y)
  matrix_set(cam_out, 2.0, 0.0, eye.z)
  matrix_set(cam_out, 3.0, 0.0, fwd.x)
  matrix_set(cam_out, 4.0, 0.0, fwd.y)
  matrix_set(cam_out, 5.0, 0.0, fwd.z)
  matrix_set(cam_out, 6.0, 0.0, right.x)
  matrix_set(cam_out, 7.0, 0.0, right.y)
  matrix_set(cam_out, 8.0, 0.0, right.z)
  matrix_set(cam_out, 9.0, 0.0, up.x)
  matrix_set(cam_out, 10.0, 0.0, up.y)
  matrix_set(cam_out, 11.0, 0.0, up.z)
}

def K_IMG() 20.0; def K_PROG() 21.0; def K_LBL() 22.0

def on_render() {
  let width = 300.0; let height = 225.0; let max_depth = 3.0
  let fov = 60.0; let aspect = width / height
  let tan_hfov = sin(fov * 3.14159265 / 360.0) / cos(fov * 3.14159265 / 360.0)

  let spheres = build_scene()
  let cam = matrix_zeros(12.0, 1.0)
  build_camera(vec3_new(0.0, 0.8, -1.0), vec3_new(0.0, 0.0, -3.0), cam)

  let eye = vec3_new(matrix_get(cam, 0.0, 0.0), matrix_get(cam, 1.0, 0.0), matrix_get(cam, 2.0, 0.0))
  let fwd = vec3_new(matrix_get(cam, 3.0, 0.0), matrix_get(cam, 4.0, 0.0), matrix_get(cam, 5.0, 0.0))
  let right = vec3_new(matrix_get(cam, 6.0, 0.0), matrix_get(cam, 7.0, 0.0), matrix_get(cam, 8.0, 0.0))
  let up = vec3_new(matrix_get(cam, 9.0, 0.0), matrix_get(cam, 10.0, 0.0), matrix_get(cam, 11.0, 0.0))

  let hit = matrix_zeros(8.0, 1.0)
  let scratch = matrix_zeros(8.0, 1.0)
  let col = matrix_zeros(3.0, 1.0)

  flux_qt_set_text(flux_get_var(K_LBL()), "Rendering...")

  var py = 0.0
  while (py < height) {
    var px = 0.0
    while (px < width) {
      let ndc_x = (2.0 * (px + 0.5) / width - 1.0) * aspect * tan_hfov
      let ndc_y = (1.0 - 2.0 * (py + 0.5) / height) * tan_hfov
      let dir = vec3_normalize(vec3_new(
        ndc_x * right.x + ndc_y * up.x + fwd.x,
        ndc_x * right.y + ndc_y * up.y + fwd.y,
        ndc_x * right.z + ndc_y * up.z + fwd.z))
      shade(eye, dir, max_depth, spheres, hit, scratch, col)
      flux_qt_put_pixel(matrix_get(col, 0.0, 0.0), matrix_get(col, 1.0, 0.0), matrix_get(col, 2.0, 0.0))
      px = px + 1.0
    }
    py = py + 1.0
  }

  flux_qt_render_image(flux_get_var(K_IMG()), width, height)
  flux_qt_set_text(flux_get_var(K_LBL()), "Done!")
  flux_qt_progress_set_value(flux_get_var(K_PROG()), 100.0)
  0.0
}

def main() {
  let win = flux_qt_get_window()
  flux_qt_set_window_title(win, "Ray Viewer")
  flux_qt_set_window_size(win, 400, 380)

  let central = flux_qt_create_widget()
  let lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(central, lay)
  flux_qt_set_central_widget(win, central)

  let lbl = flux_qt_create_label("Click Render to trace rays")
  flux_qt_layout_add_widget(lay, lbl)
  flux_set_var(K_LBL(), lbl)

  let img = flux_qt_create_label("")
  flux_qt_set_window_size(img, 300, 225)
  flux_qt_layout_add_widget(lay, img)
  flux_set_var(K_IMG(), img)

  let prog = flux_qt_create_progressbar()
  flux_qt_progress_set_range(prog, 0, 100)
  flux_qt_layout_add_widget(lay, prog)
  flux_set_var(K_PROG(), prog)

  let btn = flux_qt_create_button("Render Raytraced Scene")
  flux_qt_on_click_by_name(btn, "on_render")
  flux_qt_layout_add_widget(lay, btn)

  on_render()
  0.0
}

main()
