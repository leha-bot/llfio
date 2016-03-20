/* io_service.hpp
Multiplex file i/o
(C) 2015 Niall Douglas http://www.nedprod.com/
File Created: Dec 2015


Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef BOOST_AFIO_IO_SERVICE_H
#define BOOST_AFIO_IO_SERVICE_H

#include "handle.hpp"

#include <deque>

#undef _threadid  // windows macro splosh sigh

//!\def BOOST_AFIO_COMPILE_KQUEUES Undefined to autodetect, 1 to compile in BSD kqueue support, 0 to leave it out
/*!\def BOOST_AFIO_USE_POSIX_AIO Undefined to autodetect, 1 to use POSIX AIO, 0 to not use

\warning On FreeBSD the AIO kernel module needs to be loaded for POSIX AIO to work.
Run as root 'kldload aio' or add 'aio_load=YES' in loader.conf.
*/
//!\def BOOST_AFIO_IO_POST_SIGNAL Undefined to autoset to first free SIGRTMIN if realtime signals available, else SIGUSR1. Only used if BOOST_AFIO_USE_KQUEUES=0.
//!\def BOOST_AFIO_HAVE_REALTIME_SIGNALS Undefined to autodetect. 0 to use non-realtime signals. Note performance in this use case is abysmal.

// Need to decide which kind of POSIX AIO to use
#ifndef WIN32
// Right now the only thing we support is POSIX AIO
#if !defined(BOOST_AFIO_USE_POSIX_AIO)
#define BOOST_AFIO_USE_POSIX_AIO 1
#endif
// BSD kqueues not implemented yet
//# if defined(__FreeBSD__) && !defined(BOOST_AFIO_COMPILE_KQUEUES)
//#  define BOOST_AFIO_COMPILE_KQUEUES 1
//# endif
#if BOOST_AFIO_COMPILE_KQUEUES
#if defined(BOOST_AFIO_USE_POSIX_AIO) && !BOOST_AFIO_USE_POSIX_AIO
#error BSD kqueues must be combined with POSIX AIO!
#endif
#if !defined(BOOST_AFIO_USE_POSIX_AIO)
#define BOOST_AFIO_USE_POSIX_AIO 1
#endif
#endif

#if BOOST_AFIO_USE_POSIX_AIO
// We'll be using POSIX AIO and signal based interruption for post()
#include <signal.h>
// Do we have realtime signals?
#if !defined(BOOST_AFIO_HAVE_REALTIME_SIGNALS) && defined(_POSIX_RTSIG_MAX) && defined(SIGRTMIN)
#ifndef BOOST_AFIO_IO_POST_SIGNAL
#define BOOST_AFIO_IO_POST_SIGNAL -1
#endif
#define BOOST_AFIO_HAVE_REALTIME_SIGNALS 1
#else
#ifndef BOOST_AFIO_IO_POST_SIGNAL
#define BOOST_AFIO_IO_POST_SIGNAL (SIGUSR1)
#endif
#define BOOST_AFIO_HAVE_REALTIME_SIGNALS 0
#endif
struct aiocb;
#endif
#endif

BOOST_AFIO_V2_NAMESPACE_BEGIN

class io_service;
class async_file_handle;

