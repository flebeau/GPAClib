/*! 
 * \file GPAC.hpp
 * \brief Main file of GPAClib containing the GPAC class
 * \author Fabrice L.
 *
 * Header file containing the GPAC class and useful predefined analog circuits.
 */

#ifndef GPAC_HPP_
#define GPAC_HPP_

#include <map>
#include <memory>
#include <string>
#include <set>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <queue>
#include <math.h>
#include <omp.h>
#include <boost/numeric/odeint.hpp>
#include <boost/numeric/odeint/external/openmp/openmp.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/math/constants/constants.hpp>

#include "utils.hpp"
#include "gate.hpp"
#include "circuit.hpp"
#include "gnuplot-iostream/gnuplot-iostream.h"

namespace GPAClib {
/*! \brief Main class implementing analog circuits and simulating them using Odeint
 * \tparam T Type of the values (floating point number)
 *
 * Implements an analog circuit, which is a combination of addition, product, constant and 
 * integration gates. A special gate called `t` is used to refer to the variable (the "time").
 *
 * Operators +, * and parenthesis are overloaded so that it is easy to add, multiply, and compose circuits.
 *
 * Methods are provided in order to validate a circuit and to simulate it using Boost Odeint.
 * The result of the simulation can be exported to a pdf file using Gnuplot.
 */
template<typename T>
class GPAC : public Circuit {
public:
	/*! \brief Constructing an analog circuit with a name
	 * \param name Name of the circuit (optional)
	 * \param validation_ If true, the circuit is validated at every modification (default: true)
	 * \param block_ If true, the circuit is a common circuit, not a user-specific one (default: false)
	 */
	GPAC(std::string name = "", bool validation_ = true, bool block_ = false) : Circuit(name), validation(validation_), block(block_), finalized(false) {}
	
	/*! \brief Copy constructor 
	 * 
	 * Duplicate the input dircuit by copying the gates and setting the same output gate, validation
	 * option, block option. If the input circuit is a block circuit (not user-specific), ensure that
	 * the name of the obtained circuit is different from the input one.
	 */
	GPAC(const GPAC<T> &circuit) {
		copyInto(circuit, false);
		if (!circuit.Block() && circuit.Name() != "")
			circuit_name = circuit.Name() + "_";
		else if (circuit.Block())
			circuit_name = circuit.Name();
		output_gate = circuit.Output();
		validation = circuit.Validation();
		finalized = false;
		block = circuit.Block();
	}
	
	/// Copy through '=' operator
	GPAC<T> &operator=(const GPAC<T> &circuit) {
		gates.clear();
		copyInto(circuit, false);
		if (!circuit.Block() && circuit.Name() != "")
			circuit_name = circuit.Name() + "_";
		else if (circuit.Block())
			circuit_name = circuit.Name();
		output_gate = circuit.Output();
		validation = circuit.Validation();
		finalized = false;
		block = circuit.Block();
		return *this;
	}
	
	/*! \brief Copy all gates of circuit in current circuit
	 * \param circuit Source circuit whose gates are to be copied
	 * \param validate If true, all gates imported are validated (default: true)
	 *
	 * Import all gates from input circuit in the current circuit. Also import the initial
	 * values of integration gates of the input circuit.
	 */
	void copyInto(const GPAC<T> &circuit, bool validate = true) {
		for (const auto &g : circuit) {
			if (circuit.isAddGate(g)) {
				const AddGate<T>* gate = circuit.asAddGate(g);
				addAddGate(g, gate->X(), gate->Y(), validate);
			}
			else if (circuit.isProductGate(g)) {
				const ProductGate<T>* gate = circuit.asProductGate(g);
				addProductGate(g, gate->X(), gate->Y(), validate);
			}
			else if (circuit.isIntGate(g)) {
				const IntGate<T>* gate = circuit.asIntGate(g);
				addIntGate(g, gate->X(), gate->Y(), validate);
			}
			else if (circuit.isConstantGate(g)) {
				const ConstantGate<T>* gate = circuit.asConstantGate(g);
				addConstantGate(g, gate->Constant(), validate);
			}
			if (circuit.isIntGate(g) && circuit.getValues().count(g) > 0)
				setInitValue(g, circuit.getValues().at(g));
		}
	}
	
	/*! \brief Validation of gate names
	 * \param gate_name Name of the gate to be added
	 * \param forbid_underscore If true, gate names starting with underscore are rejeted
	 *
	 */
	void validateGateName(std::string gate_name, bool forbid_underscore = true) const {
		if (gate_name.size() == 0) {
			CircuitErrorMessage() << "Gate name cannot be of length 0!";
			exit(EXIT_FAILURE);
		}
		else if (gate_name[0] == '_' && forbid_underscore) {
			CircuitErrorMessage() << "Gate names starting with underscore character are reserved!";
			exit(EXIT_FAILURE);
		}
		else if (gate_name == "t") {
			CircuitErrorMessage() << "Can't name a gate \"t\" is reserved!";
			exit(EXIT_FAILURE);
		}
	}
	/*! \brief Adding an addition gate
	 * \param gate_name Name of the gate to be added (if empty string, creates a new unique name)
	 * \param x Name of the first input gate
	 * \param y Name of the second input gate
	 * \param validate If true, validate gate name before adding it, even if the circuit `validation` attribute is set to false (default: true)
	 * \return Name of the added gate
	 *
	 * Add an addition gate to the circuit with specified name and inputs. If there is already a gate
	 * with the given name, a warning is issued and the existing gate is overwritten.
	 */
	std::string addAddGate(std::string gate_name, std::string x, std::string y, bool validate = true) {
		finalized = false;
		if (gate_name == "")
			gate_name = getNewGateName();
		else if (validation && validate)
			validateGateName(gate_name);
		if (gates.count(gate_name) > 0) {
			CircuitWarningMessage() << "Gate \"" << gate_name << "\" already exists, adding it again will overwrite it!";
			gates[gate_name].reset(new AddGate<T>(x,y));
		}
		else
			gates[gate_name] = std::unique_ptr<Gate>(new AddGate<T>(x,y));
		ensureNewGateIdLargeEnough(gate_name);
		return gate_name;
	}
	
    /*! \brief Adding a product gate
	 * \param gate_name Name of the gate to be added (if empty string, creates a new unique name)
	 * \param x Name of the first input gate
	 * \param y Name of the second input gate
	 * \param validate If true, validate gate name before adding it, even if the circuit `validation` attribute is set to false (default: true)
	 * \return Name of the added gate
	 *
	 * Add a product gate to the circuit with specified name and inputs. If there is already a gate
	 * with the given name, a warning is issued and the existing gate is overwritten.
	 */
	std::string addProductGate(std::string gate_name, std::string x, std::string y, bool validate = true) {
		finalized = false;
		if (gate_name == "")
			gate_name = getNewGateName();
		else if (validation && validate)
			validateGateName(gate_name);
		if (gates.count(gate_name) > 0) {
			CircuitWarningMessage() << "Gate \"" << gate_name << "\" already exists, adding it again will overwrite it!";
			gates[gate_name].reset(new ProductGate<T>(x,y));
		}
		else
			gates[gate_name] = std::unique_ptr<Gate>(new ProductGate<T>(x,y));
		ensureNewGateIdLargeEnough(gate_name);
		return gate_name;
	}
	
	/*! \brief Adding an integration gate
	 * \param gate_name Name of the gate to be added (if empty string, creates a new unique name)
	 * \param x Name of the integrand gate name
	 * \param y Name of the gate with respect to which is integration is done
	 * \param validate If true, validate gate name before adding it, even if the circuit `validation` attribute is set to false (default: true)
	 * \return Name of the added gate
	 *
	 * Add an integration to the circuit with specified name and inputs. If there is already a gate
	 * with the given name, a warning is issued and the existing gate is overwritten.
	 */
	std::string addIntGate(std::string gate_name, std::string x, std::string y, bool validate = true) {
		finalized = false;
		if (gate_name == "")
			gate_name = getNewGateName();
		else if (validation && validate)
			validateGateName(gate_name);
		if (validation && validate && has(y) && isConstantGate(y)) {
			CircuitErrorMessage() << "Gate \"" << gate_name << "\" is defined as an integration gate with constant second input!";
			exit(EXIT_FAILURE);
		}
		if (gates.count(gate_name) > 0) {
			CircuitWarningMessage() << "Gate \"" << gate_name << "\" already exists, adding it again will overwrite it!";
			gates[gate_name].reset(new IntGate<T>(x,y));
		}
		else {
			gates[gate_name] = std::unique_ptr<Gate>(new IntGate<T>(x,y));
		}
		ensureNewGateIdLargeEnough(gate_name);
		return gate_name;
	}
	
	/*! \brief Adding a constant gate
	 * \param gate_name Name of the gate to be added (if empty string, creates a new unique name)
	 * \param value Value of the gate
	 * \param validate If true, validate gate name before adding it, even if the circuit `validation` attribute is set to false (default: true)
	 * \return Name of the added gate
	 *
	 * Add a constant gate to the circuit with specified name and value. If there is already a gate
	 * with the given name, a warning is issued and the existing gate is overwritten.
	 */
	std::string addConstantGate(std::string gate_name, T value, bool validate = true) {
		finalized = false;
		if (gate_name == "")
			gate_name = getNewGateName();
		else if (validation && validate)
			validateGateName(gate_name);
		if (gates.count(gate_name) > 0) {
			CircuitWarningMessage() << "Gate \"" << gate_name << "\" already exists, adding it again will overwrite it!";
			gates[gate_name].reset(new ConstantGate<T>(value));
		}
		else
			gates[gate_name] = std::unique_ptr<Gate>(new ConstantGate<T>(value));
		ensureNewGateIdLargeEnough(gate_name);
		return gate_name;
	}
	
	/// Delete the gate
	GPAC<T> &eraseGate(std::string gate_name) {
		gates.erase(gate_name);
		return *this;
	}
	
	/// Modify all occurences of gate_name in the inputs into new_name
	GPAC<T> &renameInputs(std::string gate_name, std::string new_name) {
		for (const auto &g : gates) {
			if (!isConstantGate(g.first)) {
				BinaryGate<T>* gate = asBinaryGate(g.first);
				if (gate->X() == gate_name)
					gate->X() = new_name;
				if (gate->Y() == gate_name)
					gate->Y() = new_name;
			}
		}
		return *this;
	}
	
	/// Rename a gate
	GPAC<T> &renameGate(std::string gate_name, std::string new_name) {
		gates[new_name] = std::move(gates[gate_name]);
		gates.erase(gate_name);
		if (values.count(gate_name)) {
			T value = values[gate_name];
			values.erase(gate_name);
			values[new_name] = value;
		}
			
		/* Replace output gate if necessary */
		if (gate_name == output_gate)
			output_gate = new_name;
		
		return *this;
	}
	
