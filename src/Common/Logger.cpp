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

void CircuitLab::Logger::Log(std::string logMsg, LogLevel level)
{
	if (level >= m_minLevel)
	{
		// Il flush ad ogni riga (std::endl) è costoso, soprattutto su console: lo si fa solo per
		// avvisi ed errori, che devono arrivare su file anche se il programma si interrompe subito dopo
		const bool flush = level >= LogLevel::Warning;
		if (m_logToConsole)
		{
			std::cout << LevelToString(level) << logMsg << '\n';
			if (flush)
				std::cout.flush();
		}
		if (m_fileStream.is_open())
		{
			m_fileStream << LevelToString(level) << logMsg << '\n';
			if (flush)
				m_fileStream.flush();
		}
	}
}

void CircuitLab::Logger::SetLogToFile(const std::string &path)
{
	m_filePath = path;
	m_fileStream.open(m_filePath);

	if (!m_fileStream.is_open())
		std::cout << "[WARNING] Errore nell'apertura del file " << path << std::endl;
}
