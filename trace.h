#ifndef TRACE_H_
#define TRACE_H_

#include <stdio.h>

#define TRACE_LEVEL_DEBUG     5
#define TRACE_LEVEL_INFO      4
#define TRACE_LEVEL_WARNING   3
#define TRACE_LEVEL_ERROR     2
#define TRACE_LEVEL_FATAL     1
#define TRACE_LEVEL_NO_TRACE  0

#if !defined(TRACE_LEVEL)
#define TRACE_LEVEL TRACE_LEVEL_NO_TRACE
#endif

#if defined(NOTRACE)
#error "Error: NOTRACE has to be not defined !"
#endif

#undef NOTRACE
#if (TRACE_LEVEL == TRACE_LEVEL_NO_TRACE)
#define NOTRACE
#endif

typedef void (*OutcharFunc)(char c);
typedef void (*FlushFunc)(void);

extern FILE trace;
extern OutcharFunc outchar;
extern FlushFunc flush;

#if defined(NOTRACE)
#define TRACE_DEBUG(...)      { }
#define TRACE_INFO(...)       { }
#define TRACE_WARNING(...)    { }
#define TRACE_ERROR(...)      { }
#define TRACE_FATAL(...)      { while(1); }
#define TRACE_DEBUG_WP(...)   { }
#define TRACE_INFO_WP(...)    { }
#define TRACE_WARNING_WP(...) { }
#define TRACE_ERROR_WP(...)   { }
#define TRACE_FATAL_WP(...)   { while(1); }
#else
#if (TRACE_LEVEL >= TRACE_LEVEL_DEBUG)
#define TRACE_DEBUG(...)      { fprintf(&trace, "-D- " __VA_ARGS__); fprintf(&trace, "\r\n"); flush(); }
#define TRACE_DEBUG_WP(...)   { fprintf(&trace, __VA_ARGS__); fprintf(&trace, "\r\n"); flush(); }
#else
#define TRACE_DEBUG(...)      { }
#define TRACE_DEBUG_WP(...)   { }
#endif

#if (TRACE_LEVEL >= TRACE_LEVEL_INFO)
#define TRACE_INFO(...)       { fprintf(&trace, "-I- " __VA_ARGS__); fprintf(&trace, "\r\n"); flush(); }
#define TRACE_INFO_WP(...)    { fprintf(&trace, __VA_ARGS__); fprintf(&trace, "\r\n"); flush(); }
#else
#define TRACE_INFO(...)       { }
#define TRACE_INFO_WP(...)    { }
#endif

#if (TRACE_LEVEL >= TRACE_LEVEL_WARNING)
#define TRACE_WARNING(...)    { fprintf(&trace, "-W- " __VA_ARGS__); fprintf(&trace, "\r\n"); flush() }
#define TRACE_WARNING_WP(...) { fprintf(&trace, __VA_ARGS__); fprintf(&trace, "\r\n"); flush(); }
#else
#define TRACE_WARNING(...)    { }
#define TRACE_WARNING_WP(...) { }
#endif

#if (TRACE_LEVEL >= TRACE_LEVEL_ERROR)
#define TRACE_ERROR(...)      { fprintf(&trace, "-E- " __VA_ARGS__); fprintf(&trace, "\r\n"); flush(); }
#define TRACE_ERROR_WP(...)   { fprintf(&trace, __VA_ARGS__); fprintf(&trace, "\r\n"); flush(); }
#else
#define TRACE_ERROR(...)      { }
#define TRACE_ERROR_WP(...)   { }
#endif

#if (TRACE_LEVEL >= TRACE_LEVEL_FATAL)
#define TRACE_FATAL(...)      { fprintf(&trace, "-F- " __VA_ARGS__); fprintf(&trace, "\r\n"); flush(); while(1); }
#define TRACE_FATAL_WP(...)   { fprintf(&trace, __VA_ARGS__); fprintf(&trace, "\r\n"); flush(); while(1); }
#else
#define TRACE_FATAL(...)      { while(1); }
#define TRACE_FATAL_WP(...)   { while(1); }
#endif
#endif

#ifdef NOASSERT
#define ASSERT(...)
#define SANITY_CHECK(...)
#else
#if (TRACE_LEVEL == TRACE_LEVEL_NO_TRACE)
#define ASSERT(condition, ...) \
{ \
	if(!(condition)) \
	{ \
		while(1); \
	} \
}
#define SANITY_CHECK(condition) ASSERT(condition, ...)
#else
#define ASSERT(condition, ...) \
{ \
	if(!(condition)) \
	{ \
		fprintf(&trace, "-F- ASSERT: " __VA_ARGS__); \
		fprintf(&trace, "\r\n"); \
		flush(); \
		while(1); \
	} \
}
#define SANITY_ERROR "Sanity check failed at %s:%d\n\r"
#define SANITY_CHECK(condition) ASSERT(condition, SANITY_ERROR, __FILE__, __LINE__)
#endif
#endif

void trace_setup(OutcharFunc t, FlushFunc);

#endif /* TRACE_H_ */
