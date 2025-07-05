/*
 * Command execution helpers.
 *
 * Provides routines for running external programs and formatting
 * command strings for debugging.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_COMMAND_H
#define VC_COMMAND_H

/* Convert an argument vector into a single string for debugging.
 * Arguments containing spaces or shell metacharacters are quoted using
 * single quotes. The returned buffer is heap allocated and must be freed
 * by the caller. Returns NULL if the result would exceed implementation
 * limits.
 */
char *command_to_string(char *const argv[]);

/* Spawn a command and wait for completion.
 * Returns 1 on success, 0 on failure and -1 if the child was terminated
 * by a signal.
 */
int command_run(char *const argv[]);

#endif /* VC_COMMAND_H */
