#include "Typedefs.h"
#include "FFDB2.h"

__thread unsigned char *current_base __attribute__((tls_model("initial-exec"))) = nullptr;
__thread FFDB *current_db __attribute__((tls_model("initial-exec"))) = nullptr;