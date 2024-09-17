#pragma once

#include <string>
#include "nocopyable.h"

#define LOG_INFO(LogmsgFormat, ...)\
    do{\
        Logger &logger = Logger::instance();\
        logger.setLogLevel(INFO);\
        char buf[1024] = {0};\
        snprintf(buf, 1024, LogmsgFormat, ##__VA_ARGS__);\
        logger.log(buf);\
    }while(0)

#define LOG_ERROR(LogmsgFormat, ...)\
    do{\
        Logger &logger = Logger::instance();\
        logger.setLogLevel(INFO);\
        char buf[1024] = {0};\
        snprintf(buf, 1024, LogmsgFormat, ##__VA_ARGS__);\
        logger.log(buf);\
    }while(0)

#define LOG_FATAL(LogmsgFormat, ...)\
    do{\
        Logger &logger = Logger::instance();\
        logger.setLogLevel(FATAL);\
        char buf[1024] = {0};\
        snprintf(buf, 1024, LogmsgFormat, ##__VA_ARGS__);\
        logger.log(buf);\
        exit(-1);\
    }while(0)
#ifdef MUDEBUG
#define LOG_DEBUG(LogmsgFormat, ...)\
    do{\
        Logger &logger = Logger::instance();\
        logger.setLogLevel(DEBUG);\
        char buf[1024] = {0};\
        snprintf(buf, 1024, LogmsgFormat, ##__VA_ARGS__);\
        logger.log(buf);\
    }while(0)
#else 
    #define LOG_DEBUG(LogmsgFormat, ...)
#endif

/* 
    定义日志的级别  INOF  ERROR  FATAL  DEBUG
 */

enum  LogLevel
{
    INFO,
    ERROR,
    FATAL,
    DEBUG
};

/* 
输出一个日志类
 */

// 单列

class Logger : nocopyable
{
public:
    // 获取日志唯一的实列对象
    static Logger &instance();
    //设置日志级别
    void setLogLevel(int Level);
    //写日志
    void log(std::string msg);
private:
    int logLevel_;
    Logger(){

    }
};