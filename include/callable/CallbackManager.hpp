//
// @brief   
// @details 
// @author  Steffen Peikert (ch3ll)
// @email   Horizon@ch3ll.com
// @version 1.0.0
// @date    21/07/2020 12:46
// @project Horizon
//


#pragma once


#include <preprocessor/Class.hpp>

#include <functional>
#include <memory>
#include <utility>
#include <cassert>
#include <stdexcept>
#include <vector>
#include <algorithm>

namespace HORIZON::CALLABLE
{
    // forward declarations of all involved classes
    struct CallbackHandle;

    namespace INTERNAL
    {
        /*!
         * @brief The base callback manager.
         *
         * @details This class is not intended for direct usage.
         * Using this as the common base class allows the Callback-Handle to store a managed instance of callbackManger.
         *
         * In addition, this class acts as an interface/base implementation for the actual callback manager.
         */
        class BaseCallbackManager : public std::enable_shared_from_this<BaseCallbackManager>
        {
            // allow CallbackHandle to call
            friend struct HORIZON::CALLABLE::CallbackHandle;

        public:
            // TODO: expose identifier? Requisits to Identifier:
            //  - Empty constructible
            //  - Incrementable to a new, unique id (operator++()) (or at least a substantial amount of unique ids)
            //  - Comparable (operator==(Identifier const& other), preferably fast, since it has impact on removal costs
            /*!
             * The internal identifier type used to separate each callbacks.
             */
            using Identifier = size_t;

        private:
            Identifier _handleIdentifier;

        public:
            /*!
             * @brief Unregisters a handle.
             * @details After calling this method, the handle is no longer registered!
             *
             * @note The associated manager of the handle need not be this one.
             *
             * @param handle    The handle to unregister.
             */
            static void Unregister(CallbackHandle& handle) noexcept;

        protected:
            /*!
             * Creates a new BaseCallbackManager and initialises the identifier for this callbackManager instance.
             */
            BaseCallbackManager();

            /*!
             * @brief Creates the next available handle and returns it.
             * @warning If you pass the handle, you must move it. If you create a copy, the handle is destroyed and unregisters itself!
             * @return  The handle
             */
            CallbackHandle GetNextHandle() noexcept;

            /*!
             * @brief Unregisters a callback with a given id.
             * @details The Unregister callback for the CallbackHandle.
             * It is guaranteed that the handle is associated with this manager instance.
             *
             * @param id The ID of the handle.
             */
            virtual void Unregister(Identifier const& id) noexcept = 0;
        };
    }


    /*!
     * @ingroup group_module_callable
     *
     * @details The lifetime managing object of a callback.
     * After creating a callback, a handle will be returned. As long as the handle exists, the callback is guaranteed to exist. A callback handle
     * keeps a reference to its corresponding callback manager. If a registered handle is destroyed, it will self-unregister from the callback
     * manager. Because of this behaviour, a CallbackHandle can only be passed by move operations.
     *
     * Using an object for managing the lifetime of callbacks allows unregistering said callbacks once the associated object is destroyed.
     */
    struct CallbackHandle
    {
        friend class HORIZON::CALLABLE::INTERNAL::BaseCallbackManager;

    public:
        using Identifier = INTERNAL::BaseCallbackManager::Identifier;

    private:
        using Manager = std::shared_ptr<INTERNAL::BaseCallbackManager>;

        // do not mark id as const since it will prevent move assignment
        Identifier _id;
        Manager    _manager;

    private:
        /*!
         * Creates a new handle. This method can only be accessed by the callback manager.
         *
         * @param id        The handle id.
         * @param manager   The associated callback manager
         */
        explicit CallbackHandle(Identifier const& id, Manager manager) :
                _id(id),
                _manager(std::move(manager))
        { }

    public:
        // make sure that there cannot be any copies of this object
        NoCopy(CallbackHandle);
        // enable move operations
        DefaultMove(CallbackHandle);

        /*!
         * @return true iff the handle references a valid callback at a manager.
         */
        [[nodiscard]]inline bool IsRegistered() const noexcept
        { return (bool) _manager; }

        /*!
         * @return The identifier of the handle.
         */
        [[nodiscard]] inline Identifier const& GetIdentifier() const noexcept
        { return _id; }

        /*!
         * @brief Auto-unregistering the callback.
         *
         * @details Calls Unregister() to make sure that the handle is unregistered from the CallbackManager
         */
        ~CallbackHandle();

        /*!
         * Unregisters the handle from the callback manager.
         * Only the first invocation will unregister, all others are ignored.
         *
         * After calling this method, IsRegistered returns false.
         */
        void Unregister() noexcept;
    };

    // forward declaration of CallbackManager
    template<class Signature>
    class CallbackManager;

    namespace INTERNAL
    {
        /*!
         * @details Generic implementation for callback storage managing.
         * This base class is used in every specialised CallbackManager
         *
         * @tparam Result   Result type of the callback
         * @tparam Args     Argument types of the callback
         */
        template<class Result,
                 class... Args>
        class GenericCallbackManager : public INTERNAL::BaseCallbackManager
        {
        public:
            using Callback = std::function<Result(Args...)>;

        protected:
            struct CallbackEntry
            {
                Callback   _callback;
                Identifier _id;

                //TODO: is this POD?
                CallbackEntry(Callback&& callback, Identifier const& id) :
                        _callback(callback),
                        _id(id)
                { }

                bool operator==(CallbackEntry const& other) noexcept
                { return _id == other._id; }

                bool operator==(Identifier const& other) noexcept
                { return _id == other; }

