mkdir -p assignments/assignment2

{
    echo "Version="
    aarch64-none-linux-gnu-gcc --version
    echo "Sysroot="
    aarch64-none-linux-gnu-gcc -print-sysroot
    echo "Config="
    aarch64-none-linux-gnu-gcc -v 2>&1
} > assignments/assignment2/cross-compile.txt
