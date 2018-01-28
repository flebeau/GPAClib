/*!
 * \file utils.hpp
 * \brief Some useful classes, mostly for pretty printing
 * \authpr Fabrice L.
 */
#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <iostream>
#include "rlutil.hpp"

/// \brief Custom class for printing error messages using the rlutil library
class ErrorMessage {
public:
	/*! \brief Constructor for the ErrorMessage class
	 * \param location Information about the location of the error
	 * \param stream Stream for outputing the error message
	 */
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

/// \brief Custom class for printing warning messages using the rlutil library
class WarningMessage {
public:
	/*! \brief Constructor for the WarningMessage class
	 * \param location Information about the location of the warning
	 * \param stream Stream for outputing the warning message
	 */
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
