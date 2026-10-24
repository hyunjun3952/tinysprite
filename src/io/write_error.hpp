#ifndef TINYSPRITE_IO_WRITE_ERROR_HPP
#define TINYSPRITE_IO_WRITE_ERROR_HPP

/* Forgets the reason of the last failed file write on this thread. Exporters call it first, so a
 * refusal that never reaches the disk (bad extension, bad scale) does not report an older error. */
void file_write_error_clear();

#endif
