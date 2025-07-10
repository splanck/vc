#define __GLIBC_USE(x) 1
#if __GLIBC_USE(FORTIFY)
# define __BEGIN_DECLS extern "C" {
# define __END_DECLS }
#endif
__BEGIN_DECLS
int foo;
__END_DECLS

