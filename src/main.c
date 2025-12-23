#include <stdio.h>

int main() {
    // C23 features test
    // 1. bool, true, false are keywords in C23 (no need for stdbool.h)
    bool is_c23 = true;

    // 2. Binary literals (0b prefix)
    int binary_val = 0b101010;

    // 3. Digit separators
    int million = 1'000'000;

    // 4. Empty initializer for arrays/structs (if supported by clang yet, standard C23)
    int arr[5] = {}; 

    if (is_c23) {
        printf("Hello, C23!\n");
        printf("Binary value: %d\n", binary_val);
        printf("Million: %d\n", million);
        printf("Array first element: %d\n", arr[0]);
    }

    return 0;
}
