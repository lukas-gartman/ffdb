#include "Typedefs.h"

__thread unsigned char *current_base __attribute__((tls_model("initial-exec")))
    = nullptr;