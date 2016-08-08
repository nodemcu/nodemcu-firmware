#include "spiffs.h"
bool myspiffs_mount();
void myspiffs_unmount();
int myspiffs_open(const char *name, int flags);
int myspiffs_close( int fd );
size_t myspiffs_write( int fd, const void* ptr, size_t len );
size_t myspiffs_read( int fd, void* ptr, size_t len);
int myspiffs_lseek( int fd, int off, int whence );
int myspiffs_eof( int fd );
int myspiffs_tell( int fd );
int myspiffs_getc( int fd );
int myspiffs_ungetc( int c, int fd );
int myspiffs_flush( int fd );
int myspiffs_error( int fd );
void myspiffs_clearerr( int fd );
int myspiffs_check( void );
int myspiffs_rename( const char *old, const char *newname );
size_t myspiffs_size( int fd );
int myspiffs_format (void);

