//
// @brief
// @details
// @author  Steffen Peikert (ch3ll)
// @email   Horizon@ch3ll.com
// @version 1.0.0
// @date    25/07/2020 17:26
// @project Horizon
//


#pragma once

#include <functional>
#include <future>
#include <preprocessor/Class.hpp>

namespace HORIZON::CALLABLE
{
	/*!
	 * @ingroup group_module_callable
	 *
	 * @brief Represents generic errors that can occur during job execution.
	 */
	class JobError : public std::logic_error
	{
	public:
		/*!
		 * @brief Creates a job error with the specified error message.
		 * @param what  The error message.
		 */
		[[maybe_unused]] explicit JobError(std::string const& what) : std::logic_error(what) { }

		/*!
		 * @brief Creates a job error with the specified error message.
		 * @param what The error message.
		 */
		explicit JobError(char const* what) : std::logic_error(what) { }

		JobError(JobError const& other) noexcept = default;
	};

	namespace INTERNAL
	{
		/*!
		 * @brief The base class of all jobs.
		 *
		 * @details This class is an interface for Job. Its main use is to provide a common type for storing jobs in a
		 * container (e.g. in a threadpool implementation, see HORIZON::PARALLEL::ThreadPool). In addition, it works as
		 * a skeleton to prevent code duplication in the template-specialisation.
		 */
		class BaseJob : public std::enable_shared_from_this<BaseJob>
		{
		public:
			/*!
			 * @brief The status of the job
			 */
			enum class ExecutionStatus
			{
				/*!
				 * The job is waiting for execution.
				 */
				Waiting = 0x1,

				/*!
				 * The job is waiting for dependencies to finish.
				 */
				Pending = 0x2,

				/*!
				 * The job is currently running.
				 */
				Running = 0x3,

				/*!
				 * The job is finished and a result is available.
				 */
				Finished = 0x4,

				/*!
				 * The job is finished because an exception occurred.
				 * No result is available.
				 */
				Erroneous = 0x5
			};

			/*!
			 * The status of the job result.
			 */
			enum class ResultStatus
			{
				/*!
				 * Waiting for the result.
				 */
				Waiting,

				/*!
				 * The result is available.
				 */
				Available,

				/*!
				 * The result is no longer available because it has been moved.
				 */
				Moved
			};

		private:
			// TODO: priority?
			// TODO: dependency on other jobs? ==> Dependency upgrade for all previous jobs
			// TODO: reset/repeat job?
			// TODO: terminate job early (incl. dequeing from thread pool?)

			ResultStatus    _resultStatus;
			ExecutionStatus _executionStatus;

			std::exception _error;

			// std::vector<std::shared_ptr<BaseJob>> _dependencies;

			// lock for the task execution
			mutable std::mutex              _resultLock;
			mutable std::condition_variable _waitNotifier;


		public:
			HORIZON_NoCopy(BaseJob)

			/*!
			 * @returns Gets the execution status.
			 */
			[[maybe_unused]] [[nodiscard]] inline ExecutionStatus const& GetExecutionStatus() const noexcept
			{
				return _executionStatus;
			}

			/*!
			 * @returns Gets the result status.
			 */
			[[maybe_unused]] [[nodiscard]] inline ResultStatus const& GetResultStatus() const noexcept
			{
				return _resultStatus;
			}

			/*!
			 * @returns True iff an error occurred during execution.
			 * @see GetError()
			 */
			[[maybe_unused]] [[nodiscard]] inline bool IsErroneous() const noexcept
			{
				return _executionStatus == ExecutionStatus::Erroneous;
			}

			/*!
			 * @returns Gets the error. If no error occurred, this will be the default exception.
			 */
			[[maybe_unused]] [[nodiscard]] inline std::exception const& GetError()
				const noexcept  // this signature is a first :D
			{
				return _error;
			}

			/*!
			 * @brief Waits for the job to finish. After calling this method, a result is available.
			 *
			 * @note This blocks the calling thread.
			 *
			 * @see IsErroneous(), GetError(), GetResultStatus(), Execute(), GetExecutionStatus()
			 */
			void Wait() const
			{
				// i am not using future.wait() here, since it might misbehave in case it is invalid.
				// in addition, this thread is supposed to wait UNTIL a result is available. I need to move the result
				// to the local storage after execution, but future.wait() might return before.

				// acquire lock
				std::unique_lock lock(_resultLock);

				// early out if we are already finished
				// this must be done after the lock guard, since otherwise the thread might be running and provide a
				// result before the conditional variable is in place
				if (_executionStatus == ExecutionStatus::Finished) { return; }
				// wait for notification from task execution
				_waitNotifier.wait(lock);
			}

