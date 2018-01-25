/*!
 * \file circuit.hpp
 * \brief File of GPAClib containing the GPAC class
 * \author Fabrice L.
 * 
 * Header file containing the GPAC class and useful circuits.
 */


#ifndef CIRCUIT_HPP_
#define CIRCUIT_HPP_

#include <map>
#include <memory>
#include <string>

#include "utils.hpp"
#include "gate.hpp"

/*! \namespace GPAClib
 * 
 * Namespace containing the library and the main class
 */
namespace GPAClib {

using GatesMap = std::map<std::string, std::unique_ptr<Gate> >;

/*! \class CircuitConstIterator
 * \brief Iterator on Circuit class
 * 
 * Iterator on the names of the gates of a circuit.
 */
class CircuitConstIterator : public GatesMap::const_iterator {
public:
	CircuitConstIterator() : GatesMap::const_iterator() {}
	CircuitConstIterator(GatesMap::const_iterator it_) : GatesMap::const_iterator(it_) {}
	
	const std::string *operator->() {return (const std::string * const) &( GatesMap::const_iterator::operator->()->first);}
	std::string operator*() {return GatesMap::const_iterator::operator*().first;}
};

/*! \class Circuit
 * \brief Abstract class representing a circuit
 *
 * Base class for defining circuits, which have a name and gates. One of the gates is the output gate.
 */
class Circuit {
public:
	/*! \brief Circuit constructor
	 * \param name Name of the circuit
	 */
	Circuit(std::string name = "") : circuit_name(name), gates(), output_gate("") {}
	
	/*! \brief Accessing the gates
	 * \return A reference to the gates of the circuit.
	 */
	const GatesMap &Gates() const {return gates;}
	
	CircuitConstIterator begin() const {return CircuitConstIterator(gates.cbegin());}
	CircuitConstIterator end() const {return CircuitConstIterator(gates.cend());}
	
	const std::string &Output() const {return output_gate;}
	void setOutput(std::string output) {output_gate = output;}
	
	const std::string &Name() const {return circuit_name;}
	void rename(std::string name) {circuit_name = name;}
	
	/*! \brief Custom error message for circuits
	 * \return An instance of class ErrorMessage specifying the name of the circuit
	 */
	ErrorMessage CircuitErrorMessage() const {
		return ErrorMessage("circuit " + circuit_name);
	}
	/*! \brief Custom warning message for circuits
	 * \return An instance of class WarningMessage specifying the name of the circuit
	 */
	WarningMessage CircuitWarningMessage() const {
		return WarningMessage("circuit " + circuit_name);
	}
	
protected:
	std::string circuit_name; /*!< Name of the circuit */
    GatesMap gates; /*!< Map storing gates by their name */
	std::string output_gate; /*!< Name of the output gate of the circuit */
};

}

#endif
