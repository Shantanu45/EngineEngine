#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <typeindex>

namespace Locator
{
	class ServiceLocator 
	{
	public:
		template<typename T>
		void provide(std::shared_ptr<T> service) {
			auto key = std::type_index(typeid(T));
			services_[key] = service;
		}

		template<typename T>
		std::shared_ptr<T> get() {
			auto key = std::type_index(typeid(T));
			auto it = services_.find(key);
			if (it == services_.end()) {
				throw std::runtime_error("Service not registered: " + std::string(typeid(T).name()));
			}
			return std::static_pointer_cast<T>(it->second);
		}

		template<typename T>
		bool has() const {
			return services_.count(std::type_index(typeid(T))) > 0;
		}

	private:
		std::unordered_map<std::type_index, std::shared_ptr<void>> services_;
	};
}

class Services {
public:
	static Locator::ServiceLocator& get() {
		static Locator::ServiceLocator instance;
		return instance;
	}
	Services() = delete;
};

