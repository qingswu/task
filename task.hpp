//
// dsa is a utility library of data structures and algorithms built with C++11.
// This file (task.hpp) is part of the dsa project.
//
// author: Dalton Woodard
// contact: daltonmwoodard@gmail.com
// repository: https://github.com/daltonwoodard/task.git
// license:
//
// Copyright (c) 2016 DaltonWoodard. See the COPYRIGHT.md file at the top-level
// directory or at the listed source repository for details.
//
//      Licensed under the Apache License. Version 2.0:
//          https://www.apache.org/licenses/LICENSE-2.0
//      or the MIT License:
//          https://opensource.org/licenses/MIT
//      at the licensee's option. This file may not be copied, modified, or
//      distributed except according to those terms.
//

#ifndef DSA_TASK_HPP
#define DSA_TASK_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <forward_list>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>
#include "utilities/functions.hpp"
#include "utilities/sequence.hpp"


namespace dsa
{
    /*
     * task; a type-erased, allocator-aware std::packaged_task that also
     * contains its own arguments. The underlying packaged_task and the stored
     * argument tuple can be heap allocated or allocated with a provided
     * allocator.
     *
     * There is a single helper method for creating task objects: make_task,
     * which returns a pair of the newly constructed task and a std::future
     * object to the return value.
     */
    class task
    {
    public:
        task (void) = default;
        ~task (void) = default;

        task (task const &) = delete;
        task (task &&) noexcept = default;

        task & operator= (task const &) = delete;
        task & operator= (task &&) noexcept = default;

        void swap (task & other) noexcept
        {
            std::swap (this->_t, other._t);
        }

        operator bool (void) const noexcept
        {
            return static_cast <bool> (this->_t);
        }

        template <class>
        friend class task_system;

        template <class F, class ... Args>
        friend std::pair <
            task,  std::future <typename std::result_of <F (Args...)>::type>
        > make_task (F && f, Args && ... args)
        {
            using pair_type = std::pair <
                task, std::future <typename std::result_of <F (Args...)>::type>
            >;
            using model_type = task_model <
                typename std::result_of <F (Args...)>::type (Args...)
            >;

            task t (std::forward <F> (f), std::forward <Args> (args)...);
            auto fut = dynamic_cast <model_type &> (*t._t).get_future ();
            return pair_type (std::move (t), std::move (fut));
        }

        template <class Allocator, class F, class ... Args>
        friend std::pair <
            task, std::future <typename std::result_of <F (Args...)>::type>
        > make_task (std::allocator_arg_t, Allocator const & alloc,
                     F && f, Args && ... args)
        {
            using pair_type = std::pair <
                task, std::future <typename std::result_of <F (Args...)>::type>
            >;
            using model_type = task_model <
                typename std::result_of <F (Args...)>::type (Args...)
            >;

            task t (
                std::allocator_arg_t (), alloc,
                std::forward <F> (f), std::forward <Args> (args)...
            );
            auto fut = dynamic_cast <model_type &> (*t._t).get_future ();
            return pair_type (std::move (t), std::move (fut));
        }

        void operator() (void)
        {
            if (this->_t)
                this->_t->invoke_ ();
            else
                throw std::logic_error ("bad task access");
        }

    private:
        template <class F, class ... Args>
        task (F && f, Args && ... args)
            : _t (
                new task_model <
                    typename std::result_of <F (Args...)>::type (Args...)
                > (std::forward <F> (f), std::forward <Args> (args)...)
            )
        {}

        template <class Allocator, class F, class ... Args>
        task (std::allocator_arg_t,
              Allocator const & alloc,
              F && f, Args && ... args)
            : _t (
                new task_model <
                    typename std::result_of <F (Args...)>::type (Args...)
                > (std::allocator_arg_t (), alloc,
                   std::forward <F> (f), std::forward <Args> (args)...)
            )
        {}

        struct task_concept
        {
            virtual ~task_concept (void) noexcept {}
            virtual void invoke_ (void) = 0;
        };

        template <class> struct task_model;

