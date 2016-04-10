/* shared_timed_mutex.hpp
Protect a shared filing system resource from concurrent modification
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: March 2016


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

#ifndef BOOST_AFIO_SHARED_FS_MUTEX_BASE_HPP
#define BOOST_AFIO_SHARED_FS_MUTEX_BASE_HPP

#include "../../handle.hpp"
#include "../../utils.hpp"

BOOST_AFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  //! Algorithms for protecting a shared filing system resource from racy modification
  namespace shared_fs_mutex
  {
    //! Unsigned 64 bit integer
    using uint64 = unsigned long long;
    //! Unsigned 128 bit integer
    using uint128 = utils::uint128;

    /*! \class shared_fs_mutex
    \brief Abstract base class for an object which protects shared filing system resources

    The implementations of this abstract base class have various pros and cons with varying
    time and space complexities. See their documentation for details. All share the concept
    of "entity_type" as being a unique 63 bit identifier of a lockable entity. Various
    conversion functions are provided below for converting strings, buffers etc. into an
    entity_type.
    */
    class shared_fs_mutex
    {
    public:
      //! The type of an entity id
      struct entity_type
      {
        //! The type backing the value
        using value_type = handle::extent_type;
        //! The value of the entity type which can range between 0 and (2^63)-1
        value_type value : 63;
        //! True if entity should be locked for exclusive access
        value_type exclusive : 1;
        //! Default constructor
        constexpr entity_type()
            : value(0)
            , exclusive(0)
        {
        }
        //! Constructor
        constexpr entity_type(value_type _value, bool _exclusive)
            : value(_value)
            , exclusive(_exclusive)
        {
        }
      };
      //! The type of a sequence of entities
      using entities_type = span<entity_type>;

    protected:
      BOOST_CXX14_CONSTEXPR shared_fs_mutex() {}

    public:
      virtual ~shared_fs_mutex() {}

      //! Generates an entity id from a sequence of bytes
      entity_type entity_from_buffer(const char *buffer, size_t bytes, bool exclusive = true) noexcept
      {
        uint128 hash = utils::fast_hash::hash(buffer, bytes);
        return entity_type(hash.as_longlongs[0] ^ hash.as_longlongs[1], exclusive);
      }
      //! Generates an entity id from a string
      template <typename T> entity_type entity_from_string(const std::basic_string<T> &str, bool exclusive = true) noexcept
      {
        uint128 hash = utils::fast_hash::hash(str);
        return entity_type(hash.as_longlongs[0] ^ hash.as_longlongs[1], exclusive);
      }
      //! Generates a cryptographically random entity id.
      entity_type random_entity(bool exclusive = true) noexcept
      {
        entity_type::value_type v;
        utils::random_fill((char *) &v, sizeof(v));
        return entity_type(v, exclusive);
      }
      //! Fills a sequence of entity ids with cryptographic randomness. Much faster than calling random_entity() individually.
      void fill_random_entities(span<entity_type> seq, bool exclusive = true) noexcept
      {
        utils::random_fill((char *) seq.data(), seq.size() * sizeof(entity_type));
        for(auto &i : seq)
          i.exclusive = exclusive;
      }

      //! RAII holder for a lock on a sequence of entities
      class entities_guard
      {
        entity_type _entity;

      public:
        shared_fs_mutex *parent;
        entities_type entities;
        unsigned long long hint;
        entities_guard() = default;
        entities_guard(shared_fs_mutex *_parent, entities_type _entities)
            : parent(_parent)
            , entities(_entities)
            , hint(0)
        {
        }
        entities_guard(shared_fs_mutex *_parent, entity_type entity)
            : _entity(entity)
            , parent(_parent)
            , entities(&_entity, 1)
            , hint(0)
        {
        }
        entities_guard(const entities_guard &) = delete;
        entities_guard &operator=(const entities_guard &) = delete;
        entities_guard(entities_guard &&o) noexcept : _entity(std::move(o._entity)), parent(o.parent), entities(std::move(o.entities)), hint(o.hint) { o.release(); }
        entities_guard &operator=(entities_guard &&o) noexcept
        {
          this->~entities_guard();
          new(this) entities_guard(std::move(o));
          return *this;
        }
        ~entities_guard()
        {
          if(parent)
            unlock();
        }
        //! True if extent guard is valid
        explicit operator bool() const noexcept { return parent != nullptr; }
        //! True if extent guard is invalid
        bool operator!() const noexcept { return parent == nullptr; }
        //! Unlocks the locked entities immediately
        void unlock() noexcept
        {
          if(parent)
          {
            parent->unlock(entities, hint);
            release();
          }
        }
        //! Detach this RAII unlocker from the locked state
        void release() noexcept
        {
          parent = nullptr;
          entities = entities_type();
        }
      };

    protected:
      virtual result<void> _lock(entities_guard &out, deadline d, bool spin_not_sleep) noexcept = 0;

    public:
      //! Lock all of a sequence of entities for exclusive or shared access
      result<entities_guard> lock(entities_type entities, deadline d = deadline(), bool spin_not_sleep = false) noexcept
      {
        entities_guard ret(this, std::move(entities));
        BOOST_OUTCOME_PROPAGATE_ERROR(_lock(ret, std::move(d), spin_not_sleep));
        return std::move(ret);
      }
      //! Lock a single entity for exclusive or shared access
      result<entities_guard> lock(entity_type entity, deadline d = deadline(), bool spin_not_sleep = false) noexcept
      {
        entities_guard ret(this, entity);
        return lock(ret.entities, std::move(d), spin_not_sleep);
      }
      //! Try to lock all of a sequence of entities for exclusive or shared access
      result<entities_guard> try_lock(entities_type entities) noexcept { return lock(std::move(entities), deadline(std::chrono::seconds(0))); }
      //! Try to lock a single entity for exclusive or shared access
      result<entities_guard> try_lock(entity_type entity) noexcept
      {
        entities_guard ret(this, entity);
        return try_lock(ret.entities);
      }
      //! Unlock a previously locked sequence of entities
      virtual void unlock(entities_type entities, unsigned long long hint = 0) noexcept = 0;
    };

  }  // namespace
}  // namespace

BOOST_AFIO_V2_NAMESPACE_END


#endif