    /// Returns the `block` attribute value
	bool Block() const {return block;}
	/// Returns the `validation` attribute value
	bool Validation() const {return validation;}
	/// Returns true if there is a specific gate in the circuit
	bool has(std::string gate_name) const {
		return (gates.count(gate_name) != 0);
	}
	
	/// \brief Returns true if the target gate is an addition gate
	/// \pre A gate named `gate_name` must exist in the circuit.
	bool isAddGate(std::string gate_name) const {
		return (dynamic_cast<AddGate<T>*>(gates.at(gate_name).get()) != nullptr);
	}
	/// \brief Returns true if the target gate is a product gate
	/// \pre A gate named `gate_name` must exist in the circuit.
	bool isProductGate(std::string gate_name) const {
		return (dynamic_cast<ProductGate<T>*>(gates.at(gate_name).get()) != nullptr);
	}
	/// \brief Returns true if the target gate is an integration gate
	/// \pre A gate named `gate_name` must exist in the circuit.
	bool isIntGate(std::string gate_name) const {
		return (dynamic_cast<IntGate<T>*>(gates.at(gate_name).get()) != nullptr);
	}
	/// \brief Returns true if the target gate is a constant gate
	/// \pre A gate named `gate_name` must exist in the circuit.
	bool isConstantGate(std::string gate_name) const {
		return (dynamic_cast<ConstantGate<T>*>(gates.at(gate_name).get()) != nullptr);
	}
	/// \brief Returns true if the gate is a combination of constant gates, e.g. (1+1)*2
	/// \pre A gate named `gate_name` must exist in the circuit.
	bool isCombinationConstantGates(std::string gate_name) const {
		if (gate_name == "t")
			return false;
		if (isIntGate(gate_name))
			return false;
		if (isConstantGate(gate_name))
			return true;
		const BinaryGate<T> *gate = asBinaryGate(gate_name);
		return (isCombinationConstantGates(gate->X()) && isCombinationConstantGates(gate->Y()));
	}
	/// \brief Returns the value of the gate as a constant
	/// \pre The gate named `gate_name` must be a combination of constant gates
	T valueCombinationConstantGates(std::string gate_name) const {
		if (isConstantGate(gate_name))
			return asConstantGate(gate_name)->Constant();
		const BinaryGate<T> *gate = asBinaryGate(gate_name);
		return gate->operator()(valueCombinationConstantGates(gate->X()), valueCombinationConstantGates(gate->Y()));
	}
	
	/// \brief Returns true if the target gate is a binary gate
	/// \pre A gate named `gate_name` must exist in the circuit.
	bool isBinaryGate(std::string gate_name) const {
		return (dynamic_cast<BinaryGate<T>*>(gates.at(gate_name).get()) != nullptr);
	}
	
    /// \brief Returns a AddGate constant pointer to the specified gate
	/// \pre A gate named `gate_name` must exist in the circuit and must be an addition gate.
	const AddGate<T>* asAddGate(std::string gate_name) const {
		return (dynamic_cast<AddGate<T>*>(gates.at(gate_name).get()));
	}
	/// \brief Returns a ProductGate constant pointer to the specified gate
	/// \pre A gate named `gate_name` must exist in the circuit and must be a product gate.
	const ProductGate<T>* asProductGate(std::string gate_name) const {
		return (dynamic_cast<ProductGate<T>*>(gates.at(gate_name).get()));
	}
	/// \brief Returns a IntGate constant pointer to the specified gate
	/// \pre A gate named `gate_name` must exist in the circuit and must be an integration gate.
	const IntGate<T>* asIntGate(std::string gate_name) const {
		return (dynamic_cast<IntGate<T>*>(gates.at(gate_name).get()));
	}
	/// \brief Returns a ConstantGate constant pointer to the specified gate
	/// \pre A gate named `gate_name` must exist in the circuit and must be a constant gate.
	const ConstantGate<T>* asConstantGate(std::string gate_name) const {
		return (dynamic_cast<ConstantGate<T>*>(gates.at(gate_name).get()));
	}
	/// \brief Returns a BinaryGate constant pointer to the specified gate
	/// \pre A gate named `gate_name` must exist in the circuit and must be a binary gate.
	const BinaryGate<T>* asBinaryGate(std::string gate_name) const {
		return (dynamic_cast<BinaryGate<T>*>(gates.at(gate_name).get()));
	}
	
	/*! \brief Export the circuit into a string
	 * \param show_all_values If set to true, show the numerical values of all gates, not just integration gates (default: false)
	 *
	 * The output format for a circuit matches the specification format (see README for more details)
	 */
	std::string toString(bool show_all_values = false) const {
		std::stringstream res("");
		std::string prefix_line = "";
		if (circuit_name != "") {
			res << "Circuit " << circuit_name << ":\n";
		}
		else {
			res << "Circuit unknown:\n";
		}
		prefix_line = "\t";
		for (const auto &g : gates) {
			if (g.first == output_gate) // Skip output gate to print it last
				continue;
			res << prefix_line << g.first
				<< ": "  << g.second->toString();
			if (values.count(g.first))
				if (show_all_values || isIntGate(g.first))
					res << " | " << values.at(g.first);
			res << "\n";
		}
		res << prefix_line << output_gate
			<< ": " << gates.at(output_gate)->toString();
		if (values.count(output_gate))
				if (show_all_values || isIntGate(output_gate))
					res << " | " << values.at(output_gate);
		res << "\n;\n";
		return res.str();
	}
	
	/*! \brief Compute a dot representation of the circuit
	 * 
	 * Returns a dot representation of the circuit, where nodes are gates and edges of the inputs
	 * point to the corresponding gate. The output gate is highlighted in red and doubled rectangle.
	 * For integration gates, the input corresponding to the integration variable are drawn dashed.
	 */
	std::string toDot(bool show_int_gates_number = false) const {
		std::stringstream res("");
		
		/* Identify different types of gates */
		std::vector<std::string> constant_names;
		std::vector<std::string> addition_names;
		std::vector<std::string> product_names;
		std::vector<std::string> integration_names;
		
		for (const auto &g : gates) {
			if (isConstantGate(g.first))
				constant_names.push_back(g.first);
			else if (isAddGate(g.first))
				addition_names.push_back(g.first);
			else if (isProductGate(g.first))
				product_names.push_back(g.first);
			else if (isIntGate(g.first))
				integration_names.push_back(g.first);
		}
		
		/* Generate the header first */
		res << "digraph " << circuit_name << "{\n"
			<< "\tnode [shape = box];\n\n";
		
		/* Define nodes with the appropriate labels */
		// First, the node for t
		res << "\tnode [label = \"t\"]; t;\n\n";
        
        // Constant gates
		for (const auto &gate_name: constant_names) {
			const ConstantGate<T> *gate = asConstantGate(gate_name);
			res << "\tnode [label = \"" << gate->Constant() << "\"]; "
				<< gate_name;
			if (output_gate == gate_name)
				res << " [color = red, fontcolor = red, peripheries = 2]";
			res << ";\n";
		}
		if (constant_names.size() > 0)
			res << "\n";
		
		// Addition gates
		if (addition_names.size() > 0) {
			res << "\tnode [label = \"+\"];\n";
			for (const auto &gate_name: addition_names) {
				res << "\t" << gate_name;
				if (output_gate == gate_name)
					res << " [color = red, fontcolor = red, peripheries = 2]";
				res << ";\n";
			}
			res << "\n";
		}
		
		// Product gates
		if (product_names.size() > 0) {
			res << "\tnode [label = \"⨯\"];\n";
			for (const auto &gate_name: product_names) {
				res << "\t" << gate_name;
				if (output_gate == gate_name)
					res << " [color = red, fontcolor = red, peripheries = 2]";
				res << ";\n";
			}
			res << "\n";
		}
		
		// Integration gates
		/*if (integration_names.size() > 0) {
			res << "\tnode [label = \"∫\"];\n";
			for (const auto &gate_name: integration_names) {
				res << "\t" << gate_name;
				if (output_gate == gate_name)
					res << " [color = red, fontcolor = red, peripheries = 2]";
				res << ";\n";
			}
			res << "\n";
			}*/
		for (unsigned i = 0; i<integration_names.size(); ++i) {
			res << "\tnode [label = \"∫";
			if (show_int_gates_number)
				res << "_" << i+1;
			res << "\"];";
			res << integration_names[i];
			if (output_gate == integration_names[i])
					res << " [color = red, fontcolor = red, peripheries = 2]";
			res << ";\n";
		}
		
		/* Now define edges */
		for (const auto &g: gates) {
			if (isBinaryGate(g.first)) {
				const BinaryGate<T> *gate = asBinaryGate(g.first);
				res << "\t" << gate->X() << " -> " << g.first << ";\n";
				if (isIntGate(g.first) && gate->Y() == "t")
					continue;
				res << "\t" << gate->Y() << " -> " << g.first;
				if (isIntGate(g.first))
					res << " [style = dashed]";
				res << ";\n";
			}
		}
		res << "}\n";
		
		return res.str();
	}
	
	/// \brief Export the dot representation to a file
	void toDot(std::string filename) const {
		std::ofstream ofs(filename, std::ofstream::out);
		ofs << toDot() << "\n";
	}
	
	/// \brief Compute a latex code representing the PIVP corresponding to the circuit
	/// \pre The circuit must be finalized!
	std::string toLaTeX() const {
		if (!finalized) {
			CircuitErrorMessage() << "Cannot export to LaTeX a circuit if it is not finalized!";
			exit(EXIT_FAILURE);
		}
		
		std::stringstream res("");
		std::map<std::string, unsigned> int_gate_numbers;
		for (unsigned i = 0; i<int_gates.size(); ++i)
			int_gate_numbers[int_gates[i]] = i+1;
		
		/* Add the header (standalone LaTeX file) */
		res << "\\documentclass[varwidth=\\maxdimen, preview]{standalone}\n"
			<< "\\usepackage{amsmath}\n"
			<< "\\begin{document}\n"
			<< "\\begin{equation*}\n"
			<< "\\begin{cases}\n";
		
		/* Add the pODE */
		for (unsigned i = 0; i<int_gates.size(); ++i)
			res << "x_{" << std::to_string(i+1) << "}' = "
				<< toTermLaTeXGate(int_gate_numbers, asIntGate(int_gates[i])->X()).toString()
				<< "\\\\" << "\n";
		
		res << "y = "
			<< toTermLaTeXGate(int_gate_numbers, output_gate).toString()
			<< "\\\\\n";
		
		/* Close the environments */
		res << "\\end{cases}\n"
			<< "\\end{equation*}\n"
			<< "\\end{document}\n";
		return res.str();
	}
	
	/// \brief Export the LaTeX representation to a file
	void toLaTeX(std::string filename) const {
		std::ofstream ofs(filename, std::ofstream::out);
		ofs << toLaTeX() << "\n";
	}
	
