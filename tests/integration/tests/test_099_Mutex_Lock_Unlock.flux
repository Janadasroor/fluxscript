def main() -> Double {
    let mtx = flux_mutex_create();
    flux_mutex_lock(mtx);
    flux_mutex_unlock(mtx);
    flux_mutex_destroy(mtx);
    1.0
}

main()
