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
#include <boost/numeric/odeint.hpp>
#include <boost/tuple/tuple.hpp>

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
	 * \param gate_name Name of the gate to be added
	 * \param x Name of the first input gate
	 * \param y Name of the second input gate
	 * \param validate If true, validate gate name before adding it, even if the circuit `validation` attribute is set to false (default: true)
	 *
	 * Add an addition gate to the circuit with specified name and inputs. If there is already a gate
	 * with the given name, a warning is issued and the existing gate is overwritten.
	 */
    void addAddGate(std::string gate_name, std::string x, std::string y, bool validate = true) {
		finalized = false;
		if (validation && validate)
			validateGateName(gate_name);
		if (gates.count(gate_name) > 0) {
			CircuitWarningMessage() << "Gate \"" << gate_name << "\" already exists, adding it again will overwrite it!";
			gates[gate_name].reset(new AddGate<T>(x,y));
		}
		else
			gates[gate_name] = std::unique_ptr<Gate>(new AddGate<T>(x,y));
	}
	
    /*! \brief Adding a product gate
	 * \param gate_name Name of the gate to be added
	 * \param x Name of the first input gate
	 * \param y Name of the second input gate
	 * \param validate If true, validate gate name before adding it, even if the circuit `validation` attribute is set to false (default: true)
	 *
	 * Add a product gate to the circuit with specified name and inputs. If there is already a gate
	 * with the given name, a warning is issued and the existing gate is overwritten.
	 */
    void addProductGate(std::string gate_name, std::string x, std::string y, bool validate = true) {
		finalized = false;
		if (validation && validate)
			validateGateName(gate_name);
		if (gates.count(gate_name) > 0) {
			CircuitWarningMessage() << "Gate \"" << gate_name << "\" already exists, adding it again will overwrite it!";
			gates[gate_name].reset(new ProductGate<T>(x,y));
		}
		else
			gates[gate_name] = std::unique_ptr<Gate>(new ProductGate<T>(x,y));
	}
	
	/*! \brief Adding an integration gate
	 * \param gate_name Name of the gate to be added
	 * \param x Name of the integrand gate name
	 * \param y Name of the gate with respect to which is integration is done
	 * \param validate If true, validate gate name before adding it, even if the circuit `validation` attribute is set to false (default: true)
	 *
	 * Add an integration to the circuit with specified name and inputs. If there is already a gate
	 * with the given name, a warning is issued and the existing gate is overwritten.
	 */
    void addIntGate(std::string gate_name, std::string x, std::string y, bool validate = true) {
		finalized = false;
		if (validation && validate)
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
	}
	
	/*! \brief Adding a constant gate
	 * \param gate_name Name of the gate to be added
	 * \param value Value of the gate
	 * \param validate If true, validate gate name before adding it, even if the circuit `validation` attribute is set to false (default: true)
	 *
	 * Add a constant gate to the circuit with specified name and value. If there is already a gate
	 * with the given name, a warning is issued and the existing gate is overwritten.
	 */
    void addConstantGate(std::string gate_name, T value, bool validate = true) {
		finalized = false;
		if (validation && validate)
			validateGateName(gate_name);
		if (gates.count(gate_name) > 0) {
			CircuitWarningMessage() << "Gate \"" << gate_name << "\" already exists, adding it again will overwrite it!";
			gates[gate_name].reset(new ConstantGate<T>(value));
		}
		else
			gates[gate_name] = std::unique_ptr<Gate>(new ConstantGate<T>(value));
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
			prefix_line = "\t";
		}
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
	std::string toDot() const {
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
		if (integration_names.size() > 0) {
			res << "\tnode [label = \"∫\"];\n";
			for (const auto &gate_name: integration_names) {
				res << "\t" << gate_name;
				if (output_gate == gate_name)
					res << " [color = red, fontcolor = red, peripheries = 2]";
				res << ";\n";
			}
			res << "\n";
		}
		
		/* Now define edges */
		for (const auto &g: gates) {
			if (isBinaryGate(g.first)) {
				const BinaryGate<T> *gate = asBinaryGate(g.first);
				res << "\t" << gate->X() << " -> " << g.first << ";\n";
				res << "\t" << gate->Y() << " -> " << g.first;
				if (isIntGate(g.first))
					res << " [style = dashed]";
				res << ";\n";
			}
		}
		res << "}\n";
		
		return res.str();
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
				return true;
			}
			if (circuit.isIntGate(gate2->Y()) && circuit.asIntGate(gate2->Y())->Y() == "t")
				return false;
			if (circuit.isProductGate(gate1->Y()))
				return true;
			if (circuit.isProductGate(gate2->Y()))
				return false;
			if (circuit.isAddGate(gate1->Y()))
				return true;
			if (circuit.isAddGate(gate2->Y()))
				return false;
			return x < y;
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
		
		/* Modify all occurences of integration gate with second input which is not t */
		while (pb_int_gates.size() > 0) {
			std::string gate_name = pb_int_gates.top();
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
				if ((u != "t" && isConstantGate(u)) || (v != "t" && isConstantGate(v))) {
					std::string c_gate;
					std::string not_c_gate;
					if (u != "t" && isConstantGate(u)) {
						c_gate = u;
						not_c_gate = v;
					}
					if (v != "t" && isConstantGate(v)) {
						c_gate = v;
						not_c_gate = u;
					}
					
					std::string prod_gate = getNewGateName();
					addProductGate(prod_gate, c_gate, w, false);
					gate->X() = prod_gate;
					gate->Y() = not_c_gate;
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
			// Problem if none of these 3 cases above: all remainning problematic integration gates cannot be simplified
			else {
				CircuitErrorMessage() << "Cannot normalize the circuit!";
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
	GPAC<T> &simplify() {
		unsigned n_deletions = 0;
		if (finalized)
			return *this;
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
						new_names[integration_names[j]] = new_names[integration_names[i]];
						changed = true;
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
		
		if (n_deletions > 0) {
			std::cerr << "In circuit " << circuit_name << ": deleted " << n_deletions << " gates.\n\n";
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
		result.ensureUniqueNames(*this);
		std::string old_output = result.Output();
		result.copyInto(*this, false); // copy circuit in result
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
		result.normalize(); // Normalize the circuit!
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
	GPAC<T> &finalize(bool simplification = true) {
		if (finalized)
			return *this;
		normalize();
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
		
		std::cerr << "Finalized circuit ";
		if (circuit_name == "")
			std::cerr << "<unknown> ";
		else
			std::cerr << circuit_name;
		std::cerr << " of size " << size() << ".\n" << std::endl;
		
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
	
	/*! \brief Step for simulating the circuit
	 * \param y Values of integration gates computed so far
	 * \param dydt Values of integration gates after propagating the previous values in the circuit
	 * \pre Vectors should be of the same size as int_gates
	 */
	void ODE(std::vector<T> &y, std::vector<T> &dydt, const double t) {
		resetNonIntValues();
		for (unsigned i = 0; i<y.size(); ++i) {
			values[int_gates[i]] = y[i];
		}
		computeValues(t);
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
		boost::numeric::odeint::runge_kutta4<std::vector<T> > stepper;
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
		boost::numeric::odeint::runge_kutta4<std::vector<T> > stepper;
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
};

/* ===== Extern operators ===== */

/// Operator for addition with a constant on the right
template<typename T>
GPAC<T> operator+(T constant, const GPAC<T> &circuit) {
	return circuit + constant;
}

/// Operator for multiplication with a constant on the right
template<typename T>
GPAC<T> operator*(T constant, const GPAC<T> &circuit) {
	return circuit * constant;
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
	
}

#endif
