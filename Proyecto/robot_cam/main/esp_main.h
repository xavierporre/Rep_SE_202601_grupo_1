#pragma once
#include "sdkconfig.h"

/* cam_integrado: flujo camara activo, sin display, sin CLI */
/* NO definir CLI_ONLY_INFERENCE -> loop() con camara activo */

#ifdef __cplusplus
extern "C" {
#endif
extern void run_inference(void *ptr);
#ifdef __cplusplus
}
#endif
