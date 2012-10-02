#ifdef __KERNEL__
# ifdef CONFIG_SUPERH32
#  include <asm/unistd_32.h>
# else
#  include <asm/unistd_64.h>
# endif
#else
# ifdef __SH5__
#  include <asm/unistd_64.h>
# else
#  include <asm/unistd_32.h>
# endif
#endif
