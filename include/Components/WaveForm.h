#pragma once
#include <nlohmann/json.hpp>
#include <map>

#include "Common/ComponentType.h"
#include "Common/ComponentValue.h"

namespace CircuitLab {
	class WaveForm
	{
	protected:
		WaveFormType m_waveFormType;
		virtual void SaveSpecificData(nlohmann::json &j) const = 0;
	public:
		virtual ~WaveForm() = default;
		virtual double Evaluate(double t) = 0;
		virtual std::map<ComponentValue, double> GetValues() const = 0;
		virtual void SetValues(const std::map<ComponentValue, double> &values) = 0;
		WaveFormType GetType() const { return m_waveFormType; }
		void Save(nlohmann::json &j) const;
		static std::unique_ptr<WaveForm> Create(WaveFormType type);
		static std::unique_ptr<WaveForm> Load(const nlohmann::json &j);
	};
}