	/*! \brief Parenthesis operator for adding binary gates
	 * \param gate_name Name of the gate to be added
	 * \param op Symbol of the operator
	 * \param x First input name
	 * \param y Second input name
	 * 
	 * User-friendly parenthesis operator for adding binary gates to a circuit. Symbols for describing
	 * operation are:
	 *   - `a`, `A`, `+` for addition
	 *   - `p`, `P`, `x`, `X`, `*` for product
	 *   - `i`, `I` for integration.
	 */
	GPAC<T> &operator()(std::string gate_name, std::string op, std::string x, std::string y) {
		if (op == "a" || op == "A" || op == "+")
			addAddGate(gate_name, x, y);
		else if (op == "p" || op == "P" || op == "x" || op == "X" || op == "*")
			addProductGate(gate_name, x, y);
		else if (op == "i" || op == "I")
			addIntGate(gate_name, x, y);
		else {
			CircuitWarningMessage() << op << " is not a valid operation, it is skipped.";
		}
		return *this;
	}
	/*! \brief Parenthesis operator for adding constant gates
	 * \param gate_name Name of the gate to be added
	 * \param value Value of the constant gate to be added
	 */
	GPAC<T> &operator()(std::string gate_name, T value) {
		addConstantGate(gate_name, value);
		return *this;
	}
	
	/// \brief Add the name of the circuit in front of the name of the gate
	std::string exportName(std::string gate_name) const {
		if (gate_name == "t")
			return "t";
		else if (gate_name.length() < circuit_name.length() && std::equal(gate_name.begin(), gate_name.end(), circuit_name.begin()))
			return gate_name;
		else if (gate_name[0] == '_')
			return circuit_name + gate_name;
		else
			return circuit_name + "_" + gate_name;
	}
	
	/// \brief Exports the circuit to a C++ code that can be used with GPAClib
	std::string toCode(std::string var_name = "circuit") const {	
		std::stringstream res("");
		std::string op;
		
		res << var_name << "\n";
		
		for (const auto &g : gates) {
			if (isConstantGate(g.first)) {
				const ConstantGate<T> *gate = asConstantGate(g.first);
				res << "\t(\"" << exportName(g.first) << "\", " << gate->Constant() << ")\n";
				continue;
			}
			const BinaryGate<T> *gate = asBinaryGate(g.first);
			if (isAddGate(g.first))
				op = "\"+\"";
			else if (isProductGate(g.first))
				op = "\"*\"";
			else
				op = "\"I\"";
			res << "\t(\"" << exportName(g.first) << "\", " << op << ", "
				<< "\"" << exportName(gate->X()) << "\", " << "\"" << exportName(gate->Y()) << "\")\n";
		}
		res << ";\n"
			<< var_name << ".setOutput(\"" << exportName(output_gate) << "\");\n";		
        
        /* Now export initial values for integration gates */
		for (const auto &g : gates) {
			if (!isIntGate(g.first))
				continue;
			if (values.count(g.first))
				res << var_name << ".setInitValue(\"" << exportName(g.first) << "\", " << values.at(g.first) << ");\n";
		}
		
		return res.str();
	}
	
	/*! \brief Comparator class for sorting integration gates
	 *
	 * When normalizing a circuit, some integration gates are replaced with larger subcircuits.
	 * This comparator class is used to sort integration gates so that the one leading to smaller
	 * subcircuits are treated first.
	 */
	class CompareIntGate {
	public:
		CompareIntGate(const GPAC &circuit_) : circuit(circuit_) {}
		
		/// Treat intgration gates with product gates as second input before the one with
		/// addition gates as second input
		bool operator()(const std::string &x, const std::string &y) {
			const IntGate<T> *gate1 = circuit.asIntGate(x);
			const IntGate<T> *gate2 = circuit.asIntGate(y);
			if (circuit.isIntGate(gate1->Y()) && circuit.asIntGate(gate1->Y())->Y() == "t") {
				return false;
			}
			if (circuit.isIntGate(gate2->Y()) && circuit.asIntGate(gate2->Y())->Y() == "t")
				return true;
			if (circuit.isProductGate(gate1->Y()))
				return false;
			if (circuit.isProductGate(gate2->Y()))
				return true;
			if (circuit.isAddGate(gate1->Y()))
				return false;
			if (circuit.isAddGate(gate2->Y()))
				return true;
			return x > y;
		}
	private:
		const GPAC &circuit; /*!< Reference to the circuit in which the gates to be compared are */
	};
	
	/*! \brief Normalization of a circuit
	 * \param guess_init_value Try to propagate initial values of integration gates when introducing new integration gates (default: true)
	 * 
	 * In a circuit, integration gates for which the second input are not variable `t` are 
	 * problematic, especially for simulating. Fortunately, it is possible to modify the circuit
	 * by introducing new gates in order to ensure that all integration gates have `t` as their second
	 * input. This process always terminates.
	 *
	 * In order to do this normalization, we maintain a priority queue of the problematic integration
	 * gates (sorted with the CompareIntGate comparator class). We then treat each integration gate in
	 * the queue until the queue is empty (some steps can increase the size of the queue).
	 */
	GPAC<T> &normalize(bool guess_init_value = true) {
		if (finalized)
			return *this;
		
		/* First make a list of all integration gates with no t inputs */
		std::priority_queue<std::string, std::vector<std::string>, CompareIntGate> pb_int_gates(CompareIntGate(*this));
		for (const auto &g : gates) {
			if (!isIntGate(g.first))
				continue;
			IntGate<T> *gate = asIntGate(g.first);
			if (gate->Y() != "t")
				pb_int_gates.push(g.first);
		}
		
		std::map<std::string, T> shifts;
		
		/* Modify all occurences of integration gate with second input which is not t */
		while (pb_int_gates.size() > 0) {
			std::string gate_name = pb_int_gates.top();
			//std::cerr << gate_name << std::endl;
            //std::cout << "#" << gate_name << "\n";
			//std::cout << toString() << "\n";
			
			pb_int_gates.pop();
			IntGate<T> *gate = asIntGate(gate_name);
			// There are three cases
			// Case 1: the second input of the gate is an integration gate with second input t
			if (isIntGate(gate->Y()) && asIntGate(gate->Y())->Y() == "t") {
				IntGate<T> *input_gate = asIntGate(gate->Y());
				const std::string &u = input_gate->X();
				const std::string &v = gate->X();
				std::string prod_gate = getNewGateName();
				addProductGate(prod_gate, u, v, false);
				gate->Y() = "t";
				gate->X() = prod_gate;
			}
			// Case 2: the second input of the gate is a product gate
			else if (isProductGate(gate->Y())) {
				ProductGate<T> *input_gate = asProductGate(gate->Y());
				const std::string &u = input_gate->X();
				const std::string &v = input_gate->Y();
				const std::string &w = gate->X();
				
				// If the second input is some gate multiplied by a constant, modify accordingly
				if (isCombinationConstantGates(u) || isCombinationConstantGates(v)) {
					std::string c_gate;
					std::string not_c_gate;
					if (isCombinationConstantGates(u)) {
						c_gate = u;
						not_c_gate = v;
					}
					if (isCombinationConstantGates(v)) {
						c_gate = v;
						not_c_gate = u;
					}
					
					std::string prod_gate = getNewGateName();
					addProductGate(prod_gate, c_gate, w, false);
					gate->X() = prod_gate;
					gate->Y() = not_c_gate;
					if (not_c_gate != "t")
						pb_int_gates.push(gate_name);
					continue;
				}
				
				std::string p1 = getNewGateName();
				std::string p2 = getNewGateName();
				addProductGate(p1, u, w, false);
				addProductGate(p2, w, v, false);
				std::string i1 = getNewGateName();
				std::string i2 = getNewGateName();
				addIntGate(i1, p1, v, false);
				if (guess_init_value && getValues().count(gate_name) > 0)
					setInitValue(i1, 0.5 * getValues().at(gate_name));
				if (v != "t")
					pb_int_gates.push(i1);
				addIntGate(i2, p2, u, false);
				if (guess_init_value && getValues().count(gate_name) > 0)
					setInitValue(i2, 0.5 * getValues().at(gate_name));
				if (u != "t")
					pb_int_gates.push(i2);
				gates[gate_name].reset(new AddGate<T>(i1,i2));
			}
			// Case 3: the second input of the gate is an addition gate
			else if (isAddGate(gate->Y())) {
				AddGate<T> *input_gate = asAddGate(gate->Y());
				const std::string &w = gate->X();
				const std::string &u = input_gate->X();
				const std::string &v = input_gate->Y();
				
				/* If it is of the form u+c ignore the constant */
				if (isCombinationConstantGates(u)) {
					gate->Y() = v;
					if (v != "t")
						pb_int_gates.push(gate_name);
				}
				else if (isCombinationConstantGates(v)) {
					gate->Y() = u;
					if (u != "t")
						pb_int_gates.push(gate_name);
				}
				else {
					std::string i1 = getNewGateName();
					std::string i2 = getNewGateName();
					addIntGate(i1, w, u, false);
					if (guess_init_value && getValues().count(gate_name) > 0)
						setInitValue(i1, 0.5 * getValues().at(gate_name));
					if (u != "t")
						pb_int_gates.push(i1);
					addIntGate(i2, w, v, false);
					if (guess_init_value && getValues().count(gate_name) > 0)
						setInitValue(i2, 0.5 * getValues().at(gate_name));
					if (v != "t")
						pb_int_gates.push(i2);
					gates[gate_name].reset(new AddGate<T>(i1,i2));
				}
			}
			// Problem if none of these 3 cases above: all remainning problematic integration gates cannot be simplified
			else {
				CircuitErrorMessage() << "Cannot normalize the circuit! Problem with gate " << gate_name << ".";
				std::cerr << *this << std::endl << std::endl;
				while (pb_int_gates.size() > 0) {
					std::cerr << pb_int_gates.top() << std::endl;
					pb_int_gates.pop();
				}
				exit(EXIT_FAILURE);
			}
		}
		
		return *this;
	}
	
	/*! \brief Validation of a circuit
	 * 
	 * Checks if the circuit is valid. More specifically, it checks:
	 *   - if the names of all gates are valid
	 *   - if the inputs of the binary gates are either gates present in the circuit or `t`
	 *   - if the integration gates have their second input equal to `t` (normalized circuit)
	 *   - if the output gate has been set to a valid gate in the circuit.
	 */
	GPAC<T> &validate() {
		if (finalized)
			return *this;
		for (const auto &g : gates) {
			if (!isBinaryGate(g.first))
				continue;
			const BinaryGate<T>* gate = asBinaryGate(g.first);
			if (!validation)
				validateGateName(g.first, false);
			if ((gate->X() != "t" && gates.count(gate->X()) == 0) || (gate->Y() != "t" && gates.count(gate->Y()) == 0)) {
				CircuitErrorMessage() << "Gate " << g.first << " has an input which is neither t or the output of a gate of the circuit!";
				exit(EXIT_FAILURE);
			}
			if (!validation && isIntGate(g.first) && gate->Y() != "t" && isConstantGate(gate->Y())) {
				CircuitErrorMessage() << "Integration gate " << g.first << " has its second input which is constant!";
				exit(EXIT_FAILURE);
			}
			if (isIntGate(g.first) && gate->Y() != "t") {
				CircuitErrorMessage() << "Integration gate " << g.first << " has its second input different from t. You should normalize the circuit before using it!";
				exit(EXIT_FAILURE);
			}
		}
		if (output_gate == "") {
			CircuitErrorMessage() << "Output gate has not been set!";
			exit(EXIT_FAILURE);
		}
		else if (output_gate != "t" && gates.count(output_gate) == 0) {
			std::cerr << output_gate << std::endl;
			for (const auto &g : gates) {
				std::cerr << g.first << ",";
			}
			std::cerr << std::endl;
			CircuitErrorMessage() << "Output gate is invalid!";
			exit(EXIT_FAILURE);
		}
		return *this;
	}
	
