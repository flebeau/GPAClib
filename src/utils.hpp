#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <iostream>
#include "rlutil.hpp"

//constexpr double PI = 3.141592653589793238463;

/* Custom class for printing error messages using the rlutil library */
class ErrorMessage {
public:
	ErrorMessage(std::string location = "", std::ostream &stream=std::cerr) : stream_(stream) {
		rlutil::setColor(rlutil::LIGHTRED);
		stream_ << "Error: ";
		rlutil::resetColor();
		if (location != "")
			stream << "in " << location << ": ";
	}

	~ErrorMessage() {
		rlutil::resetColor();
		stream_ << "\n\n";
	}

	template<class T>
	ErrorMessage &operator<< (const T &right) {
		stream_ << right;
		return *this;
	}

private:
	std::ostream &stream_;
};

/* Custom class for printing warning messages using the rlutil library */
class WarningMessage {
public:
	WarningMessage(std::string location = "", std::ostream &stream=std::cerr) : stream_(stream) {
		rlutil::setColor(rlutil::LIGHTMAGENTA);
		stream_ << "Warning: ";
		rlutil::resetColor();
		if (location != "")
			stream << "in " << location << ": ";
	}

	~WarningMessage() {
		rlutil::resetColor();
		stream_ << "\n\n";
	}

	template<class T>
	WarningMessage &operator<< (const T &right) {
		stream_ << right;
		return *this;
	}

private:
	std::ostream &stream_;
};

#endif
