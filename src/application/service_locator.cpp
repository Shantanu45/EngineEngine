#include "service_locator.h"

Locator::ServiceLocator& Services::get()
{
	static Locator::ServiceLocator instance;
	return instance;
}