	/*! \brief Simplify the circuit as much as possible
	 *
	 * This method first delete duplicate constant gates, i.e. multiple constant gates with
	 * the same value are merged into one.
	 *
	 * Then it recursively tries to delete binary gates that have the same inputs, by merging them
	 * and trying again until a fixed point is reached.
	 */
	GPAC<T> &simplify(bool constants_only = false) {
		unsigned n_deletions = 0;
		if (finalized)
			return *this;
		
		/* Replace all gates corresponding to constants by constant gates, e.g. (1+1) -> 2 */
		for (auto &g : gates) {
			if (isCombinationConstantGates(g.first))
				g.second.reset(new ConstantGate<T>(valueCombinationConstantGates(g.first)));
		}
		
		/* Delete all gates that are not linked to the output */
		std::set<std::string> useless_gates;
		for (const auto &g : gates)
			useless_gates.insert(g.first);
		if (useless_gates.count(output_gate) > 0) {
			useless_gates.erase(output_gate);
			findUselessGates(useless_gates, output_gate);
		}
		for (const auto &g_name : useless_gates)
			eraseGate(g_name);
		
		/* Sort inputs of symmetric binary gates (i.e. addition and product) so that they always are in the same order */
		for (const auto &g : gates) {
			if (isAddGate(g.first) || isProductGate(g.first)) {
				BinaryGate<T> *gate = asBinaryGate(g.first);
				if (gate->X() > gate->Y()) {
					std::string temp = gate->X();
					gate->X() = gate->Y();
					gate->Y() = temp;
				}
			}
		}
		
		/* Identify different types of gates */
		std::vector<std::string> constant_names;
		std::vector<std::string> addition_names;
		std::vector<std::string> product_names;
		std::vector<std::string> integration_names;
		
		for (const auto &g : gates) {
			if (isConstantGate(g.first))
				constant_names.push_back(g.first);
			else if (isAddGate(g.first))
				addition_names.push_back(g.first);
			else if (isProductGate(g.first))
				product_names.push_back(g.first);
			else if (isIntGate(g.first))
				integration_names.push_back(g.first);
		}
		
		/* Prefer to keep user-defined names unchanged */
		std::sort(constant_names.begin(), constant_names.end(), PreferUserDefinedNames());
		std::sort(addition_names.begin(), addition_names.end(), PreferUserDefinedNames());
		std::sort(product_names.begin(), product_names.end(), PreferUserDefinedNames());
		std::sort(integration_names.begin(), integration_names.end(), PreferUserDefinedNames());
		
		/* First delete duplicate constants */
		std::map<std::string, std::string> new_names;
		for (const auto &name : constant_names)
			new_names[name] = name;
		for (unsigned i = 0; i<constant_names.size(); ++i) {
			const ConstantGate<T> *gate1 = asConstantGate(constant_names[i]);
			for (unsigned j = i+1; j<constant_names.size(); ++j) {
				const ConstantGate<T> *gate2 = asConstantGate(constant_names[j]);
				if (gate1->Constant() == gate2->Constant())
					new_names[constant_names[j]] = new_names[constant_names[i]];
			}
		}
		
		for (const auto &new_name : new_names) {
			if (new_name.first != new_name.second) {
				if (output_gate == new_name.first)
					output_gate = new_name.second;
				gates.erase(new_name.first);
				n_deletions++;
			}
		}
		
		// Rename all replaced constant gates
		for (const auto &g : gates) {
			if (!isConstantGate(g.first)) {
				BinaryGate<T>* gate = asBinaryGate(g.first);
				if (new_names.count(gate->X()) > 0)
					gate->X() = new_names[gate->X()];
				if (new_names.count(gate->Y()) > 0)
					gate->Y() = new_names[gate->Y()];
			}
		}
		
		if (constants_only)
			return *this;
		
		/* Recursively delete duplicate product, addition and integration gates  */
		new_names.clear();
		for (const auto &name : addition_names)
			new_names[name] = name;
		for (const auto &name : product_names)
			new_names[name] = name;
		for (const auto &name : integration_names)
			new_names[name] = name;
		bool changed = true;
		while (changed) {
			changed = false;
			// Determine all equal gates
			for (unsigned i = 0; i<addition_names.size(); ++i) {
				if (new_names.count(addition_names[i]) == 0)
					continue; // Gate was already removed
				const AddGate<T> *gate1 = asAddGate(addition_names[i]);
				for (unsigned j = i+1; j<addition_names.size(); ++j) {
					if (new_names.count(addition_names[j]) == 0)
						continue; // Gate was already removed
					const AddGate<T> *gate2 = asAddGate(addition_names[j]);
					if (gate1->X() == gate2->X() && gate1->Y() == gate2->Y()) {
						new_names[addition_names[j]] = new_names[addition_names[i]];
						changed = true;
					}
				}	
			}
			for (unsigned i = 0; i<product_names.size(); ++i) {
				if (new_names.count(product_names[i]) == 0)
					continue; // Gate was already removed
				const ProductGate<T> *gate1 = asProductGate(product_names[i]);
				for (unsigned j = i+1; j<product_names.size(); ++j) {
					if (new_names.count(product_names[j]) == 0)
						continue; // Gate cas already removed
					const ProductGate<T> *gate2 = asProductGate(product_names[j]);
					if (gate1->X() == gate2->X() && gate1->Y() == gate2->Y()) {
						new_names[product_names[j]] = new_names[product_names[i]];
						changed = true;
					}
				}	
			}
			for (unsigned i = 0; i<integration_names.size(); ++i) {
				if (new_names.count(integration_names[i]) == 0)
					continue; // Gate was already removed
				const IntGate<T> *gate1 = asIntGate(integration_names[i]);
				for (unsigned j = i+1; j<integration_names.size(); ++j) {
					if (new_names.count(integration_names[j]) == 0)
						continue; // Gate cas already removed
					const IntGate<T> *gate2 = asIntGate(integration_names[j]);
					if (gate1->X() == gate2->X() && gate1->Y() == gate2->Y()) {
						if (values.count(integration_names[i])>0 && values.count(integration_names[j])>0
							&& values[integration_names[i]] == values[integration_names[j]]) {
							new_names[integration_names[j]] = new_names[integration_names[i]];
							changed = true;
						}
					}
				}	
			}
			
			// If nothing to change, it's over
			if (!changed)
				break;
			
			// Now rename inputs accordingly
			for (const auto &g : gates) {
				if (!isConstantGate(g.first)) {
					BinaryGate<T>* gate = asBinaryGate(g.first);
					if (new_names.count(gate->X()) > 0)
						gate->X() = new_names[gate->X()];
					if (new_names.count(gate->Y()) > 0)
						gate->Y() = new_names[gate->Y()];
				}
			}
			
			// Finally delete useless gates
			for (const auto &new_name : new_names) {
				if (new_name.first != new_name.second) {
					if (output_gate == new_name.first)
						output_gate = new_name.second;
					gates.erase(new_name.first);
					new_names.erase(new_name.first);
					n_deletions++;
				}
			}
		}
		
		/* Now delete useless gates (gates with output not used) */
		changed = true;
		while (changed) {
			changed = false;
			std::map<std::string, bool> used_gates;
			used_gates[output_gate] = true;
			for (const auto &g : gates) {
				if (!isBinaryGate(g.first))
					continue;
				const BinaryGate<T> *gate = asBinaryGate(g.first);
				used_gates[gate->X()] = true;
				used_gates[gate->Y()] = true;
			}
			for (const auto &g : gates) {
				if (used_gates.count(g.first) == 0) {
					gates.erase(g.first);
					changed = true;
					n_deletions++;
				}
			}
		}
		
		if (n_deletions > 0) {
			std::cerr << "In circuit " << circuit_name << ": deleted " << n_deletions << " gate(s).\n\n";
		}
		return *this;
	}
	
	/* === Operators for constructing new circuits === */
	
	/// Rename gates in current circuit so that there is no name in common with the input circuit
	void ensureUniqueNames(const GPAC<T> &circuit) {
		std::map<std::string, std::string> new_names;
		for (const auto &g : gates) {
			if (circuit.has(g.first)) { // If there is a name shared
				new_names[g.first] = getNewGateName();
			}
		}
		
		/* Then replace gates with renamed ones */
		for (const auto &new_name : new_names) {
			gates[new_name.second] = std::move(gates[new_name.first]);
			gates.erase(new_name.first);
			if (values.count(new_name.first)) {
				T value = values[new_name.first];
				values.erase(new_name.first);
				values[new_name.second] = value;
			}
		}
		
		/* Replace output gate if necessary */
		if (new_names.count(output_gate) > 0)
			output_gate = new_names[output_gate];
		
		/* Finally rename inputs accordingly */
		for (const auto &g : gates) {
			if (!isBinaryGate(g.first))
				continue;
			BinaryGate<T>* gate = asBinaryGate(g.first);
			if (new_names.count(gate->X()) > 0)
				gate->X() = new_names[gate->X()];
			if (new_names.count(gate->Y()) > 0)
				gate->Y() = new_names[gate->Y()];
		}
	}
	
	/* ===== Operations between circuits  ===== */
	
