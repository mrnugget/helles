#ifndef _helles_logging_h
#define _helles_logging_h

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#define LOG_GREEN(str) printf(KGRN str KNRM);
#define LOG_RED(str) printf(KRED str KNRM);

#endif
