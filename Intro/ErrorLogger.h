#ifndef ERROR_LOGGER_H
#define ERROR_LOGGER_H

#include <string>
#include <vector>
#include <ostream>

namespace intro
{

	class ErrorLogger
	{
		size_t line, col;
	public:
		ErrorLogger(size_t line_, size_t col_)
			: line(line_)
			, col(col_)
		{}

		virtual ~ErrorLogger() {}

		size_t getLine(void) { return line; };
		size_t getColumn(void) { return col; };
		virtual bool hasErrors(void) = 0;
		virtual void print(std::wostream &out, unsigned position) = 0;
		void print(std::wostream &out)
		{
			print(out, 0);
		};
	};

	class ErrorLocation : public ErrorLogger
	{
		std::vector<ErrorLogger *> errors;
		const std::wstring location;
	public:
		ErrorLocation(int line_, int col_, const std::wstring &location_)
			: ErrorLogger(line_, col_)
			, location(location_)
		{}

		virtual ~ErrorLocation()
		{
			for (auto error : errors)
				delete error;
		}

		virtual bool hasErrors(void)
		{
			return !errors.empty();
		}
		virtual void print(std::wostream &out, unsigned position)
		{
			out << getLine() << L", " << getColumn() << L" at " << location << ":\n";
			for (ErrorLogger *error : errors)
			{
				error->print(out, position + 1);
			}
		}

		void addError(ErrorLogger *error)
		{
			errors.push_back(error);
		}

	};

	class ErrorDescription : public ErrorLogger
	{
		std::wstring message;
	public:
		ErrorDescription(size_t line_, size_t col_, const std::wstring &message_)
			: ErrorLogger(line_, col_)
			, message(message_)
		{}

		virtual bool hasErrors(void)
		{
			return true;
		}
		virtual void print(std::wostream &out, unsigned position)
		{
			out << L"At " << getLine() << L", " << getColumn()
				<< L" - Error: " << message << std::endl;
		}

	};

}

#endif