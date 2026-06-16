#include "Components/VoltageGenerator.h"

CircuitLab::VoltageGenerator::VoltageGenerator(std::unique_ptr<WaveForm> waveForm) : Component(2, ComponentType::voltageGenerator)
{
	m_waveForm = std::move(waveForm);
}

void CircuitLab::VoltageGenerator::StampMatrix(Eigen::MatrixXd &A, const std::map<int, int> &nodeMap, const std::map<int, int> &VoltageGeneratorMap)
{
	// Recupera gli indici nella matrice (-1 se il terminale è a ground)
	int n1 = (GetTerminals()[0].GetNodeId() != 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n2 = (GetTerminals()[1].GetNodeId() != 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;

	// k è l'indice della riga extra per la corrente incognita
	int k = VoltageGeneratorMap.at(GetId());

	if (n1 >= 0) { A(n1, k) += 1; A(k, n1) += 1; }
	if (n2 >= 0) { A(n2, k) -= 1; A(k, n2) -= 1; }
}

void CircuitLab::VoltageGenerator::StampVector(Eigen::VectorXd &B, const std::map<int, int> &nodeMap, const std::map<int, int> &VoltageGeneratorMap, const StampContext &ctx)
{
	(void)nodeMap;
	// k è l'indice della riga extra per la corrente incognita
	int k = VoltageGeneratorMap.at(GetId());

	B[k] = m_waveForm->Evaluate(ctx.t);
}

CircuitLab::WaveFormType CircuitLab::VoltageGenerator::GetWaveFormType() const
{
	return m_waveForm->GetType();
}

void CircuitLab::VoltageGenerator::SetWaveFormType(WaveFormType type)
{
	SetWaveForm(WaveForm::Create(type));
}

void CircuitLab::VoltageGenerator::SetWaveForm(std::unique_ptr<WaveForm> waveForm)
{
	m_waveForm = std::move(waveForm);
}

void CircuitLab::VoltageGenerator::SaveSpecificData(nlohmann::json &j) const
{
	m_waveForm->Save(j);
}

void CircuitLab::VoltageGenerator::LoadSpecificData(const nlohmann::json &j)
{
	SetWaveForm(WaveForm::Load(j["waveform"]));
}

std::map<CircuitLab::ComponentValue, double> CircuitLab::VoltageGenerator::GetValues() const
{
	return m_waveForm->GetValues();
}

void CircuitLab::VoltageGenerator::SetValues(const std::map<ComponentValue, double> &values)
{
	m_waveForm->SetValues(values);
}
