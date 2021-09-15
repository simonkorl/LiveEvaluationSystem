#ifndef MC_UTILS_H
#define MC_UTILS_H
// debug functions
#define DEBUG 0
#if DEBUG
#define eprintf(format, args...)                                               \
  {                                                                            \
    fprintf(stderr, "[DEBUG] %s->%s()->line.%d : " format "\n", __FILE__,      \
            __FUNCTION__, __LINE__, ##args);                                   \
  }
#else
#define eprintf(format, args...)                                               \
  {}
#endif // DEBUG
#endif // MC_UTILS_H
