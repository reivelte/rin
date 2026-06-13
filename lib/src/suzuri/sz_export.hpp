
#ifndef SZ_API_H
#define SZ_API_H

#ifdef SZ_STATIC_DEFINE
#  define SZ_API
#  define SZ_NO_EXPORT
#else
#  ifndef SZ_API
#    ifdef suzuri_EXPORTS
        /* We are building this library */
#      define SZ_API __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define SZ_API __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef SZ_NO_EXPORT
#    define SZ_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef SZ_DEPRECATED_API
#  define SZ_DEPRECATED_API __attribute__ ((__deprecated__))
#endif

#ifndef SZ_DEPRECATED_API_EXPORT
#  define SZ_DEPRECATED_API_EXPORT SZ_API SZ_DEPRECATED_API
#endif

#ifndef SZ_DEPRECATED_API_NO_EXPORT
#  define SZ_DEPRECATED_API_NO_EXPORT SZ_NO_EXPORT SZ_DEPRECATED_API
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef SZ_NO_DEPRECATED
#    define SZ_NO_DEPRECATED
#  endif
#endif

#endif /* SZ_API_H */
