//
// @brief
// @details
// @author  Steffen Peikert (ch3ll)
// @email   Horizon@ch3ll.com
// @version 1.0.0
// @date    15/08/2020 20:02
// @project Horizon
//


#include "horizon/callable/CallbackManager.hpp"

using namespace HORIZON::CALLABLE;
using namespace HORIZON::CALLABLE::INTERNAL;

BaseCallbackManager::BaseCallbackManager() : _handleIdentifier() { }

void BaseCallbackManager::Unregister(CallbackHandle& handle) noexcept { handle.Unregister(); }

CallbackHandle BaseCallbackManager::GetNextHandle() noexcept
{
	return CallbackHandle(_handleIdentifier++, shared_from_this());
}

CallbackHandle::~CallbackHandle() { Unregister(); }

void CallbackHandle::Unregister() noexcept
{
	// only do something if there is a manager associated
	if (!IsRegistered()) { return; }

	// remove the callback
	_manager->Unregister(_id);
	// and release the pointer to the manager.
	// WARN: this might free the associated manager
	_manager.reset();
}