/* file_handle.hpp
A handle to a file
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

#include "handle.hpp"

#ifndef BOOST_AFIO_FILE_HANDLE_H
#define BOOST_AFIO_FILE_HANDLE_H

BOOST_AFIO_V2_NAMESPACE_BEGIN

class io_service;

/*! \class file_handle
\brief A handle to a regular file or device, kept data layout compatible with
async_file_handle.
*/
class BOOST_AFIO_DECL file_handle : public io_handle
{
public:
  using path_type = io_handle::path_type;
  using extent_type = io_handle::extent_type;
  using size_type = io_handle::size_type;
  using mode = io_handle::mode;
  using creation = io_handle::creation;
  using caching = io_handle::caching;
  using flag = io_handle::flag;
  using buffer_type = io_handle::buffer_type;
  using const_buffer_type = io_handle::const_buffer_type;
  using buffers_type = io_handle::buffers_type;
  using const_buffers_type = io_handle::const_buffers_type;
  template <class T> using io_request = io_handle::io_request<T>;
  template <class T> using io_result = io_handle::io_result<T>;

protected:
  path_type _path;
  io_service *_service;

public:
  //! Default constructor
  file_handle()
      : io_handle()
      , _service(nullptr)
  {
  }
  //! Construct a handle from a supplied native handle
  file_handle(path_type path, native_handle_type h, caching caching = caching::none, flag flags = flag::none)
      : io_handle(std::move(h), std::move(caching), std::move(flags))
      , _path(std::move(path))
      , _service(nullptr)
  {
  }
  //! Implicit move construction of file_handle permitted
  file_handle(file_handle &&o) noexcept : io_handle(std::move(o)), _path(std::move(o._path)), _service(o._service) { o._service = nullptr; }
  //! Explicit conversion from handle and io_handle permitted
  explicit file_handle(handle &&o, path_type path) noexcept : io_handle(std::move(o)), _path(std::move(path)), _service(nullptr) {}
  using io_handle::really_copy;
  //! Copy the handle. Tag enabled because copying handles is expensive (fd duplication).
  explicit file_handle(const file_handle &o, really_copy _)
      : io_handle(o, _)
      , _path(o._path)
      , _service(o._service)
  {
  }
  //! Move assignment of file_handle permitted
  file_handle &operator=(file_handle &&o) noexcept
  {
    this->~file_handle();
    new(this) file_handle(std::move(o));
    return *this;
  }
  //! Swap with another instance
  void swap(file_handle &o) noexcept
  {
    file_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  /*! Create a file handle opening access to a file on path

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  //[[bindlib::make_free]]
  static BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<file_handle> file(path_type _path, mode _mode = mode::read, creation _creation = creation::open_existing, caching _caching = caching::all, flag flags = flag::none) noexcept;

  /*! Clone this handle (copy constructor is disabled to avoid accidental copying)

  \errors Any of the values POSIX dup() or DuplicateHandle() can return.
  */
  BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC result<file_handle> clone() const noexcept;

  BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC path_type path() const noexcept override { return _path; }
  //! The i/o service this handle is attached to
  io_service *service() const noexcept { return _service; }

  /*! Return the current maximum permitted extent of the file.

  \errors Any of the values POSIX fstat() or GetFileInformationByHandleEx() can return.
  */
  result<extent_type> length() const noexcept;

  /*! Resize the current maximum permitted extent of the file to the given extent, avoiding any
  new allocation of physical storage where supported. Note that on extents based filing systems
  this will succeed even if there is insufficient free space on the storage medium.

  \errors Any of the values POSIX ftruncate() or SetFileInformationByHandle() can return.
  */
  //[[bindlib::make_free]]
  result<extent_type> truncate(extent_type newsize) noexcept;
};

BOOST_AFIO_V2_NAMESPACE_END

#if BOOST_AFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define BOOST_AFIO_INCLUDED_BY_HEADER 1
#ifdef WIN32
#include "detail/impl/windows/file_handle.ipp"
#else
#include "detail/impl/posix/file_handle.ipp"
#endif
#undef BOOST_AFIO_INCLUDED_BY_HEADER
#endif

#endif
