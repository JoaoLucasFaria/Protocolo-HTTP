#include "http.h"

const char* guess_mime(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    ext++;
    if (!strcasecmp(ext,"html") || !strcasecmp(ext,"htm")) return "text/html; charset=utf-8";
    if (!strcasecmp(ext,"txt"))  return "text/plain; charset=utf-8";
    if (!strcasecmp(ext,"css"))  return "text/css";
    if (!strcasecmp(ext,"js"))   return "application/javascript";
    if (!strcasecmp(ext,"png"))  return "image/png";
    if (!strcasecmp(ext,"jpg") || !strcasecmp(ext,"jpeg")) return "image/jpeg";
    if (!strcasecmp(ext,"gif"))  return "image/gif";
    if (!strcasecmp(ext,"pdf"))  return "application/pdf";
    return "application/octet-stream";
}