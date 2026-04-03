/*****************************************************************//**
 * \file   service_locator.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#include "service_locator.h"

Locator::ServiceLocator& Services::get()
{
	static Locator::ServiceLocator instance;
	return instance;
}


