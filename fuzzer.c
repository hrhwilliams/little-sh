#include <stddef.h>
#include <stdint.h>

#include "tokenizer.h"

int LLVMFuzzerTestOneInput(char *data, size_t size) {
    tokenize(data);
    return 0;  // Values other than 0 and -1 are reserved for future use.
}
