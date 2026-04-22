#include "Common/Logger.h"
#include <iostream>

std::string_view CircuitLab::Logger::LevelToString(LogLevel level)
{
	switch (level)
	{
	case LogLevel::Debug:
		return "[DEBUG] ";
	case LogLevel::Info:
		return "[INFO] ";
	case LogLevel::Warning:
		return "[WARNING] ";
	case LogLevel::Error:
		return "[ERROR] ";
	default:
		return "";
	}
}

CircuitLab::Logger::~Logger()
{
	if (m_fileStream.is_open())
		m_fileStream.close();
}

void CircuitLab::Logger::Log(std::string logMsg, LogLevel level)
{
	if (level >= m_minLevel)
	{
		if (m_logToConsole)
			std::cout << LevelToString(level) << logMsg << std::endl;
		if (m_fileStream.is_open())
			m_fileStream << LevelToString(level) << logMsg << std::endl;
	}
}

void CircuitLab::Logger::SetLogToFile(const std::string &path)
{
	m_filePath = path;
	m_fileStream.open(m_filePath);

	if (!m_fileStream.is_open())
		std::cout << "[WARNING] Errore nell'apertura del file " << path << std::endl;
}