			/*!
			 * @brief Runs the associated task.
			 *
			 * @note This method is a convenience overload for Execute().
			 *
			 * @see Execute()
			 */
			inline void operator()() { Execute(); }

			/*!
			 * @brief Runs the associated task.
			 * @details After calling this method, a possible result is available and all waiting threads are notified.
			 *
			 * @see IsErroneous(), Wait(), GetError(), GetResultStatus()
			 */
			void Execute()
			{
				std::lock_guard lock(_resultLock);

				// check if we are allowed to run, otherwise exit
				if (_executionStatus != ExecutionStatus::Waiting) { return; }

				// update status
				_executionStatus = ExecutionStatus::Pending;

				// wait for dependencies to finish
				// for (auto& dependency : _dependencies)
				// {
				//     // TODO: priority propagation
				//     // CRITICAL: as long as there is no priority, just waiting might stall the (threadpool) pipeline.
				//     dependency->Wait();
				// }

				_executionStatus = ExecutionStatus::Running;

				// run the task
				try
				{
					// todo: this forces the actual implementation to store an additional future.
					//  maybe dont pass task to base class but instead provide a skeleton to run the packaged_task and
					//  let the implementation handle the result extraction?
					RunJob();
				}
				catch (std::exception& ex)
				{
					// store exception
					_error = ex;

					// mark execution as finished and notify waiting threads
					_executionStatus = ExecutionStatus::Erroneous;
					_waitNotifier.notify_all();
				}

				_executionStatus = ExecutionStatus::Finished;
				// notify possible waiting threads
				_waitNotifier.notify_all();
			}

		protected:
			/*!
			 * @brief Creates a new base job.
			 */
			BaseJob() : _resultStatus(ResultStatus::Waiting), _executionStatus(ExecutionStatus::Waiting) { }

			/*!
			 * @brief Callback for the Execute()-skeleton to run the job. This needs to be implemented in the child
			 * class.
			 */
			virtual void RunJob() = 0;

			/*!
			 * @brief Marks the result status as moved.
			 * @warning This performs NO check if the result is actually available!
			 */
			inline void MarkResultAsMoved() noexcept { _resultStatus = ResultStatus::Moved; }

			/*!
			 * @brief Checks the result status and raises error if the result is no longer available (e.g. because it
			 * was moved)
			 * @see Job::Get(), Job::Access()
			 */
			void CheckResultAccess()
			{
				// all jobs are lazy evaluated (and possibly async) ==> When no result is (yet) available, try to
				// execute job calling execute
				if (GetResultStatus() == ResultStatus::Waiting) { Execute(); }

				// either we have a result or the job was erroneous or the result was previously moved

				// check for moved or error
				if (GetResultStatus() == ResultStatus::Moved) { throw JobError("Result was already moved!"); }
				if (GetExecutionStatus() == ExecutionStatus::Erroneous)
				{
					throw _error;  // TODO: pass error?
				}

				// result is available
			}
		};
	}  // namespace INTERNAL

	/*!
	 * @ingroup group_module_callable
	 *
	 * @brief A generic Job.
	 *
	 * @details This class encapsulates a user task, so that it may be run on a separate thread. In addition, once the
	 * result is available, it can either be move-accessed, like the std::future-implementation or managed by the Job
	 * and reference accessed. If the job execution is erroneous, the exception is stored and can be accessed.
	 *
	 * @tparam Result The result type.
	 */
	template<class Result>
	class Job : public INTERNAL::BaseJob
	{
	public:
		template<class Function>
		friend std::shared_ptr<Job<std::invoke_result_t<Function>>> make_job(Function&& function);


	private:
		std::packaged_task<Result()> _job;
		// WARN: this requires the result to have a default initialiser. Allocate empty storage instead? see solution of
		// future...
		Result                       _result;

		struct PassKey
		{
			PassKey() { }
		};


	public:
		/*!
		 * @brief Job constructor with pass-key-idiom. This is required for the make_shared method.
		 * @param task The task of the job.
		 *
		 * @see make_job()
		 */
		Job(PassKey const&, std::packaged_task<Result(void)> task) : BaseJob(), _job(std::move(task)) { }

		HORIZON_NoCopy(Job)

		// TODO: Wait_for/Wait_until?

		/*!
		 * @brief Gets the result of the job. If the task is not yet complete, the calling thread will be blocked
		 * until the job is finished.
		 *
		 * This method moves the result to the caller. If the result needs to be shared between multiple calls, use
		 * Access() instead.
		 *
		 * @return The result of the job.
		 *
		 * @throws JobError If the result is not available after the execution of the task, a JobError is thrown.
		 * This might be the case if the task threw an exception or if the result has already been moved by a call
		 * to Get().
		 */
		[[maybe_unused]] [[nodiscard]] inline Result Get()
		{
			CheckResultAccess();

			// move the result
			MarkResultAsMoved();
			return std::move(_result);
		}

