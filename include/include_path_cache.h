/*
 * Caches for system include path discovery.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */
#ifndef VC_INCLUDE_PATH_CACHE_H
#define VC_INCLUDE_PATH_CACHE_H

/* Initialize cached include path information */
void include_path_cache_init(void);

/* Clean up cached include path information */
void include_path_cache_cleanup(void);

/* Obtain the multiarch triple directory (or NULL) */
const char *include_path_cache_multiarch(void);

/* Obtain the GCC include directory path */
const char *include_path_cache_gcc_dir(void);

/* Standard include directory list terminated by NULL */
const char * const *include_path_cache_std_dirs(void);

#endif /* VC_INCLUDE_PATH_CACHE_H */
