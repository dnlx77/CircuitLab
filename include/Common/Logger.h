#pragma once
#include <fstream>
#include <sstream>

#define LOG_DEBUG(logMsg) \
    do { \
		std::ostringstream oss;\
		oss << logMsg;\
		std::string msg = oss.str();\
		Logger::GetInstance().Log(msg, CircuitLab::LogLevel::Debug);\
    } while(0)

#define LOG_INFO(logMsg) \
    do { \
		std::ostringstream oss;\
		oss << logMsg;\
		std::string msg = oss.str();\
		Logger::GetInstance().Log(msg, CircuitLab::LogLevel::Info);\
    } while(0)

#define LOG_WARNING(logMsg) \
    do { \
		std::ostringstream oss;\
		oss << logMsg;\
		std::string msg = oss.str();\
		Logger::GetInstance().Log(msg, CircuitLab::LogLevel::Warning);\
    } while(0)

#define LOG_ERROR(logMsg) \
    do { \
		std::ostringstream oss;\
		oss << logMsg;\
		std::string msg = oss.str();\
		Logger::GetInstance().Log(msg, CircuitLab::LogLevel::Error);\
    } while(0)

namespace CircuitLab {

	enum class LogLevel {
		Debug,
		Info,
		Warning,
		Error
	};

	class Logger {
	private:
		Logger() = default;
		Logger(const Logger &) = delete;
		Logger &operator=(const Logger &) = delete;

		bool m_logToConsole = true;
		std::ofstream m_fileStream;
		LogLevel m_minLevel = LogLevel::Debug;
		std::string m_filePath;

		std::string_view LevelToString(LogLevel level);
	public:
		~Logger();
		static Logger &GetInstance() {
			static Logger instance;
			return instance;
		}

		void Log(std::string logMsg, LogLevel level);

		LogLevel GetLogMinLevel() const { return m_minLevel; }
		bool IsLogToConsole() const { return m_logToConsole; }
		const std::string &GetFilePath() const { return m_filePath; }

		void SetMinLogLevel(LogLevel newMinLevel) { m_minLevel = newMinLevel; }
		void SetLogToConsole(bool logToConsole) { m_logToConsole = logToConsole; }
		void SetLogToFile(const std::string &path);
	};
}