	/// Returns a new circuit which represents the addition of the two circuits
	GPAC<T> operator+(const GPAC<T> &circuit) const {
		if (output_gate == "" || circuit.Output() == "") {
			CircuitErrorMessage() << "Can't add two circuits with no defined output!";
			exit(EXIT_FAILURE);
		}
		
		GPAC<T> result(*this);
		result.ensureUniqueNames(circuit);
		std::string old_output = result.Output();
		result.copyInto(circuit, false); // copy circuit in result
		result.setOutput(getNewGateName());
		result.addAddGate(result.Output(), old_output, circuit.Output(), false);
		return result;
	}
	/// Import the input circuit into the current one and add an addition gate between the two
	GPAC<T> &operator+=(const GPAC<T> &circuit) {
		if (output_gate == "" || circuit.Output() == "") {
			CircuitErrorMessage() << "Can't add two circuits with no defined output!";
			exit(EXIT_FAILURE);
		}
		
		ensureUniqueNames(circuit);
		std::string old_output = Output();
		copyInto(circuit, false); // copy circuit in result
		setOutput(getNewGateName());
		addAddGate(Output(), old_output, circuit.Output(), false);
		return *this;
	}
	/// Returns a new circuit which represents the product of the two circuits
	GPAC<T> operator*(const GPAC<T> &circuit) const {
		if (output_gate == "" || circuit.Output() == "") {
			CircuitErrorMessage() << "Can't multiply two circuits with no defined output!";
			exit(EXIT_FAILURE);
		}
		
		GPAC<T> result(*this);
		result.ensureUniqueNames(circuit);
		std::string old_output = result.Output();
		result.copyInto(circuit, false); // copy circuit in result
		result.setOutput(getNewGateName());
		result.addProductGate(result.Output(), old_output, circuit.Output(), false);
		return result;
	}
	/// Import the input circuit into the current one and add a product gate between the two
	GPAC<T> &operator*=(const GPAC<T> &circuit) {
		if (output_gate == "" || circuit.Output() == "") {
			CircuitErrorMessage() << "Can't multiply two circuits with no defined output!";
			exit(EXIT_FAILURE);
		}
		
		ensureUniqueNames(circuit);
		std::string old_output = Output();
		copyInto(circuit, false); // copy circuit in result
		setOutput(getNewGateName());
		addProductGate(Output(), old_output, circuit.Output(), false);
		return *this;
	}
	/// Returns a circuit representing the division with the input circuit
	GPAC<T> operator/(const GPAC<T> &circuit) {
		return (*this) * circuit.Inverse();
	}
	/// Import the inverse of the input circuit into the current one and add a product gate
	GPAC<T> &operator/=(const GPAC<T> &circuit) {
		*this *= circuit.Inverse();
		return *this;
	}
	/// \brief Returns a new circuit which represents the integration of the first one with respect to the second one with given initial value
	GPAC<T> Integrate(const GPAC<T> &circuit, T value) const {
		if (output_gate == "" || circuit.Output() == "") {
			CircuitErrorMessage() << "Can't combine circuits with no defined output!";
			exit(EXIT_FAILURE);
		}
		
		GPAC<T> result(*this);
		result.ensureUniqueNames(circuit);
		std::string old_output = result.Output();
		result.copyInto(circuit, false); // copy circuit in result
		result.setOutput(getNewGateName());
		result.addIntGate(result.Output(), old_output, circuit.Output(), false);
		result.setInitValue(result.Output(), value);
		return result;
	}
	/// \brief Recursive method used to compute derivative circuit
	/// \return Name of the gate representing derivate of subcircuit (replacing input gate)
	std::string DerivateGate(std::string gate_name) {
		std::string res;
		if (gate_name == "t")
			res =  addConstantGate("", 1, false);
		else if (isCombinationConstantGates(gate_name))
			res =  addConstantGate("", 0, false);
		else { 
			const BinaryGate<T> *gate = asBinaryGate(gate_name);
			if (isIntGate(gate_name)) {
				if (gate->Y() != "t") {
					CircuitErrorMessage() << "Can't compute derivate circuit of a circuit that is not normalize!";
					exit(EXIT_FAILURE);
				}
				res =  gate->X();
			}
			else {
				std::string const_gate = "";
				std::string other_gate = "";
				if (gate->X() != "t" && isCombinationConstantGates(gate->X())) {
					const_gate = gate->X();
					other_gate = gate->Y();
				}
				else if (gate->Y() != "t" && isCombinationConstantGates(gate->Y())) {
					const_gate = gate->Y();
					other_gate = gate->X();
				}
				
				if (const_gate != "") {
					std::string deriv = DerivateGate(other_gate);
					if (isAddGate(gate_name))
						res = deriv;
					else if (isProductGate(gate_name)) {
						res = addProductGate("", const_gate, deriv, false);
					}
				}
				else {				
					std::string deriv_x = DerivateGate(gate->X());
					std::string deriv_y = DerivateGate(gate->Y());
					if (isAddGate(gate_name)) {
						res =  addAddGate("", deriv_x, deriv_y, false);
					}
					else { // Else it is a product gate, use formula (xy)' = x'y + xy'
						std::string p1 = addProductGate("", deriv_x, gate->Y(), false);
						std::string p2 = addProductGate("", gate->X(), deriv_y, false);
						res =  addAddGate("", p1, p2, false);
					}
				}
			}
		}
		if (gate_name == output_gate)
			output_gate = res;
		return res;
	}
	/// \brief Returns the derivative of the circuit
	GPAC<T> Derivate() const {
		GPAC<T> res(*this);
		res.rename(res.Name() + "_der");
		res.DerivateGate(res.Output());
		return res;
	}
	/// \brief Returns the circuit computing the inverse
	GPAC<T> Inverse() const {
		GPAC<T> res(*this);
		double init = res.computeValue(0);
		res.rename(res.Name() + "_inv");
		
		res.DerivateGate(res.Output());
		
		std::string c = res.addConstantGate("", -1, false);
		std::string p1 = res.addProductGate("", c, res.Output());
		std::string p2 = getNewGateName();
		std::string z = res.addIntGate("", p2, "t", false);
		res.setInitValue(z, 1. / init);
		std::string p3 = res.addProductGate("",z,z,false);
		res.addProductGate(p2, p1, p3, false);
		
		res.setOutput(z);
		return res;
	}
	/// Returns a new circuit which represents the composition of the two circuits
	GPAC<T> operator()(const GPAC<T> &circuit) const {
		if (output_gate == "" || circuit.Output() == "") {
			CircuitErrorMessage() << "Can't compose two circuits with no defined output!";
			exit(EXIT_FAILURE);
		}
		// First check if one of the two circuits is the identity
		if (circuit.Output() == "t")
			return *this;
		if (output_gate == "t")
			return circuit;
		
		GPAC<T> result(circuit);
		GPAC<T> copy(*this);
		
		/* Compute new initial values */
		double b = circuit.computeValue(0);
		if (b > 0) {
			copy.finalize(false, false);
			copy.Simulate(0, b, 0.001);
			//std::cerr << copy.OutputValue() << std::endl;
		}
		else if (b < 0) {
			GPAC<T> id("Id", true, true);
			id.setOutput("t");
			GPAC<T> copy2 = copy(-id);
			copy2.finalize(false, false);
			copy2.Simulate(0, -b, 0.001);
			copy.importValues(copy2.getValues());
			//std::cerr << copy.OutputValue() << std::endl;
		}
			
		result.ensureUniqueNames(copy);
		std::string old_output = result.Output();
		result.copyInto(copy, false); // copy circuit in result
		
		
		/* Replace all instances of t in second circuit by the output of the first one */
		for (const auto &g : *this) {
			if (!result.isBinaryGate(g))
				continue;
			BinaryGate<T> *gate = result.asBinaryGate(g);
			if (gate->X() == "t")
				gate->X() = old_output;
			if (gate->Y() == "t")
				gate->Y() = old_output;
		}
		/* Set new output */
		result.setOutput(Output());
		result.normalize(); //  Normalize the circuit!
		return result;
	}
	/// Returns a new circuit which represents the addition of the circuit with a constant
	GPAC<T> operator+(T constant) const {
		GPAC<T> res(*this);
		bool found = false;
		// Check if constant is already in circuit
		for (const std::string &gate_name : res) {
			if (!isConstantGate(gate_name))
				continue;
			ConstantGate<T>* gate = res.asConstantGate(gate_name);
			if (gate->Constant() == constant) {
				found = true;
				std::string new_gate = getNewGateName();
				res.addAddGate(new_gate,res.Output(),gate_name,false);
				res.setOutput(new_gate);
				break;
			}
		}
		if (!found) {
			std::string constant_gate = getNewGateName();
			std::string output = getNewGateName();
			res.addConstantGate(constant_gate, constant,false);
			res.addAddGate(output,res.Output(),constant_gate,false);
			res.setOutput(output);
		}
		return res;
	}
	/// Add a constant gate (if it is not already in the circuit) and add an addition gate
	GPAC<T> &operator+=(T constant) {
		bool found = false;
		// Check if constant is already in circuit
		for (const std::string &gate_name : *this) {
			if (!isConstantGate(gate_name))
				continue;
			ConstantGate<T>* gate = asConstantGate(gate_name);
			if (gate->Constant() == constant) {
				found = true;
				std::string new_gate = getNewGateName();
				addAddGate(new_gate,Output(),gate_name,false);
				setOutput(new_gate);
				break;
			}
		}
		if (!found) {
			std::string constant_gate = getNewGateName();
			std::string output = getNewGateName();
			addConstantGate(constant_gate, constant,false);
			addAddGate(output,Output(),constant_gate,false);
			setOutput(output);
		}
		return *this;
	}
	/// Returns a new circuit which represents the substraction of the circuit with a constant
	GPAC<T> operator-(T constant) const {
		return *this + (-constant);
	}
	/// Subtract a constant to the circuit
	GPAC<T> &operator-=(T constant) {
		*this += (-constant);
		return *this;
	}
	/// Returns a new circuit which represents the product of the circuit with a constant
	GPAC<T> operator*(T constant) const {
		GPAC<T> res(*this);
		bool found = false;
		// Check if constant is already in circuit
		for (const std::string &gate_name : res) {
			if (!isConstantGate(gate_name))
				continue;
			ConstantGate<T>* gate = res.asConstantGate(gate_name);
			if (gate->Constant() == constant) {
				found = true;
				std::string new_gate = getNewGateName();
				res.addProductGate(new_gate,res.Output(),gate_name,false);
				res.setOutput(new_gate);
				break;
			}
		}
		if (!found) {
			std::string constant_gate = getNewGateName();
			std::string output = getNewGateName();
			res.addConstantGate(constant_gate, constant, false);
			res.addProductGate(output,res.Output(),constant_gate,false);
			res.setOutput(output);
		}
		return res;
	}
	/// Add a constant gate (if it is not already in the circuit) and add a product gate
	GPAC<T> &operator*=(T constant) {
		bool found = false;
		// Check if constant is already in circuit
		for (const std::string &gate_name : *this) {
			if (!isConstantGate(gate_name))
				continue;
			ConstantGate<T>* gate = asConstantGate(gate_name);
			if (gate->Constant() == constant) {
				found = true;
				std::string new_gate = getNewGateName();
				addProductGate(new_gate,Output(),gate_name,false);
				setOutput(new_gate);
				break;
			}
		}
		if (!found) {
			std::string constant_gate = getNewGateName();
			std::string output = getNewGateName();
			addConstantGate(constant_gate, constant, false);
			addProductGate(output,Output(),constant_gate,false);
			setOutput(output);
		}
		return *this;
	}
	/// Returns a circuit divided by a constant
	GPAC<T> operator/(T constant) const {
		return *this * (1./constant);
	}
	/// Divide the circuit by a constant
	GPAC<T> &operator/=(T constant) {
		*this *= (1./constant);
		return *this;
	}
	
