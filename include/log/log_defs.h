/*
** Copyright (C) 2020 FITSCO.
** @file common.h
** @brief All macros
** @author Rimon Chen
** @date 2021-01-13
** @version 1.0
**
** History:
**   1. 2021-01-13 Create this file
**/
#ifndef LOG_LOG_DEFS_H__
#define LOG_LOG_DEFS_H__

#ifdef _WIN32
#define LOG_EXPORT __declspec(dllexport)
#define LOG_IMPORT __declspec(dllimport)
#else
#define LOG_EXPORT __attribute__((visibility("default")))
#define LOG_IMPORT __attribute__((visibility("default")))
#endif // _WIN32

#ifdef LOG_LIB
#define LOG_API LOG_EXPORT
#else
#define LOG_API LOG_IMPORT
#endif // LOG_LIB

#ifdef  __cplusplus
# define LOG_BEGIN_DECLS  extern "C" {
# define LOG_END_DECLS    }
#else
# define LOG_BEGIN_DECLS
# define LOG_END_DECLS
#endif

#define UNUSED(x)                 (void)x
#define STRING_IP4_MAXLEN         16

#endif // LOG_LOG_DEFS_H__