		/*!
		 * Gets a reference to the result without moving it. If the task is not yet complete, the calling thread will be
		 * blocked until the job is finished.
		 *
		 * This method returns a const reference to the caller. If the result only needs to be accessed once or needs to
		 * change owner, use Get() instead.
		 *
		 * @return  The result of the job.
		 *
		 * @throws  JobError If the result is not available after the execution of the task, a JobError is thrown. This
		 * might be the case if the task threw an exception or if the result has already been moved by a call to Get().
		 */
		[[maybe_unused]] inline Result const& Access()
		{
			CheckResultAccess();

			return _result;
		}


		std::shared_ptr<Job<Result>> shared_from_this()  // hides the base-class shared-from-this implementation (which
														 // might be needed for priority
		// pass through
		{
			return std::static_pointer_cast<Job<Result>>(BaseJob::shared_from_this());
		}

		std::shared_ptr<Job<Result> const> shared_from_this() const  // same reason as with shared_from_this
		{
			return std::static_pointer_cast<Job<Result> const>(BaseJob::shared_from_this());
		}

	private:
		/*!
		 * @brief Implementation of job execution. This is the callback for the skeleton code in BaseJob::Wait()
		 */
		void RunJob() final
		{
			_job();
			_result = _job.get_future().get();
		}
	};

	// specialisation for void return
	/*!
	 * @copydoc Job<Result>
	 */
	template<>
	class Job<void> : public INTERNAL::BaseJob
	{
	public:
		template<class Function>
		friend std::shared_ptr<Job<std::invoke_result_t<Function>>> make_job(Function&& function);

	private:
		struct PassKey
		{ };

		std::packaged_task<void(void)> _job;


	public:
		/*!
		 * @brief Job constructor with pass-key-idiom. This is required for the make_shared method.
		 * @param task The task of the job.
		 *
		 * @see make_job()
		 */
		Job(PassKey const&, std::packaged_task<void(void)> task) : BaseJob(), _job(std::move(task)) { }

		// prevent copy construction
		HORIZON_NoCopy(Job)

		// TODO: Wait_for/Wait_until?

		/*!
		 * @details Convenience method so that all jobs have the same signature.
		 * This effectively calls Wait.
		 *
		 * @return The result of the job.
		 *
		 * @throws JobError If the result is not available after the execution of the task, a JobError is thrown. This
		 * might be the case if the task threw an exception or if the result has already been moved by a call to Get().
		 */
		[[maybe_unused]] inline void Get() { CheckResultAccess(); }

		/*!
		 * @details Convenience method so that all jobs have the same signature.
		 * This effectively calls Wait.
		 *
		 * @return  The result of the job.
		 *
		 * @throws  JobError If the result is not available after the execution of the task, a JobError is thrown. This
		 * might be the case if the task threw an exception or if the result has already been moved by a call to Get().
		 */
		[[maybe_unused]] inline void Access() { CheckResultAccess(); }


		std::shared_ptr<Job<void>> shared_from_this()  // hides the base-class shared-from-this implementation (which
													   // might be needed for priority
		// pass through
		{
			return std::static_pointer_cast<Job<void>>(BaseJob::shared_from_this());
		}

		std::shared_ptr<Job<void> const> shared_from_this() const  // same reason as with shared_from_this
		{
			return std::static_pointer_cast<Job<void> const>(BaseJob::shared_from_this());
		}

	private:
		/*!
		 * @brief Implementation of job execution. This is the callback for the skeleton code in BaseJob::Wait()
		 */
		void RunJob() final { _job(); }
	};

	/*!
	 * @brief Creates a job from a specified task.
	 *
	 * @tparam Function Function type.
	 * @tparam Result   Result type of the task.
	 *
	 * @param function  The function to encapsulate in the job.
	 * @return          The job object.
	 *
	 * @note The job is not automatically started or queued into a threadpool!
	 */
	template<class Function>
	std::shared_ptr<Job<std::invoke_result_t<Function>>> make_job(Function&& function)
	{
		using Result = std::invoke_result_t<Function>;

		// TODO: check task validity
		// TODO: launch policy? immediately schedule to thread pool?
		// TODO: does packaged_task require the parameters? should not be the case

		// create the corresponding task for the function
		auto task = std::packaged_task<Result(void)>(std::forward<Function>(function));

		// create the job
		return std::make_shared<Job<Result>>(typename Job<Result>::PassKey{}, std::move(task));
	}

}  // namespace HORIZON::CALLABLE