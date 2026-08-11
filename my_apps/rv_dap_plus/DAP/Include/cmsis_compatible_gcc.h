/* Minimal CMSIS compatibility layer for GCC.
 * Provides macros expected by the DAP_config.h / CMSIS-DAP code.
 */
#ifndef __CMSIS_COMPATIBLE_GCC_H__
#define __CMSIS_COMPATIBLE_GCC_H__

#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif

#ifndef __WEAK
#define __WEAK __attribute__((weak))
#endif

#ifndef __ALIGN_BEGIN
#define __ALIGN_BEGIN
#endif

#ifndef __ALIGN_END
#define __ALIGN_END __attribute__((aligned(4)))
#endif

#endif /* __CMSIS_COMPATIBLE_GCC_H__ */