	/// Returns a new circuit which is the circuit subtracted with the other circuit
	GPAC<T> operator-(const GPAC<T> &circuit) const {
		return *this + (circuit * (-1));
	}
	GPAC<T> operator-() const {
		return *this * (-1.);
	}
	
	/// Returns the circuit iterated with itself j times
	GPAC<T> Iterate(unsigned j) const {
		GPAC<T> res("");
		if (j == 0) {
			res.setOutput("t");
			return res;
		}
		if (j == 1)
			return *this;
		res = Iterate(j/2);
		res = res(res);
		if (j % 2 == 1)
			res = res(*this);
		return res;
	}
	
	/// Bracket operator for iterations of circuit
	GPAC<T> operator[](unsigned j) const {
		return Iterate(j);
	}
	
	/* ===== Simulation methods =====*/
	
	/*! \brief Set the init value of an integration gate
	 * \param gate_name Name of the integration gate to set the value
	 * \param value Value to be associated with the gate
	 */
	void setInitValue(std::string gate_name, T value) {
		if (!isIntGate(gate_name)) {
			CircuitErrorMessage() << "Can only set initial value for integration gate!";
			return;
		}
		if (values[gate_name] != value)
			finalized = false;
		values[gate_name] = value;
	}
	/// Returns the values associated to the names of the gates
	const std::map<std::string, T> &getValues() const {
		return values;
	}
	
	void importValues(const std::map<std::string, T> v) {
		for (const auto &g : v) {
			if (has(g.first))
				values[g.first] = g.second;
		}
	}
	
	/*! \brief Ensures that the circuit is finalized, i.e. that everything is set for simulating
	 * \param simplfication Enable the simplification of the circuit (default: true)
	 *
	 * A finalized circuit is a circuit that is normalized, simplified and validated. Moreover, it
	 * means that some data useful for simulating has been precomputed, like the list of integration
	 * gates.
	 *
	 * The `finalized` attribute of the circuit is set to true at the end of this method. Any 
	 * operation that modify the circuit set this attribute to false. If the `finalized` attribute is
	 * set to true, this method does nothing: the circuit is already finalized!
	 */
	GPAC<T> &finalize(bool simplification = true, bool print_result = true) {
		if (finalized)
			return *this;
		normalize();
		if (finalized)
			return *this;
		if (simplification)
			simplify();
		validate();
		// Check that all valid integration gate have initial values
		for (const auto &g : gates) {
			if (isIntGate(g.first) && asIntGate(g.first)->Y() == "t" && values.count(g.first) == 0) {
				CircuitErrorMessage() << "Cannot finalize circuit as valid integration gate " << g.first << " has no initial value set.";
				exit(EXIT_FAILURE);
			}
		}
		// Set all gates different from integration gate to "not computed"
		// and determine all integration gates
		int_gates.clear();
		for (const auto &g : gates) {
			if (!isIntGate(g.first) && values.count(g.first) > 0)
				values.erase(g.first);
			if (isIntGate(g.first))
				int_gates.push_back(g.first);
		}
	    
		finalized = true;
		
		if (print_result) {
			std::cerr << "Finalized circuit ";
			if (circuit_name == "")
				std::cerr << "<unknown> ";
			else
				std::cerr << circuit_name;
			std::cerr << " of size " << size() << ".\n" << std::endl;
		}
		
		return *this;
	}
	
	/// Set all gates different from integration gate or constant gates to "not computed"
	void resetNonIntValues() {
		for (const auto &g : gates) {
			if (!isIntGate(g.first) && !isConstantGate(g.first) && values.count(g.first) > 0)
				values.erase(g.first);
		}
	}
    /// Set initial values for constants gates
	GPAC<T> &initValues() {
		for (const auto &g : gates) {
			if (isConstantGate(g.first)) {
				values[g.first] = asConstantGate(g.first)->Constant();
			}
		}
		return *this;
	}
	/*! \brief Compute the values of all the gates other than integration gates
	 * \param t_0 Value for time `t`
	 * \pre Circuit should be finalized.
	 *
	 * Compute the values of all gates starting from constant gates and integration gates values.
	 * It recursively compute values of all gates for which the values of the two inputs are known,
	 * until all gates have values.
	 */
	GPAC<T> &computeValues(T t_0 = 0) {		
		values["t"] = t_0;
		
		// Then compute values by propagating already known values
		bool changed = true;
		while (changed) {
			changed = false;
			// Update value of all gates which has inputs having initial values
			for (const auto &g : gates) {
				if (values.count(g.first) > 0)
					continue; // Value already computed
				if (isAddGate(g.first) || isProductGate(g.first)) {
					const BinaryGate<T> *gate = asBinaryGate(g.first);
					if (values.count(gate->X()) > 0 && values.count(gate->Y()) > 0) {
						values[g.first] = gate->operator()(values[gate->X()], values[gate->Y()]);
						changed = true;
					}	
				}
			}
		}
		// Check that all gates have values
		for (const auto &g : gates) {
			if (values.count(g.first) == 0) {
				CircuitErrorMessage() << "Failed to compute values (fail for gate " << g.first << ")";
				exit(EXIT_FAILURE);
			}
		}
		return *this;
	}
	
	T computeValue(T t_0 = 0) const {
		std::map<std::string, T> copy_values = values;
		
		for (const auto &g : gates) {
			if (!isIntGate(g.first) && !isConstantGate(g.first) && copy_values.count(g.first) > 0)
				copy_values.erase(g.first);
		}
		for (const auto &g : gates) {
			if (isConstantGate(g.first)) {
				copy_values[g.first] = asConstantGate(g.first)->Constant();
			}
		}
		copy_values["t"] = t_0;
		
		// Then compute values by propagating already known values
		bool changed = true;
		while (changed) {
			changed = false;
			// Update value of all gates which has inputs having initial values
			for (const auto &g : gates) {
				if (copy_values.count(g.first) > 0)
					continue; // Value already computed
				if (isAddGate(g.first) || isProductGate(g.first)) {
					const BinaryGate<T> *gate = asBinaryGate(g.first);
					if (copy_values.count(gate->X()) > 0 && copy_values.count(gate->Y()) > 0) {
						copy_values[g.first] = gate->operator()(copy_values[gate->X()], copy_values[gate->Y()]);
						changed = true;
					}	
				}
			}
		}
		return copy_values.at(output_gate);
	}
	
	T OutputValue() const {
		return values.at(output_gate);
	}
	
	/*! \brief Step for simulating the circuit
	 * \param y Values of integration gates computed so far
	 * \param dydt Values of integration gates after propagating the previous values in the circuit
	 * \pre Vectors should be of the same size as int_gates
	 */
	void ODE(std::vector<T> &y, std::vector<T> &dydt, const double t) {
		resetNonIntValues();
		#pragma omp parallel for schedule(runtime)
		for (unsigned i = 0; i<y.size(); ++i) {
			values[int_gates[i]] = y[i];
		}
		
		computeValues(t);
		
		#pragma omp parallel for schedule(runtime)
		for (unsigned i = 0; i<dydt.size(); ++i) {
			const IntGate<T> *gate = asIntGate(int_gates[i]);
			dydt[i] = values[gate->X()];
		}
	}
	/*! \brief Simulating the circuit with Odeint
	 * \param a Initial value for t
	 * \param b Last value of t
	 * \param dt Step size
	 *
	 * Simulates the circuit using Odeint implementation of the Runge-Kutta method with
	 * fixed step size.
	 */
	GPAC<T> &Simulate(T a, T b, T dt) {
		if (!finalized) {
			CircuitErrorMessage() << "Cannot simulate a circuit if it is not finalized!";
			exit(EXIT_FAILURE);
		}
		initValues();
		std::vector<T> y(int_gates.size());
		for (unsigned i = 0; i<y.size(); ++i)
			y[i] = values[int_gates[i]];
		boost::numeric::odeint::runge_kutta4<std::vector<T>, T, std::vector<T>, T, boost::numeric::odeint::openmp_range_algebra > stepper;
		boost::numeric::odeint::integrate_const(stepper, std::bind(&GPAC<T>::ODE, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), y, a, b , dt);
		return *this;
	}
	
	/// Observer used for storing all computed values of the output gate during the simulation
	class OutputObserver {
	public:
		OutputObserver(const GPAC<T> &c, std::vector<T> &v, std::vector<T> &t) : circuit(c), values(v), times(t) {}
		
		/*! \brief Store times and values of output gate
		 * \param y Values computed by Odeint
		 * \param t Current time of the simulation
		 */
		void operator()(const std::vector<T> &y, double t) {
			values.push_back(circuit.getValues().at(circuit.Output()));
			times.push_back(t);
		}
	private:
		const GPAC<T> &circuit; /*!< Reference of the circuit we are simulating */
		std::vector<T> &values; /*!< Stores values of the output gate */
		std::vector<T> &times; /*!< Stores the times of the simulation */
	};
	
	/*! \brief Simulates the circuit and export results in Gnuplot
	 * \param a Initial value for t
	 * \param b Last value of t
	 * \param dt Step size
	 * \param pdf_file If specified, export the results to this file
	 *
	 * Circuit is simulated using OutputObserver and the data obtained is then exported to Gnuplot
	 * using the Gnuplot-iostream library.
	 */
	GPAC<T> &SimulateGnuplot(T a, T b, T dt, std::string pdf_file = "") {
		if (!finalized) {
			CircuitErrorMessage() << "Cannot simulate a circuit if it is not finalized!";
			exit(EXIT_FAILURE);
		}
		initValues();
		std::vector<T> y(int_gates.size());
		for (unsigned i = 0; i<y.size(); ++i)
			y[i] = values[int_gates[i]];
		boost::numeric::odeint::runge_kutta4<std::vector<T>, T, std::vector<T>, T, boost::numeric::odeint::openmp_range_algebra > stepper;
		std::vector<T> values;
		std::vector<T> times;
		computeValues(a);
		boost::numeric::odeint::integrate_const(stepper, std::bind(&GPAC<T>::ODE, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), y, a, b , dt, OutputObserver(*this, values, times));
		Gnuplot gp;
		if (pdf_file != "")
			gp << "set terminal pdf\n"
			   << "set output '" << pdf_file << "'\n";
		gp << "set xrange [" << a << ":" << b << "]\n"
		   << "set key left top\n"
		   << "plot '-' with lines title '" <<  circuit_name << "'\n";
		gp.send1d(boost::make_tuple(times, values));
		gp.close();
		return *this;
	}
	
