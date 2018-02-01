/*!
 * \file circuit.hpp
 * \brief File containing the base class for defining circuits
 * \author Fabrice L.
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
 * \brief Namespace of the library
 */
namespace GPAClib {

/// Polymorphism-friendly container type for storing gates by names
using GatesMap = std::map<std::string, std::unique_ptr<Gate> >;

/*! \brief Iterator on Circuit class
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

/*! \brief Abstract class representing a circuit
 *
 * Base class for defining circuits, which have a name and gates. One of the gates is the output gate.
 */
class Circuit {
public:
	/*! \brief Circuit constructor
	 * \param name Name of the circuit
	 *
	 * Generates a new empty circuit with the optional given name.
	 */
	Circuit(std::string name = "") : circuit_name(name), gates(), output_gate("") {}
	
	/*! \brief Accessing the gates
	 * \return A reference to the gates of the circuit stored by names.
	 */
	const GatesMap &Gates() const {return gates;}
	size_t size() const {return gates.size();}
	
	/*! \brief Iterating on names of gates
	 * \return An iterator on the names of the gates of the circuit.
	 */
	CircuitConstIterator begin() const {return CircuitConstIterator(gates.cbegin());}
	CircuitConstIterator end() const {return CircuitConstIterator(gates.cend());}
	
	/// Retrieving name of the output gate
	const std::string &Output() const {return output_gate;}
	/// Changing the name of the output gate
	void setOutput(std::string output) {output_gate = output;}
	
	/// Retieving name of the circuit
	const std::string &Name() const {return circuit_name;}
	/// Renaming circuit
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
	std::string circuit_name; ///< Name of the circuit
    GatesMap gates; ///< Map storing gates by their name
	std::string output_gate; ///< Name of the output gate of the circuit
};

}

#endif
