/*
 * Shared object to override wcwidth/wcswidth, which asks rxvt-unicode for the
 * width of a char.
 *
 * TODO:
 * - is it possible from rxvt-unicode to detect that the client does not use
 *   its rxvtwcwidth.so?  In that case rxvt-unicode might fall back to the
 *   system's, too - or rather just be more careful.
 *   This happens e.g. when attaching to a tmux session, which was started
 *   without the wrapper.
 *
 * XXX: this is quite a mess still.
 *  - would it be possible to have a terminal escape char interface for this?!
 */

#include "../config.h"          /* NECESSARY */
#include "rxvt.h"               /* NECESSARY */
#include <sys/socket.h>
#include <sys/un.h>
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>

#include "rxvtwcwidth.h"

int SOCKET_STATUS = -1;
MapType wcwidth_cache;

typedef int (*orig_wcwidth_f_type)(wchar_t c);
int
orig_wcwidth(wchar_t c)
{
    orig_wcwidth_f_type wcwidth;
    wcwidth = (orig_wcwidth_f_type)dlsym(RTLD_NEXT, "wcwidth");
    return wcwidth(c);
}


int _wcwidth(wchar_t c)
{
  const char *wcwidth_socket_name = getenv("RXVT_WCWIDTH_SOCKET");
  if (!wcwidth_socket_name)
  {
    if (SOCKET_STATUS == -1)
    {
#ifdef DEBUG_WCWIDTH
      fprintf(stderr, "RXVT_WCWIDTH_SOCKET is not set (pid %d). Using orig_wcwidth.\n",
          getpid());
#endif
    }
    SOCKET_STATUS = 0;
    return orig_wcwidth(c);
  }
  /*
   * connect errors etc, allowing to re-activate by via setting and unsetting
   * the socket name.
   */
  if (SOCKET_STATUS == -2)
    return orig_wcwidth(c);

  if (SOCKET_STATUS == 0)
  {
    fprintf(stderr, "RXVT_WCWIDTH_SOCKET activated again (pid %d).\n", getpid());
    SOCKET_STATUS = 1;
    wcwidth_cache.clear();
  }
  else
  {
    /* Cached? */
    MapType::iterator it;
    it = wcwidth_cache.find(c);
    if(it != wcwidth_cache.end()) {
      return it->second;
    }
  }

#ifdef DEBUG_WCWIDTH_CLIENT
  fprintf(stderr, "_wcwidth: connecting to socket: %s\n", wcwidth_socket_name);
#endif
  int wcwidth_socket_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if (wcwidth_socket_fd == -1) {
    perror("wcwidth: could not open socket");
    SOCKET_STATUS = -2;
    return orig_wcwidth(c);
  }

  /* Bind socket to socket name. */
  struct sockaddr_un name;
  name.sun_family = AF_UNIX;
  strcpy(name.sun_path, wcwidth_socket_name);
  int len = strlen(name.sun_path) + sizeof(name.sun_family);

  // Write should be non-blocking in case the parent is gone (tmux).
  int orig_flags;
  orig_flags = fcntl(wcwidth_socket_fd, F_GETFL);
  fcntl(wcwidth_socket_fd, F_SETFL, orig_flags | O_NONBLOCK);

  if (-1 == connect(wcwidth_socket_fd, (struct sockaddr *)&name, len))
  {
    if (errno == EINPROGRESS)
    {
#ifdef DEBUG_WCWIDTH
      fprintf(stderr, "EINPROGRESS in connect() - selecting\n");
#endif
      struct timeval tv;
      fd_set myset;
      do
      {
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        FD_ZERO(&myset);
        FD_SET(wcwidth_socket_fd, &myset);
        int ret = select(wcwidth_socket_fd+1, NULL, &myset, NULL, &tv);
        if (ret < 0 && errno != EINTR)
        {
          perror("Error connecting");
        }
        else if (ret > 0)
        {
          // Socket selected for write
          socklen_t lon;
          lon = sizeof(int);
          int valopt;
          if (getsockopt(wcwidth_socket_fd, SOL_SOCKET, SO_ERROR,
                         (void*)(&valopt), &lon) < 0)
          {
            perror("getsockopt");
          }
          else if (!valopt)
          {
            // Success.
            break;
          }
          perror("Error in delayed connection()");
        }
        else {
          perror("Timeout in select() - Cancelling!");
        }
        SOCKET_STATUS = -2;
        close(wcwidth_socket_fd);
        return orig_wcwidth(c);
      } while (1);
    }
    else
    {
      fprintf(stderr, "Could not connect to socket %s: %s\n",
          wcwidth_socket_name, strerror(errno));
      SOCKET_STATUS = -2;
      close(wcwidth_socket_fd);
      return orig_wcwidth(c);
    }
  }
  fcntl(wcwidth_socket_fd, F_SETFL, orig_flags);


#ifdef DEBUG_WCWIDTH_CLIENT
  fprintf(stderr, "_wcwidth: sending query: %lc\n", c);
#endif
  int ret = write(wcwidth_socket_fd, &c, sizeof(wchar_t));
  int width;
  if (ret == -1) {
    perror("write");
    width = orig_wcwidth(c);
  }
  else
  {
#ifdef DEBUG_WCWIDTH_CLIENT
    fprintf(stderr, "_wcwidth: reading answer\n");
#endif
    ret = read(wcwidth_socket_fd, &width, sizeof(int));
    if (ret == -1)
    {
        perror("read");
        width = orig_wcwidth(c);
    }
    else
    {
#ifdef DEBUG_WCWIDTH_CLIENT
      fprintf(stderr, "_wcwidth: read: %i\n", width);
#endif
      wcwidth_cache[c] = width;
    }
  }
  close(wcwidth_socket_fd);
  return width;
}


int wcwidth(wchar_t c) {
  orig_wcwidth_f_type wcwidth = _wcwidth;
  return WCWIDTH(c);
}


/* This seems to be required, since it will not always use (the injected)
 * wcwidth?!
 */
int wcswidth(const wchar_t *pwcs, size_t n)
{
  int w, width = 0;

  for (;*pwcs && n-- > 0; pwcs++)
    if ((w = wcwidth(*pwcs)) < 0)
      return -1;
    else
      width += w;

  return width;
}
