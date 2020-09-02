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

namespace HORIZON::CALLABLE
{
    // forward declarations of all involved classes

    struct CallbackHandle;

    /*!
     * The base callback manager.
     * This class is not intended for direct usage.
     * Using this as the common base class allows the Callback-Handle to store a managed instance of callbackManger.
     *
     * In addition, this class acts as an interface/base implementation for the actual callback manager.
     */
    class __BaseCallbackManager : public std::enable_shared_from_this<__BaseCallbackManager>
    {
        // allow CallbackHandle to call
        friend struct CallbackHandle;

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
         * Unregisters a handle.
         * Note: The associated manager of the handle need not be this one.
         *
         * After calling this method, the handle is no longer registered!
         *
         * @param handle    The handle to unregister.
         */
        static void Unregister(CallbackHandle& handle) noexcept;

    protected:
        /*!
         * Creates a new BaseCallbackManager and initialises the identifier for this callbackManager instance.
         */
        __BaseCallbackManager();

        /*!
         * Creates the next available handle and returns it.
         * NOTE: The handle must be moved, it cannot be copied!
         * @return
         */
        CallbackHandle GetNextHandle() noexcept;

        /*!
         * The Unregister callback for the CallbackHandle.
         * It is guaranteed that the handle is associated with this manager instance.
         *
         * @param id The ID of the handle.
         */
        virtual void Unregister(Identifier const& id) noexcept = 0;
    };

    /*!
     * The lifetime managing object of a callback.
     * After creating a callback, a handle will be returned. As long as the handle exists, the callback is guaranteed to exist.
     *
     * Using an object for managing the lifetime of callbacks allows unregistering said callbacks once the associated object is destroyed.
     */
    struct CallbackHandle
    {
        friend class __BaseCallbackManager;

    public:
        using Identifier = __BaseCallbackManager::Identifier;

    protected:
        using Manager = std::shared_ptr<__BaseCallbackManager>;

        // do not mark id as const since it will prevent move assignment
        Identifier _id;
        Manager    _manager;

    protected:
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
        NoCopy     (CallbackHandle)
        // enable move operations
        DefaultMove(CallbackHandle)

        /*!
         * Returns true iff the handle references a valid callback at a manager.
         */
        [[nodiscard]
        ]

        inline bool IsRegistered() const noexcept
        { return (bool) _manager; }

        /*!
         * Returns the identifier of the handle.
         */
        [[nodiscard]] inline Identifier const& GetIdentifier() const noexcept
        { return _id; }

        /*!
         * Calls Unregister to make sure that the handle is unregistered from the callback manager
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


    template<class Signature>
    class CallbackManager;

    /*!
     * Generic implementation for callback storage managing.
     * This base class is used in every specialised CallbackManager
     * @tparam Result   Result type of the callback
     * @tparam Args     Argument types of the callback
     */
    template<class Result,
             class... Args>
    class __GenericCallbackManager : public __BaseCallbackManager
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
        __GenericCallbackManager() = default;

        /*!
         * Adds a callback to the maanger.
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
            return std::move(handle);
        }

        /*!
         * Calls the callbacks but ignores the results.
         *
         * In case of CallbackManager<void(...)> this is the same as calling operator().
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

    // specialised implementation for the typed-return case
    template<class Result,
             class ... Args>
    class CallbackManager<Result(Args...)> : public __GenericCallbackManager<Result, Args...>
    {
    private:
        struct PassKey
        {
        };

    public:
        static std::shared_ptr<CallbackManager<Result(Args...)>> Create()
        { return std::make_shared<CallbackManager<Result(Args...)>>(PassKey{ }); }

        /*!
         * Constructor requiring pass-key idiom
         */
        explicit CallbackManager(PassKey const&)
        { };

        /*!
       * Calls all registered callbacks and returns their result.
       * There is no guarantee about the call-order.
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
         * Same as CallbackManager.operator().
         *
         * Calls all registered callbacks and returns their result.
         * There is no guarantee about the call-order.
         *
         * If there is no regard to the result of the callback, use CallNoResult(...).
         *
         * @return  The results of the callbacks.
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
    template<class ... Args>
    class CallbackManager<void(Args...)> : public __GenericCallbackManager<void, Args...>
    {
    private:
        struct PassKey
        {
        };

    public:
        static std::shared_ptr<CallbackManager<void(Args...)>> Create()
        { return std::make_shared<CallbackManager<void(Args...)>>(PassKey{ }); }

        /*!
         * Constructor requiring pass-key idiom
         */
        explicit CallbackManager(PassKey const&)
        { };

        /*!
         * Calls all registered callbacks.
         * There is no guarantee about the call-order.
         *
         * This method is equivalent to CallbackManager::CallNoResult(...);
         *
         * @return  The results of the callbacks.
         */
        inline constexpr void operator()(Args&& ... args) const
        { return this->CallNoResult(std::forward<Args>(args)...); }

        /*!
         * Same as CallbackManager.operator().
         *
         * Calls all registered callbacks and returns their result.
         * There is no guarantee about the call-order.
         *
         * If there is no regard to the result of the callback, use CallNoResult(...).
         *
         * @return  The results of the callbacks.
         */
        inline constexpr void Call(Args&& ... args) const
        { return this->operator()(std::forward<Args>(args)...); }

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