                Result operator()(Args&& ...args) const
                {
                    if (_callback) return _callback(std::forward<Args>(args)...);

                    // this should not happen
                    throw std::runtime_error("No valid callback!");
                }
            };

            // TODO: make this thread safe!
            std::vector<CallbackEntry> _callbacks;


        public:
            GenericCallbackManager() = default;

            /*!
             * @brief Adds a callback to the manager.
             *
             * @param callback  The callback.
             * @return  A handle to the callback. As long as the handle exists, the callback exists.
             */
            CallbackHandle Register(Callback&& callback)
            {
                // TODO: thread safe!
                auto handle = GetNextHandle();


                // add a new callback
                _callbacks.emplace_back(std::forward<Callback>(callback), handle.GetIdentifier());

                // move the handle to the caller!
                // NOTE: do not use std::move here because of copy elision (https://stackoverflow.com/questions/19267408/why-does-stdmove-prevent-rvo)
                return handle;
            }

            /*!
             * @brief Calls the callbacks but ignores the results.
             *
             * @note In case of CallbackManager<void(...)> this is the same as calling operator().
             */
            void CallNoResult(Args&& ... args) const
            {
                for (auto const& cbf : _callbacks)
                    cbf(args...);
            }

        private:
            void Unregister(Identifier const& id) noexcept override
            {
                // WARN: This does not keep the order of callbacks in the container intact!

                // TODO: thread safe!

                // get the iterator to the callback to remove
                auto it = std::find(_callbacks.begin(), _callbacks.end(), id);

                // it must be a valid iterator
                assert(it != _callbacks.end());

                // move callback to the end of the container
                std::iter_swap(it, _callbacks.end() - 1);

                // remove last element
                _callbacks.pop_back();
            }
        };
    }

    // specialised implementation for the typed-return case
    /*!
     * @ingroup group_module_callable
     *
     * @brief Specialised Callback-manager for typed-return callbacks.
     * @tparam Result   The callback return type.
     * @tparam Args     The arguments of the callback.
     */
    template<class Result,
             class ... Args>
    class CallbackManager<Result(Args...)> : public INTERNAL::GenericCallbackManager<Result, Args...>
    {
    private:
        struct PassKey
        {
        };

    public:
        /*!
         * @return A new CallbackManager with the specified callback type.
         */
        static std::shared_ptr<CallbackManager<Result(Args...)>> Create()
        { return std::make_shared<CallbackManager<Result(Args...)>>(PassKey{ }); }

        /*!
         * @details Constructor requiring pass-key idiom. This ctor is only accessible from the static Create() method.
         * @sa Create()
         */
        explicit CallbackManager(PassKey const&)
        { };

        /*!
         * @details Calls all registered callbacks and returns their result.
         *
         * @note There is no guarantee about the call-order.
         *
         * @return  The results of the callbacks.
         */
        std::vector<Result> operator()(Args&& ... args) const
        {
            // create the result vector
            std::vector<Result> results;
            results.reserve(this->_callbacks.size());

            // call all callbacks and store the result
            int i = 0;
            for (auto& cbf : this->_callbacks)
                results[i++] = cbf(args...);

            return std::move(results);
        }

        /*!
         * @brief Executes all callbacks with the given arguments.
         * @details Calls all registered callbacks and returns their result.
         * If you do not need the result of the callback, use CallNoResult(...).
         *
         * @note There is no guarantee about the call-order.
         * @note Same as CallbackManager.operator().
         *
         * @return  The results of the callbacks.
         *
         * @sa operator(Args&& ...))
         * @sa CallNoResult(Args&& ...)
         */
        std::vector<Result> Call(Args&& ... args) const
        { return this->operator()(std::forward<Args>(args)...); }

        /*!
         * Calls the callbacks async.
         * @param args
         * @return
         */
        std::vector<Result> CallAsync(Args&& ... args) const
        {
            //TODO
            throw;
        }
    };

    // specialised implementation for the void-return case
    /*!
      * @brief Specialised Callback-manager for void-return callbacks.
      * @tparam Args     The arguments of the callback.
      */
    template<class ... Args>
    class CallbackManager<void(Args...)> : public INTERNAL::GenericCallbackManager<void, Args...>
    {
    private:
        struct PassKey
        {
        };

    public:
        /*!
         * @returns Creates a new CallbackManager.
         */
        static std::shared_ptr<CallbackManager<void(Args...)>> Create()
        { return std::make_shared<CallbackManager<void(Args...)>>(PassKey{ }); }

        /*!
         * @brief Constructor following the passkey idiom.
         * @sa Create()
         */
        explicit CallbackManager(PassKey const&)
        { };

        /*!
         * @brief Calls all registered callbacks.
         *
         * @note This method is equivalent to CallbackManager::CallNoResult(...);
         *
         * @param args The arguments of the callback.
         */
        inline constexpr void operator()(Args&& ... args) const
        { return this->CallNoResult(std::forward<Args>(args)...); }

        /*!
         *
         * @details Calls all registered callbacks.
         * If there is no regard to the result of the callback, use CallNoResult(...).
         *
         * @note This method is equivalent to CallbackManager::CallNoResult(...);
         * @note There is no guarantee about the call-order.
         */
        inline constexpr void Call(Args&& ... args) const
        { return this->CallNoResult(std::forward<Args>(args)...); }

        /*!
         * Calls the callbacks async.
         * @param args
         * @return
         */
        void CallAsync(Args&& ... args) const
        {
            //TODO
            throw;
        }
    };
}