        /*
         * tasks are assumed to be immediately invokable; that is,
         * invoking the underlying pakcaged_task with the provided arguments
         * will not block.
         */
        template <class R, class ... Args>
        struct task_model <R (Args...)> : task_concept
        {
            template <class F>
            explicit task_model (F && f, Args && ... args)
                : _f    (std::forward <F> (f))
                , _args (std::forward <Args> (args)...)
            {}

            template <class Allocator, class F>
            explicit task_model (
                std::allocator_arg_t, Allocator const & alloc,
                F && f, Args && ... args
            )
                : _f    (std::allocator_arg_t (), alloc, std::forward <F> (f))
                , _args (std::allocator_arg_t (), alloc,
                         std::forward <Args> (args)...)
            {}

            std::future <R> get_future (void)
            {
                return this->_f.get_future ();
            }

            void invoke_ (void) override
            {
                utility::apply (this->_f, this->_args);
            }

        private:
            std::packaged_task <R (Args...)> _f;
            std::tuple <Args...> _args;
        };

        std::unique_ptr <task_concept> _t;
    };

    /*
     * task_system; a work-stealing tasking system partly inspired by Sean
     * Parent's "Better Code: Concurrency" talk; see http://sean-parent.stlab.cc
     */
    template <class Allocator = std::allocator <task>>
    class task_system
    {
        class task_queue
        {
            std::queue <task> tasks_;
            std::condition_variable cv_;
            std::mutex mutex_;
            std::atomic_bool done_ {false};

        public:
            task_queue (void)
                : tasks_ {}
            {}

            task_queue (task_queue const &) = delete;

            task_queue (task_queue && other) noexcept
                : tasks_ (std::move (other).tasks_)
                , done_  (other.done_.load ())
            {}

            void set_done (void)
            {
                this->done_.store (true);
                this->cv_.notify_all ();
            }

            std::pair <bool, task> try_pop (void)
            {
                std::unique_lock <std::mutex>
                    lock (this->mutex_, std::try_to_lock);
                if (!lock || this->tasks_.empty ()) {
                    return std::make_pair (false, task {});
                } else {
                    auto t = std::move (this->tasks_.front ());
                    this->tasks_.pop ();
                    return std::make_pair (true, std::move (t));
                }
            }

            bool try_push (task & t)
            {
                {
                    std::unique_lock <std::mutex>
                        lock (this->mutex_, std::try_to_lock);
                    if (!lock)
                        return false;

                    this->tasks_.emplace (std::move (t));
                }

                this->cv_.notify_one ();
                return true;
            }

            std::pair <bool, task> pop (void)
            {
                std::unique_lock <std::mutex> lock (this->mutex_);
                while (this->tasks_.empty () && !this->done_)
                    this->cv_.wait (lock);

                if (this->tasks_.empty ())
                    return std::make_pair (false, task {});

                auto t = std::move (this->tasks_.front ());
                this->tasks_.pop ();
                return std::make_pair (true, std::move (t));
            }

            void push (task t)
            {
                {
                    std::unique_lock <std::mutex> lock (this->mutex_);
                    this->tasks_.emplace (std::move (t));
                }
                this->cv_.notify_one ();
            }
        };

        std::vector <task_queue> queues_;
        std::vector <std::thread> threads_;
        std::vector <bool> exited_;
        typename Allocator::template rebind <task::task_concept>::other
            alloc_;
        std::size_t nthreads_;
        std::size_t current_index_ {0};
        std::atomic_size_t queued_ {0};

        void run (std::size_t id)
        {
            while (true) {
                std::pair <bool, task> p;

                for (std::size_t k = 0; k < 10 * this->nthreads_; ++k) {
                    p = this->queues_ [(id + k) % this->nthreads_].try_pop ();
                    if (p.first) {
                        this->queued_--;
                        goto call;
                    }
                }

                p = this->queues_ [id].pop ();
                if (p.first) {
                    this->queued_--;
                    goto call;
                } else {
                    goto finish;
                }

            call:
                p.second ();
                continue;
            }

        finish:
            /*
             * The done signal has been set and our queue is empty, but there
             * may still be queued tasks that must be completed, so try and
             * steal work from other queues before exiting.
             */
            while (this->queued_) {
                std::pair <bool, task> p;

                for (std::size_t k = 0; k < this->nthreads_; ++k) {
                    p = this->queues_ [(id + k) % this->nthreads_].try_pop ();
                    if (p.first) {
                        this->queued_--;
                        p.second ();
                    }
                }

                std::this_thread::yield ();
            }

            this->exited_ [id] = true;
        }

