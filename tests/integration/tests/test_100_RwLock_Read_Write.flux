def main() -> Double {
    let rw = flux_rwlock_create();
    flux_rwlock_read_lock(rw);
    flux_rwlock_unlock(rw);
    flux_rwlock_write_lock(rw);
    flux_rwlock_unlock(rw);
    flux_rwlock_destroy(rw);
    1.0
}

main()
