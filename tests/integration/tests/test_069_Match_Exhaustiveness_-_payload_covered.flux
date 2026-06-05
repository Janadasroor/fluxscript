enum Option { Some(Double), None }
def run_check() -> Double {
    let opt = Option.Some(5.0);
    match opt {
        Option.Some(x) -> x,
        Option.None -> 0.0
    }
}
run_check()