/*! \class io_service
\brief An asynchronous i/o multiplexer service.
*/
class BOOST_AFIO_DECL io_service
{
  friend class async_file_handle;

public:
  //! The file extent type used by this i/o service
  using extent_type = io_handle::extent_type;
  //! The memory extent type used by this i/o service
  using size_type = io_handle::size_type;
  //! The scatter buffer type used by this i/o service
  using buffer_type = io_handle::buffer_type;
  //! The gather buffer type used by this i/o service
  using const_buffer_type = io_handle::const_buffer_type;
  //! The scatter buffers type used by this i/o service
  using buffers_type = io_handle::buffers_type;
  //! The gather buffers type used by this i/o service
  using const_buffers_type = io_handle::const_buffers_type;
  //! The i/o request type used by this i/o service
  template <class T> using io_request = io_handle::io_request<T>;
  //! The i/o result type used by this i/o service
  template <class T> using io_result = io_handle::io_result<T>;

private:
#ifdef WIN32
  win::handle _threadh;
  win::dword _threadid;
#else
  pthread_t _threadh;
#endif
  stl11::mutex _posts_lock;
  struct post_info
  {
    io_service *service;
    detail::function_ptr<void(io_service *)> f;
    post_info(io_service *s, detail::function_ptr<void(io_service *)> _f)
        : service(s)
        , f(std::move(_f))
    {
    }
  };
  std::deque<post_info> _posts;
  using shared_size_type = std::atomic<size_type>;
  shared_size_type _work_queued;
#if BOOST_AFIO_USE_POSIX_AIO
  bool _use_kqueues;
#if BOOST_AFIO_COMPILE_KQUEUES
  int _kqueueh;
#endif
  std::vector<struct aiocb *> _aiocbsv;  // for fast aio_suspend()
#endif
public:
  // LOCK MUST BE HELD ON ENTRY!
  void __post_done(post_info *pi)
  {
    // Find the post_info and remove it
    for(auto &i : _posts)
    {
      if(&i == pi)
      {
        i.f.reset();
        i.service = nullptr;
        pi = nullptr;
        break;
      }
    }
    assert(!pi);
    if(pi)
      abort();
    _work_done();
    while(!_posts.front().service)
      _posts.pop_front();
  }
  void _post_done(post_info *pi)
  {
    std::lock_guard<decltype(_posts_lock)> g(_posts_lock);
    return __post_done(pi);
  }
  void _work_enqueued(size_type i = 1) { _work_queued += i; }
  void _work_done() { --_work_queued; }
  /*! Creates an i/o service for the calling thread, installing a
  global signal handler via set_interruption_signal() if not yet installed
  if on POSIX and BSD kqueues not in use.
  */
  BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC io_service();
  io_service(io_service &&) = delete;
  io_service &operator=(io_service &&) = delete;
  BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC ~io_service();

#ifdef BOOST_AFIO_IO_POST_SIGNAL
private:
  int _blocked_interrupt_signal;
  std::atomic<bool> _need_signal;  // false = signal not needed, true = signal needed
  void _block_interruption() noexcept;
  void _unblock_interruption() noexcept;

public:
  /*! Returns the signal used for interrupting run_until(). Only used on POSIX when
  BSD kqueues are not used. Defaults to BOOST_AFIO_IO_POST_SIGNAL on platforms which use it.

  \note Only present if BOOST_AFIO_IO_POST_SIGNAL is defined.
  */
  static int interruption_signal() noexcept;
  /*! Sets the signal used for interrupting run_until(), returning the former signal
  setting. Only used on POSIX when BSD kqueues are not used. Special values are
  0 for deinstall global signal handler, and -1 for install to first unused signal
  between SIGRTMIN and SIGRTMAX. Changing this while any io_service instances exist
  is a bad idea.

  \note Only present if BOOST_AFIO_IO_POST_SIGNAL is defined.
  */
  static BOOST_AFIO_HEADERS_ONLY_FUNC_SPEC int set_interruption_signal(int sig = BOOST_AFIO_IO_POST_SIGNAL);
#endif

#if BOOST_AFIO_USE_POSIX_AIO
  //! True if this i/o service is using BSD kqueues
  bool using_kqueues() const noexcept { return _use_kqueues; }
  //! Force disable any use of BSD kqueues
  BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void disable_kqueues();
#endif

  /*! Runs the i/o service for the thread owning this i/o service. Returns true if more
  work remains and we just handled an i/o or post; false if there is no more work; ETIMEDOUT if
  the deadline passed; EOPNOTSUPP if you try to call it from a non-owning thread; EINVAL
  if deadline is invalid.
  */
  BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC result<bool> run_until(deadline d) noexcept;
  //! \overload
  result<bool> run() noexcept { return run_until(deadline()); }

private:
  BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void post(detail::function_ptr<void(io_service *)> &&f);

public:
  /*! Schedule the callable to be invoked by the thread owning this object at its next
  available opportunity. Unlike any other function in this API layer, this function is thread safe.
  */
  template <class U> void post(U &&f) { _post(detail::make_function_ptr<void(io_service *)>(std::forward<U>(f))); }
};

BOOST_AFIO_V2_NAMESPACE_END

#if BOOST_AFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define BOOST_AFIO_INCLUDED_BY_HEADER 1
#ifdef WIN32
#include "detail/impl/windows/io_service.ipp"
#else
#include "detail/impl/posix/io_service.ipp"
#endif
#undef BOOST_AFIO_INCLUDED_BY_HEADER
#endif

#endif
