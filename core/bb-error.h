#ifndef __BB_ERROR_H_
#define __BB_ERROR_H_

#define BB_ERROR_DOMAIN_QUARK g_quark_from_static_string("BBError")
typedef enum
{
  BB_ERROR_INVALID_STATE,
  BB_ERROR_INVALID_TYPE,
  BB_ERROR_INVALID_ARG,
  BB_ERROR_PARSE,
  BB_ERROR_FILE_READ,
  BB_ERROR_FILE_WRITE,
} BbErrorCode;

#endif
