// AOT wrapper: links a compiled .flux object file into a standalone executable.
// Uses dlsym to find the anonymous entry point at runtime.
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

int main(int argc, char** argv)
{
    (void)argc;
    // Self-reflective lookup: find the anon_expr function in our own symbols
    void* handle = dlopen(nullptr, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "AOT Error: dlopen failed: %s\n", dlerror());
        return 1;
    }

    // Search for any symbol containing "_anon_expr"
    // We iterate through all symbols in the ELF
    double result = 0.0;
    bool found = false;

    // Try exact well-known names first
    const char* candidates[] = {
        "__anon_expr",
        "anon_expr",
        nullptr
    };
    for (int i = 0; candidates[i]; ++i) {
        void* sym = dlsym(handle, candidates[i]);
        if (sym) {
            typedef double (*Fn)();
            Fn fn = reinterpret_cast<Fn>(sym);
            result = fn();
            found = true;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "AOT Error: could not find entry point\n");
        dlclose(handle);
        return 1;
    }

    fprintf(stdout, "Result: %g\n", result);
    dlclose(handle);
    return 0;
}
