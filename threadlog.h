#ifndef	THREADLOG_H_
#define	THREADLOG_H_

#include <ctime>
#include <cstring>
#include <unistd.h>
#include <pthread.h>

#define MAXLOGLEN	1024

void			log_dbg_print(const char * pFuncName, const char * pFormat, ...);
const char *	get_filename(const char * pFilePath);

#define DEG_LOG(fmt, args...)	\
   {				\
	char format[MAXLOGLEN];	\
	sprintf(format, "%s:%d %s()", get_filename(__FILE__), __LINE__, __FUNCTION__);	\
	log_dbg_print(format, fmt, ##args);	\
   }

#endif
