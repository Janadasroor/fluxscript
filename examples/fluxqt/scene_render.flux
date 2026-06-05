import veclib
import scene

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
      flux_qt_put_pixel(
        matrix_get(col, 0.0, 0.0),
        matrix_get(col, 1.0, 0.0),
        matrix_get(col, 2.0, 0.0))
      px = px + 1.0
    }
    py = py + 1.0
  }

  flux_qt_render_image(flux_get_var(K_IMG()), width, height)
  flux_qt_set_text(flux_get_var(K_LBL()), "Done")
  flux_qt_progress_set_value(flux_get_var(K_PROG()), 100.0)
  0.0
}

def main() {
  let win = flux_qt_get_window()
  flux_qt_set_window_title(win, "Scene Render")
  flux_qt_set_window_size(win, 400, 380)
  let central = flux_qt_create_widget()
  let lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(central, lay)
  flux_qt_set_central_widget(win, central)
  let lbl = flux_qt_create_label("Click")
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
  let btn = flux_qt_create_button("Render")
  flux_qt_on_click_by_name(btn, "on_render")
  flux_qt_layout_add_widget(lay, btn)
  0.0
}
main()
