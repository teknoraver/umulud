#ifndef HAVE_CONFIGFILE_H
#define HAVE_CONFIGFILE_H

extern unsigned int debug_level;

#ifdef DEBUG
#define DEBUGPRINTF(t, s, a...) if (stderr && debug_level > 1){ \
                                  fprintf(stderr, "%s: %s(): ", t, __FUNCTION__); \
                                  fprintf(stderr, s "\n", ##a); \
                                }
#else
#define DEBUGPRINTF(t, s, a...)
#endif

#define BIT(x)			(1 << x)
#define HID_DEBUG_TRACES	BIT(0)
#define HID_DEBUG_NOTICES	BIT(1)
#define HID_DEBUG_WARNINGS	BIT(2)
#define HID_DEBUG_ERRORS	BIT(3)

#define TRACE(s, a...) if (debug_level & HID_DEBUG_TRACES) { DEBUGPRINTF("  TRACE", s, ##a) }
#define NOTICE(s, a...) if (debug_level & HID_DEBUG_NOTICES) { DEBUGPRINTF(" NOTICE", s, ##a) }
#define WARNING(s, a...) if (debug_level & HID_DEBUG_WARNINGS) { DEBUGPRINTF("WARNING", s, ##a) }
#define ERROR(s, a...) if (debug_level & HID_DEBUG_ERRORS) { DEBUGPRINTF("  ERROR", s, ##a) }
#define MESSAGE(s, a...) DEBUGPRINTF("  MESSAGE", s, ##a) 

#endif