	GPAC<T> &SimulateDump(T a, T b, T dt) {
		if (!finalized) {
			CircuitErrorMessage() << "Cannot simulate a circuit if it is not finalized!";
			exit(EXIT_FAILURE);
		}
		initValues();
		std::vector<T> y(int_gates.size());
		for (unsigned i = 0; i<y.size(); ++i)
			y[i] = values[int_gates[i]];
		boost::numeric::odeint::runge_kutta4<std::vector<T> > stepper;
		std::vector<T> values;
		std::vector<T> times;
		computeValues(a);
		boost::numeric::odeint::integrate_const(stepper, std::bind(&GPAC<T>::ODE, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), y, a, b , dt, OutputObserver(*this, values, times));
		for (unsigned i = 0; i<times.size(); ++i) {
			std::cout << times[i] << "\t" << values[i] << std::endl;
		}
		return *this;
	}
	
protected:
	static unsigned new_gate_id; ///< Static variable used for generating unique gate names
	bool validation; ///< Option for activating verification at each modification of the circuit
	bool block; ///< Specifies if the circuit is a builtin circuit (not user-defined)
	bool finalized; ///< Boolean indicating if the circuit is ready to be simulated
	std::map<std::string, T> values; ///< Numerical values of the outputs of the gates
	std::vector<std::string> int_gates; ///< Valid integration gates
	
	/// Returns a new unique gate number
	unsigned getNewGateId() const {return ++new_gate_id;}
	
    /// Returns a new unique gate name (number preceeded with an underscore)
	std::string getNewGateName() const {return "_" + boost::lexical_cast<std::string>(++new_gate_id) ;}
	
	/// If `name` ends with _<n> with n an integer, ensure that all new gates have number larger than n
	void ensureNewGateIdLargeEnough(std::string name) const {
		int i = name.size()-1;
		while (i >= 0 && '0' <= name[i] && name[i] <= '9')
			--i;
		if (i < 0 || static_cast<unsigned>(i) >= name.size()-1 || name[i] != '_')
			return;
		unsigned id = boost::lexical_cast<unsigned>(name.substr(i+1));
		
		if (id > new_gate_id)
			new_gate_id = id;
	}
	
	/// \brief Returns a AddGate pointer to the specified gate
	/// \pre A gate named `gate_name` must exist in the circuit and must be an addition gate.
	AddGate<T>* asAddGate(std::string gate_name)  {
		return (dynamic_cast<AddGate<T>*>(gates.at(gate_name).get()));
	}
	/// \brief Returns a ProductGate pointer to the specified gate
	/// \pre A gate named `gate_name` must exist in the circuit and must be a product gate.
	ProductGate<T>* asProductGate(std::string gate_name)  {
		return (dynamic_cast<ProductGate<T>*>(gates.at(gate_name).get()));
	}
	/// \brief Returns an IntGate pointer to the specified gate
	/// \pre A gate named `gate_name` must exist in the circuit and must be an integration gate.
	IntGate<T>* asIntGate(std::string gate_name)  {
		return (dynamic_cast<IntGate<T>*>(gates.at(gate_name).get()));
	}
	/// \brief Returns a ConstantGate pointer to the specified gate
	/// \pre A gate named `gate_name` must exist in the circuit and must be a constant gate.
	ConstantGate<T>* asConstantGate(std::string gate_name)  {
		return (dynamic_cast<ConstantGate<T>*>(gates.at(gate_name).get()));
	}
	/// \brief Returns a BinaryGate pointer to the specified gate
	/// \pre A gate named `gate_name` must exist in the circuit and must be a binary gate.
	BinaryGate<T>* asBinaryGate(std::string gate_name)  {
		return (dynamic_cast<BinaryGate<T>*>(gates.at(gate_name).get()));
	}
	
	/// Comparator class used for keeping user-defined names when merging gates
	class PreferUserDefinedNames {
	public:
		bool operator()(std::string x, std::string y) {
			if (y.size() == 0)
				return false;
			if (x.size() == 0)
				return true;
			if (x[0] != '_' && y[0] == '_')
				return true;
			if (x[0] == '_' && y[0] != '_')
				return false;
			return (x<y);
		}
	};
	
	/// Contains all the info about a term written in product form for export to  LaTeX
	class TermLaTeX {
	public:
		T constant_part; ///!< Multiplicative constant
		std::map<std::string, unsigned> add_part; ///!< Multiplicative factors that are additions
		std::map<unsigned, unsigned> variables; ///!< Variables appearing in the product with their multiplicities (including t)
		
		/// Constructor for a constant
		TermLaTeX(double c) : constant_part(c), add_part(), variables() {}
		/// Constructor for a variable x_i, or t if i = 0
		TermLaTeX(unsigned i) : constant_part(1), add_part(), variables() {
			variables[i]++;
		}
		TermLaTeX() : constant_part(1), add_part(), variables() {}
		
		bool isConstant() const { return (add_part.size() == 0 && variables.size() == 0); }
		bool isAdd() const { return (add_part.size() > 0 && variables.size() == 0); }
		bool isProduct() const { return variables.size() > 0; }
		
		/// Returns the string correponding to the term
		std::string toString(bool with_add_constants = false) const {
			if (constant_part == 0)
				return "0";
			std::stringstream s("");
			if (!isConstant() && constant_part == -1)
				s << "-";
			else if (isConstant() || constant_part != 1)
				s << constant_part;
			for (const auto &a : add_part) {
				if ((constant_part != 1 || variables.size() > 0 || add_part.size() > 1 || a.second > 1) && a.first.size() > 0 && a.first[0] != '(')
					s << "(" << a.first << ")";
				else
					s << a.first;
				if (a.second > 1)
					s << "^{" << a.second << "}";
			}
			for (const auto &v : variables) {
				if (v.first == 0) // Skip t because we want to add it at the end
					continue;
				s << "x_{" << v.first << "}";
				if (v.second > 1)
					s << "^{" << v.second << "}"; 
			}
			if (variables.count(0) > 0 && variables.at(0) > 0) {
				s << "t";
				if (variables.at(0) > 1)
					s << "^{" << variables.at(0) << "}";
			}
			return s.str();
		}
		
		/// Addition of two terms
		TermLaTeX operator+(const TermLaTeX &term) const {
			if (isConstant() && term.isConstant())
				return TermLaTeX(static_cast<T>(constant_part + term.constant_part));
			
			TermLaTeX result;
			if (term.constant_part < 0) {
				TermLaTeX term2 = term;
				term2.constant_part *= -1;
				result.add_part[toString() + " - " + term2.toString()] = 1;
			}
			else
				result.add_part[toString() + " + " + term.toString()] = 1;
			return result;
		}
		
		/// Product of two terms
		TermLaTeX operator*(const TermLaTeX &term) const {
			TermLaTeX result;
			result.constant_part = constant_part * term.constant_part; // Multiply constants
			
			result.add_part = add_part; // Copy addition part
			for (const auto &a : term.add_part) {
				result.add_part[a.first] += a.second;
			}
			
			result.variables = variables; // Copy variable map
			for (const auto &v : term.variables) {
				result.variables[v.first] += v.second;
			}
			return result;
		}
	};
	
	/// Recursive function to determine the Term corresponding to the given gate
	TermLaTeX toTermLaTeXGate(const std::map<std::string, unsigned> &int_gate_numbers, std::string gate_name) const {
		if (gate_name == "t")
			return TermLaTeX(static_cast<unsigned>(0));
		if (isConstantGate(gate_name))
			return TermLaTeX(static_cast<T>(asConstantGate(gate_name)->Constant()));
		if (isIntGate(gate_name))
			return TermLaTeX(static_cast<unsigned>(int_gate_numbers.at(gate_name)));
		const BinaryGate<T> *gate = asBinaryGate(gate_name);
		if (isAddGate(gate_name))
			return (toTermLaTeXGate(int_gate_numbers, gate->X()) + toTermLaTeXGate(int_gate_numbers, gate->Y()));
	    return (toTermLaTeXGate(int_gate_numbers, gate->X()) * toTermLaTeXGate(int_gate_numbers, gate->Y()));
	}
	
