#ifndef LOG_MACROS_H__
#define LOG_MACROS_H__

#ifndef FILENAME_MAX
# define FILENAME_MAX   260
#endif // FILENAME_MAX


#define LOG_MESSAGE_MAX_SIZE       2048
#define LOG_BUFFER_SIZE            1024*1024*2L   // 2M debug 2048
#define LOG_FLUSH_INTERVAL_MSEC    5000            // 5s
#define LOG_FILE_MAX_SIZE          1024*1024*100L // 100M debug 10240//

#endif//LOG_MACROS_H__
