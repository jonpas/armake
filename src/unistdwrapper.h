#ifndef _WIN32
#include <unistd.h>
#else
// https://stackoverflow.com/questions/341817/is-there-a-replacement-for-unistd-h-for-windows-visual-c/826027#826027

#define R_OK    4       /* Test for read permission.  */
#define W_OK    2       /* Test for write permission.  */
//#define   X_OK    1       /* execute permission - unsupported in windows*/
#define F_OK    0       /* Test for existence.  */

#include <io.h>

#define access _access
#define dup2 _dup2
#define execve _execve
#define ftruncate _chsize
#define unlink _unlink
#define fileno _fileno
#define getcwd _getcwd
#define chdir _chdir
#define isatty _isatty
#define lseek _lseek
/* read, write, and close are NOT being #defined here,
* because while there are file handle specific versions for Windows,
* they probably don't work for sockets.
* You need to look at your app and consider whether
* to call e.g. closesocket().
*/
#endif