	/// Recursive function to determine gates that are not linked to the given gate
	void findUselessGates(std::set<std::string> &gates, std::string gate_name) const {
		if (gate_name == "t" || isConstantGate(gate_name))
			return;
		const BinaryGate<T> *gate = asBinaryGate(gate_name);
		bool treat_X = false;
		bool treat_Y = false;
		if (gates.count(gate->X())) {
			gates.erase(gate->X());
			treat_X = true;
		}
		if (gates.count(gate->Y())) {
			gates.erase(gate->Y());
			treat_Y = true;
		}
		if (treat_X)
			findUselessGates(gates, gate->X());
		if (treat_Y)
			findUselessGates(gates, gate->Y());
	}
};

/* ===== Extern operators ===== */

/// Operator for addition with a constant on the right
template<typename T>
GPAC<T> operator+(T constant, const GPAC<T> &circuit) {
	return circuit + constant;
}
	
/// Operator for subtraction with a constant on the right
template<typename T>
GPAC<T> operator-(T constant, const GPAC<T> &circuit) {
	return (-circuit) + constant;
}

/// Operator for multiplication with a constant on the right
template<typename T>
GPAC<T> operator*(T constant, const GPAC<T> &circuit) {
	return circuit * constant;
}
	
/// Operator for division of a constant by a circuit
template<typename T>
GPAC<T> operator/(T constant, const GPAC<T> &circuit) {
	return constant * circuit.Inverse();
}

/// Stream operator for printing circuits
template<typename T>
std::ostream &operator<<(std::ostream &os, const GPAC<T> &circuit) {
	os << circuit.toString();
	return os;
}

/* Initializing static variable */

template<typename T>
unsigned GPAC<T>::new_gate_id = 0;
	
/* ===== Some useful builtin circuits ===== */
	
/*! \brief %Circuit composed of only one constant gate of the given value
 * \remark Use this only as a standalone circuit! Otherwise use operators between constants and circuits.
 */
template<typename T>
GPAC<T> Constant(T constant) {
	GPAC<T> res("Const", true, true);
	res("c", constant);
	res.setOutput("c");
	return res;
}

/// %Circuit computing the identity function
template<typename T>
GPAC<T> Identity() {
	GPAC<T> res("Id", true, true);
	res.setOutput("t");
	return res;
}

/// %Circuit computing the exponential function
template<typename T> 
GPAC<T> Exp() {
	GPAC<T> res("Exp", true, true);
	res("exp", "I", "exp", "t");
	res.setOutput("exp");
	res.setInitValue("exp", 1);
	return res;
}
	
/// %Circuit computing the function 2^t
template<typename T>
GPAC<T> Exp2() {
	return Exp<T>()(log(2) * Identity<T>());
}

/// %Circuit computing the sine function
template<typename T> 
GPAC<T> Sin() {
	GPAC<T> res("Sin", true, true);
	res
		("sin_c", -1)
		("sin_P","x","sin","sin_c")
		("cos","I","sin_P","t")
		("sin","I","cos","t");
	res.setOutput("sin");
	res.setInitValue("cos", 1);
	res.setInitValue("sin", 0);
	return res;
}

/// %Circuit computing the cosine function
template<typename T>
GPAC<T> Cos() {
	GPAC<T> res("Cos", true, true);
	res
		("cos_c", -1)
		("cos_P","x","sin","cos_c")
		("cos","I","cos_P","t")
		("sin","I","cos","t");
	res.setOutput("cos");
	res.setInitValue("cos", 1);
	res.setInitValue("sin", 0);
	return res;
}
	
/// %Circuit computing the tangent function
template<typename T>
GPAC<T> Tan() {
	GPAC<T> res("Tan", true, true);
	res
		("c", 1)
		("tan2", "*", "tan", "tan")
		("add", "+", "c", "tan2")
		("tan", "I", "add", "t");
	res.setOutput("tan");
	res.setInitValue("tan", 0);
	return res;
}

/// %Circuit computing the arctan function
template<typename T>
GPAC<T> Arctan() {
	GPAC<T> res("Arctan", true, true);
	res
		("c", -2)
		("der", "I", "p3", "t")
		("p1", "x", "c", "t")
		("p2", "x", "der", "der")
		("p3", "x", "p1", "p2")
		("arctan", "I", "der", "t");
	res.setOutput("arctan");
	res.setInitValue("der", 1);
	res.setInitValue("arctan", 0);
	return res;
}
	
/// %Circuit computing the hyperbolic tangent function
template<typename T>	
GPAC<T> Tanh() {
	GPAC<T> res("Tanh", true, true);
	
	res
		("Tanh_c1", 1)
		("Tanh_c2", -1)
		("Tanh_p1", "*", "Tanh_out", "Tanh_out")
		("Tanh_p2", "*", "Tanh_c2", "Tanh_p1")
		("Tanh_a", "+", "Tanh_c1", "Tanh_p2")
		("Tanh_out", "I", "Tanh_a", "t");
	res.setInitValue("Tanh_out", 0);
	res.setOutput("Tanh_out");
	return res;
}

/// %Circuit computing the function 1/(1+t)
template<typename T>
GPAC<T> Inverse() {
	GPAC<T> res("Inverse", true, true);
	res
		("c", -1)
		("p", "x", "inv", "inv")
		("p2", "x", "c", "p")
		("inv", "I", "p2", "t");
	res.setOutput("inv");
	res.setInitValue("inv", 1);
	return res;
}
	
/// %Circuit computing the square root function
template<typename T>
GPAC<T> Sqrt() {
	GPAC<T> res("Sqrt", true, true);
	res
		("Sqrt_c", -0.5)
		("Sqrt_p1", "*", "Sqrt_out", "Sqrt_out")
		("Sqrt_p2", "*", "Sqrt_p1", "Sqrt_out")
		("Sqrt_p3", "*", "Sqrt_p2", "Sqrt_c")
		("Sqrt_out", "I", "Sqrt_p3", "t");
	res.setOutput("Sqrt_out");
	res.setInitValue("Sqrt_out", 20.);
	return res.Inverse();
}

/// %Circuit computing \f$t^{2^n} \f$
template<typename T>
GPAC<T> PowerPower2(unsigned n) {
	GPAC<T> res("PP2" + boost::lexical_cast<std::string>(n), true, true);
	if (n == 0) {
		res("c1", 1.);
		res.setOutput("c1");
		return res;
	}
	
	res("P1","x","t","t");
	res.setOutput("P1");
	for (unsigned i = 0; i<n-1; i++) {
		res = res * res;
	}
	return res;
}

/*! \brief %Circuit computing a polynomial using Horner's method
 * \param coeffs Coefficients given in the increasing degree order.
 */
template<typename T>
GPAC<T> Polynomial(const std::vector<T> &coeffs) {
	if (coeffs.size() == 0)
		return Constant<T>(0);
	
	GPAC<T> res("Poly", true, true);
	res("c", coeffs[coeffs.size()-1]);
	res.setOutput("c");
	
	for (int i = coeffs.size() - 1; i >= 0; --i) {
		res *= Identity<T>();
		if (coeffs[i] != 0)
			res += coeffs[i]; 
	}
	return res;
}
	
/*! \brief %Circuit useful for simulating a switch
 * \param alpha Stricly positive value corresponding to the precision of the switch
 */
template<typename T>
GPAC<T> L2(T alpha) {
	T y = 1./alpha;
	// GPAC<T> res = 0.5 + (1./boost::math::constants::pi<T>()) * Arctan<T>()(4 * y * Identity<T>());
	GPAC<T> res("L2", true, true);
	res = 0.5 + (1. / boost::math::constants::pi<T>()) * Arctan<T>()(4. * y * (Identity<T>() + (- 0.5)));
	res.rename("L2");
	/*res
		("L2_1", 4*y)
		("L2_2", "*", "L2_1", "L2_t2")
		("L2_3", "*", "L2_1", "L2_p3")
		("L2_4", "*", "L2_1", "L2_der")
		("L2_5", 1./boost::math::constants::pi<T>())
		("L2_6", "*", "L2_5", "L2_arctan")
		("L2_7", 0.5)
		("L2_arctan", "I", "L2_4", "t")
		("L2_c", -2)
		("L2_c2", -0.5)
		("L2_der", "I", "L2_3", "t")
		("L2_p1", "*", "L2_2", "L2_c")
		("L2_p2", "*", "L2_der", "L2_der")
		("L2_p3", "*", "L2_p1", "L2_p2")
		("L2_t2", "+", "L2_c2", "t")
		("L2_8", "+", "L2_6", "L2_7");
	res.setOutput("L2_8");
	res.setInitValue("L2_arctan", atan(-2. * y));
	res.setInitValue("L2_der", 1./(1.+4.*y*y));*/
	
	return res;
}

/*! \brief %Circuit useful for reducing errors
 * \param circuit Circuit computing the error-controlling function
 */
template<typename T>
GPAC<T> L2(const GPAC<T> &circuit) {
	GPAC<T> res("L2", true, true);
	res = 0.5 + (1. / boost::math::constants::pi<T>()) * Arctan<T>()(4. * circuit * (Identity<T>() + (- 0.5)));
	res.rename("L2");
	
	/*GPAC<T> res = L2<T>(0.1);
	GPAC<T> Y = circuit;
	
	Y.ensureUniqueNames(res);
	res.copyInto(Y, false);
	res.eraseGate("L2_1");
	std::string c = res.addConstantGate("", 4, false);
	std::string p = res.addProductGate("", c, Y.Output(), false);
	res.renameInputs("L2_1", p);
	
	double y = circuit.computeValue(0);
	res.setInitValue("L2_arctan", atan(-2. * y));
	res.setInitValue("L2_der", 1./(1.+4.*y*y));*/
	
	return res;
}
	
/*! \brief %Circuit for switching between two functions depending on a control function
 */
template<typename T>
GPAC<T> Switching(const GPAC<T> &C1, const GPAC<T> &C2, const GPAC<T> &Y, double alpha) {
	GPAC<T> temp = L2<T>(10. * (C1 + C2));
	return C1 * temp(alpha + 0.5 + (- Y))
		+ C2 * temp(0.5 - alpha + Y);
}	
	
/// \brief %Circuit useful to get rectangular signal
template<typename T>
GPAC<T> Upsilon() {
	GPAC<T> res("Upsilon", true, true);
	res
		("Upsilon_2", "*", "Upsilon_c", "Upsilon_cos")
		("Upsilon_3", "*", "Upsilon_c", "Upsilon_sin_P")
		("Upsilon_4", 2)
		("Upsilon_5", "*", "Upsilon_4", "Upsilon_sin")
		("Upsilon_6", 1)
		("Upsilon_7", "+", "Upsilon_5", "Upsilon_6")
		("Upsilon_c", 2. * boost::math::constants::pi<T>())
		("Upsilon_cos", "I", "Upsilon_3", "t")
		("Upsilon_sin", "I", "Upsilon_2", "t")
		("Upsilon_sin_P", "*", "Upsilon_sin", "Upsilon_sin_c")
		("Upsilon_sin_c", -1);
	res.setOutput("Upsilon_7");
	res.setInitValue("Upsilon_cos", 0);
	res.setInitValue("Upsilon_sin", 1);
	
	return res;
}
	
/// \brief %Circuit apprixmating rounding function
template<typename T>
GPAC<T> Round() {
	GPAC<T> res = Identity<T>() - 0.2 * Sin<T>()(2 * boost::math::constants::pi<T>() * Identity<T>());
	res.rename("Round");
	return res;
}
	
/// \brief %Circuit approximating the mod 10 function
template<typename T>
GPAC<T> Mod10() {
	GPAC<T> csin = Sin<T>();
	GPAC<T> ccos = Cos<T>();
	GPAC<T> ct = Identity<T>();
	GPAC<T> res("Mod10", true, true);

	using namespace boost::numeric::ublas;
	
	/* Compute coefficients for trigonometric interpolation */
	matrix<T> A(10,10);
	vector<T> y(10);
	T pi = boost::math::constants::pi<T>();
	for (unsigned i = 0; i<10; i++) {
		y(i) = i; //w(i) = i
		A(i,0) = 1; //a_0
		A(i,9) = cos(pi * i); //a_5
		for (unsigned j = 1; j<=4; j++) {
			A(i,j) = cos(j*i*pi/5.); // a_j
			A(i,4+j) = sin(j*i*pi/5.); //b_j
		}
	}
	permutation_matrix<size_t> pm(A.size1());
	lu_factorize(A, pm);
	lu_substitute(A, pm, y); // y contains the coefficients
	
	res = Constant<T>(y(0));
	res += y(9) * ccos(pi*ct);
	for (unsigned j = 1; j<= 4; j++) {
		res += y(j) * ccos(j * (pi / 5.) * ct);
		res += y(4+j) * csin(j * (pi / 5.) * ct);
	}
	
	return res;
}
	
/** Functions defined in Amaury Pouly's thesis **/

/// \brief %Circuit computing absolute value with error delta
template<typename T>
GPAC<T> Abs(T delta) {
	return delta + Tanh<T>()((1./delta) * Identity<T>()) * Identity<T>();
}

/// \brief %Circuit computing the sign function
template<typename T>
GPAC<T> Sgn(T mu) {
	return Tanh<T>()((1./mu) * Identity<T>());
}

/// \brief %Circuit computing a switch between value 0 and 1 at t=1
template<typename T>
GPAC<T> Ip1(T mu) {
	return 0.5 * (1. + Sgn<T>(mu)(Identity<T>() - 1.));
}

/// \brief %Circuit computing a switch between value 0 for t <= a and x for t >= b
template<typename T>
GPAC<T> Lxh(T a, T b, T mu, T x) {
	T delta = 0.5*(b-a);
	T nu = mu + log(1. + x*x);
	return x * Ip1(nu * (1./delta))(Identity<T>() - (a + b) / 2. + 1.);
}
	
/// \brief %Circuit computing a switch between value x for t <= a and y for t >= b
template<typename T>
GPAC<T> Select(T a, T b, T mu, T x, T y) {
	return x + Lxh<T>(a, b, mu, y-x);
}

/// \brief Returns a circuit computing the max of two circuits with error delta
template<typename T>
GPAC<T> Max(const GPAC<T> &X, const GPAC<T> &Y, T delta) {
	return 0.5 * (Y + X + Abs<T>(2*delta)(Y-X));
}

}

#endif