        void join (void)
        {
            this->done ();
            for (auto & th : this->threads_)
                th.join ();
        }

    public:
        task_system (void)
            : task_system (std::thread::hardware_concurrency ())
        {}

        task_system (std::size_t nthreads,
                           Allocator const & alloc = Allocator ())
            : queues_   {}
            , threads_  {}
            , exited_   (nthreads, false)
            , alloc_    (alloc)
            , nthreads_ {nthreads}
        {
            this->queues_.reserve (nthreads);
            for (std::size_t th = 0; th < nthreads; ++th)
                this->queues_.emplace_back ();

            this->threads_.reserve (nthreads);
            for (std::size_t th = 0; th < nthreads; ++th)
                this->threads_.emplace_back (
                    &task_system::run, this, th
                );
        }

        ~task_system (void)
        {
            this->join ();
        }

        void done (void)
        {
            for (auto & q : this->queues_)
                q.set_done ();
        }

        void wait_to_completion (void)
        {
            while (true) {
                std::this_thread::yield ();

                if (this->queued_) {
                    continue;
                } else {
                    auto should_exit = true;
                    for (auto b : this->exited_)
                        should_exit &= b;
                    if (should_exit)
                        return;
                }
            }
        }

        void reset (void)
        {
            this->join ();
            this->threads_.clear ();
            this->queues_.clear ();
            this->queued_.store (0);

            for (std::size_t th = 0; th < this->nthreads_; ++th)
                this->queues_.emplace_back ();

            for (std::size_t th = 0; th < this->nthreads_; ++th)
                this->threads_.emplace_back (
                    &task_system::run, this, th
                );
        }

        template <class F, class ... Args>
        auto push (F && f, Args && ... args)
            -> typename std::remove_reference <
                decltype (make_task (
                    std::allocator_arg_t {}, this->alloc_,
                    std::forward <F> (f), std::forward <Args> (args)...
                ).second)
            >::type
        {
            auto t = make_task (
                std::allocator_arg_t {}, this->alloc_,
                std::forward <F> (f), std::forward <Args> (args)...
            );

            auto const idx = this->current_index_++;
            for (std::size_t k = 0; k < 10 * this->nthreads_; ++k) {
                /*
                 * In order to maintain consistency we need to speculatively
                 * incremement the queued count and then decrement only if
                 * the try_push call failed. This is because the queued count
                 * must be incremented before a push and decremented only after
                 * a pop.
                 */
                this->queued_++;
                if (this->queues_ [(idx + k) % this->nthreads_]
                        .try_push (t.first)) {
                    return std::move (t.second);
                } else {
                    this->queued_--;
                }
            }

            this->queued_++;
            this->queues_ [idx % this->nthreads_].push (std::move (t.first));
            return std::move (t.second);
        }

        void push (task && t)
        {
            auto const idx = this->current_index_++;
            for (std::size_t k = 0; k < 10 * this->nthreads_; ++k) {
                /*
                 * In order to maintain consistency we need to speculatively
                 * incremement the queued count and then decrement only if
                 * the try_push call failed. This is because the queued count
                 * must be incremented before a push and decremented only after
                 * a pop.
                 */
                this->queued_++;
                if (this->queues_ [(idx + k) % this->nthreads_].try_push (t)) {
                    return;
                } else {
                    this->queued_--;
                }
            }

            this->queued_++;
            this->queues_ [idx % this->nthreads_].push (std::move (t));
        }
    };
}   // namespace dsa

#endif  // #ifndef DSA_TASK_HPP
