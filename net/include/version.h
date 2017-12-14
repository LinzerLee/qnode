/*
 * version.h
 *
 *  Created on: 2017年10月25日
 *      Author: linzer
 */

#ifndef __QNODE_VERSION_H__
#define __QNODE_VERSION_H__

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0
#define VERSION_RELEASE 1
#define VERSION_SUFFIX ""

#define VERSION_HEX  ((VERSION_MAJOR << 16) | \
                      (VERSION_MINOR <<  8) | \
                      (VERSION_PATCH))

#define STRINGIFY(v) #v
#define VERSION_STRING_BASE  STRINGIFY(UV_VERSION_MAJOR) "." \
                             STRINGIFY(UV_VERSION_MINOR) "." \
                             STRINGIFY(UV_VERSION_PATCH)

#if VERSION_RELEASE
# define VERSION_STRING  VERSION_STRING_BASE
#else
# define VERSION_STRING  VERSION_STRING_BASE "-" VERSION_SUFFIX
#endif

#endif /* __QNODE_VERSION_H__ */
