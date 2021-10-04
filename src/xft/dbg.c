#include "xftint.h"

_X_HIDDEN int XftDebug(void) {
	static int initialized;
	static int debug;
	
	if (!initialized) {
		initialized = 1;
		char* e = getenv("XFT_DEBUG");
		if (e) {
			printf("XFT_DEBUG=%s\n", e);
			debug = atoi (e);
			if (debug <= 0)
				debug = 1;
		}
	}
	return debug;
}