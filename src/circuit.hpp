#ifndef CIRCUIT_HPP_
#define CIRCUIT_HPP_

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
#include "gnuplot-iostream/gnuplot-iostream.h"

namespace GPAClib {

using GatesMap = std::map<std::string, std::unique_ptr<Gate> >;

class CircuitConstIterator : public GatesMap::const_iterator {
public:
	CircuitConstIterator() : GatesMap::const_iterator() {}
	CircuitConstIterator(GatesMap::const_iterator it_) : GatesMap::const_iterator(it_) {}
	
	const std::string *operator->() {return (const std::string * const) &( GatesMap::const_iterator::operator->()->first);}
	std::string operator*() {return GatesMap::const_iterator::operator*().first;}
};


class Circuit {
public:
	Circuit(std::string name = "") : circuit_name(name), gates(), output_gate("") {}
	
	const GatesMap &Gates() const {return gates;}
	
	CircuitConstIterator begin() const {return CircuitConstIterator(gates.cbegin());}
	CircuitConstIterator end() const {return CircuitConstIterator(gates.cend());}
	
	const std::string &Output() const {return output_gate;}
	void setOutput(std::string output) {output_gate = output;}
	
	const std::string &Name() const {return circuit_name;}
	void rename(std::string name) {circuit_name = name;}
	ErrorMessage CircuitErrorMessage() const {
		return ErrorMessage("circuit " + circuit_name);
	}
	WarningMessage CircuitWarningMessage() const {
		return WarningMessage("circuit " + circuit_name);
	}
	
protected:
	std::string circuit_name;
    GatesMap gates;
	std::string output_gate;
};

template<typename T>
class GPAC : public Circuit {
public:
	/* Constructors */
	GPAC(std::string name = "", bool validation_ = true, bool block_ = false) : Circuit(name), validation(validation_), block(block_), finalized(false) {}
	
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
	
	// Copy all gates of circuit in current circuit
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
			//if (circuit.isIntGate(g) && circuit.asIntGate(g)->Y() == "t" && circuit.getValues().count(g) > 0)
			if (circuit.isIntGate(g) && circuit.getValues().count(g) > 0)
				setInitValue(g, circuit.getValues().at(g));
		}
	}
	
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
	bool Block() const {return block;}
	bool Validation() const {return validation;}
	bool has(std::string gate_name) const {
		return (gates.count(gate_name) != 0);
	}
	bool isAddGate(std::string gate_name) const {
		return (dynamic_cast<AddGate<T>*>(gates.at(gate_name).get()) != nullptr);
	}
	bool isProductGate(std::string gate_name) const {
		return (dynamic_cast<ProductGate<T>*>(gates.at(gate_name).get()) != nullptr);
	}
	bool isIntGate(std::string gate_name) const {
		return (dynamic_cast<IntGate<T>*>(gates.at(gate_name).get()) != nullptr);
	}
	bool isConstantGate(std::string gate_name) const {
		return (dynamic_cast<ConstantGate<T>*>(gates.at(gate_name).get()) != nullptr);
	}
	bool isBinaryGate(std::string gate_name) const {
		return (dynamic_cast<BinaryGate<T>*>(gates.at(gate_name).get()) != nullptr);
	}
	const AddGate<T>* asAddGate(std::string gate_name) const {
		return (dynamic_cast<AddGate<T>*>(gates.at(gate_name).get()));
	}
	const ProductGate<T>* asProductGate(std::string gate_name) const {
		return (dynamic_cast<ProductGate<T>*>(gates.at(gate_name).get()));
	}
	const IntGate<T>* asIntGate(std::string gate_name) const {
		return (dynamic_cast<IntGate<T>*>(gates.at(gate_name).get()));
	}
	const ConstantGate<T>* asConstantGate(std::string gate_name) const {
		return (dynamic_cast<ConstantGate<T>*>(gates.at(gate_name).get()));
	}
	const BinaryGate<T>* asBinaryGate(std::string gate_name) const {
		return (dynamic_cast<BinaryGate<T>*>(gates.at(gate_name).get()));
	}
	
	std::string toString() const {
		std::stringstream res("");
		std::string prefix_line = "";
		if (circuit_name != "") {
			res << "Circuit " << circuit_name << ":\n";
			prefix_line = "\t";
		}
		for (const auto &g : gates) {
			res << prefix_line << g.first;
			if (g.first == output_gate) {
				res << "*";
			}
			res << ": "  << g.second->toString();
			if (values.count(g.first))
				res << " | " << values.at(g.first);
			res << "\n";
		}
		return res.str();
	}
	
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
	GPAC<T> &operator()(std::string gate_name, T value) {
		addConstantGate(gate_name, value);
		return *this;
	}
	
	class CompareIntGate {
	public:
		CompareIntGate(const GPAC &circuit_) : circuit(circuit_) {}
		
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
		const GPAC &circuit;
	};
	
	/* Normalization: making sure that every second input of integration gates is t*/
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
		else if (gates.count(output_gate) == 0) {
			CircuitErrorMessage() << "Output gate is invalid!";
			exit(EXIT_FAILURE);
		}
		return *this;
	}
	GPAC<T> &simplify() {
		if (finalized)
			return *this;
		/* Sort inputs of binary gates so that they always are in the same order */
		for (const auto &g : gates) {
			if (isAddGate(g.first) || isProductGate(g.first)) {
				BinaryGate<T> *gate = asBinaryGate(g.first);
				if (gate->X() >= gate->Y()) {
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
		
		/* First delete constants */
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
				gates.erase(new_name.first);
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
		
		/* Recursively delete all equal product, addition and integration gates  */
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
						continue; // Gate cas already removed
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
					gates.erase(new_name.first);
					new_names.erase(new_name.first);
				}
			}
		}
		
		
		return *this;
	}
	
	/* Operators for constructing new circuits */
	
	// Function that rename gates in current circuit so that set of names of circuit and the other circuit are disjoint
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
	
	GPAC operator+(const GPAC<T> &circuit) const {
		if (output_gate == "" || circuit.Output() == "") {
			CircuitErrorMessage() << "Can't add two circuits with no defined output!";
			exit(EXIT_FAILURE);
		}
		
		GPAC result(*this);
		result.ensureUniqueNames(circuit);
		std::string old_output = result.Output();
		result.copyInto(circuit, false); // copy circuit in result
		result.setOutput(getNewGateName());
		result.addAddGate(result.Output(), old_output, circuit.Output(), false);
		return result;
	}
	GPAC &operator+=(const GPAC<T> &circuit) {
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
	GPAC operator*(const GPAC<T> &circuit) const {
		if (output_gate == "" || circuit.Output() == "") {
			CircuitErrorMessage() << "Can't multiply two circuits with no defined output!";
			exit(EXIT_FAILURE);
		}
		
		GPAC result(*this);
		result.ensureUniqueNames(circuit);
		std::string old_output = result.Output();
		result.copyInto(circuit, false); // copy circuit in result
		result.setOutput(getNewGateName());
		result.addProductGate(result.Output(), old_output, circuit.Output(), false);
		return result;
	}
	GPAC &operator*=(const GPAC<T> &circuit) {
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
	GPAC operator()(const GPAC<T> &circuit) const {
		if (output_gate == "" || circuit.Output() == "") {
			CircuitErrorMessage() << "Can't multiply two circuits with no defined output!";
			exit(EXIT_FAILURE);
		}
		// First check if one of the two circuits is the identity
		if (circuit.Output() == "t")
			return *this;
		if (output_gate == "t")
			return circuit;
		
		GPAC result(circuit);
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
		return result;
	}
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
		
	/* Simulation methods */
	
	void setInitValue(std::string gate_name, T value) {
		//if (!isIntGate(gate_name) || asIntGate(gate_name)->Y() != "t") {
		if (!isIntGate(gate_name)) {
			CircuitErrorMessage() << "Can only set initial value for integration gate!";
			return;
		}
		if (values[gate_name] != value)
			finalized = false;
		values[gate_name] = value;
	}
	const std::map<std::string, T> &getValues() const {
		return values;
	}
	
	GPAC<T> &finalize() {
		if (finalized)
			return *this;
		normalize();
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
		return *this;
	}
	void resetNonIntValues() {
		// Set all gates different from integration gate or constant gates to "not computed"
		for (const auto &g : gates) {
			if (!isIntGate(g.first) && !isConstantGate(g.first) && values.count(g.first) > 0)
				values.erase(g.first);
		}
	}
	GPAC<T> &initValues() {
		// Initial values for constants gates
		for (const auto &g : gates) {
			if (isConstantGate(g.first)) {
				values[g.first] = asConstantGate(g.first)->Constant();
			}
		}
		return *this;
	}
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
	
	// Step for simulating the circuit, vectors should be of the same size as int_gates
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
	
	class OutputObserver {
	public:
		OutputObserver(const GPAC<T> &c, std::vector<T> &v, std::vector<T> &t) : circuit(c), values(v), times(t) {}
		
		void operator()(const std::vector<T> &y, double t) {
			values.push_back(circuit.getValues().at(circuit.Output()));
			times.push_back(t);
		}
	private:
		const GPAC<T> &circuit;
		std::vector<T> &values;
		std::vector<T> &times;
	};
	
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
	
private:
	static unsigned new_gate_id;
	bool validation;
	bool block;
	bool finalized;
	std::map<std::string, T> values; // Numerical values of the outputs of the gates
	std::vector<std::string> int_gates; // Valid integration gates
	
	unsigned getNewGateId() const {return ++new_gate_id;}
	std::string getNewGateName() const {return "_" + boost::lexical_cast<std::string>(++new_gate_id) ;}
	AddGate<T>* asAddGate(std::string gate_name)  {
		return (dynamic_cast<AddGate<T>*>(gates.at(gate_name).get()));
	}
	ProductGate<T>* asProductGate(std::string gate_name)  {
		return (dynamic_cast<ProductGate<T>*>(gates.at(gate_name).get()));
	}
	IntGate<T>* asIntGate(std::string gate_name)  {
		return (dynamic_cast<IntGate<T>*>(gates.at(gate_name).get()));
	}
	ConstantGate<T>* asConstantGate(std::string gate_name)  {
		return (dynamic_cast<ConstantGate<T>*>(gates.at(gate_name).get()));
	}
	BinaryGate<T>* asBinaryGate(std::string gate_name)  {
		return (dynamic_cast<BinaryGate<T>*>(gates.at(gate_name).get()));
	}
	
	class PreferUserDefinedNames {
	public:
		bool operator()(std::string x, std::string y) {
			if (y.size() == 0)
				return false;
			if (x.size() == 0)
				return true;
			if (x[0] != '_' && y[0] == '_')
				return true;
			return (x<y);
		}
	};
};

/* Extern operators */

template<typename T>
GPAC<T> operator+(T constant, const GPAC<T> &circuit) {
	return circuit + constant;
}

template<typename T>
GPAC<T> operator*(T constant, const GPAC<T> &circuit) {
	return circuit * constant;
}

template<typename T>
std::ostream &operator<<(std::ostream &os, const GPAC<T> &circuit) {
	os << circuit.toString();
	return os;
}

/* Initializing static variable */

template<typename T>
unsigned GPAC<T>::new_gate_id = 0;
	
/* Some useful basic circuits */
	
// Note: use this only as a standalone circuit! Otherwise use operators between constants and circuits
template<typename T>
GPAC<T> Constant(T constant) {
	GPAC<T> res("Const", true, true);
	res("c", constant);
	res.setOutput("c");
	return res;
}

template<typename T>
GPAC<T> Identity() {
	GPAC<T> res("Id", true, true);
	res.setOutput("t");
	return res;
}

template<typename T> 
GPAC<T> Exp() {
	GPAC<T> res("Exp", true, true);
	res("exp", "I", "exp", "t");
	res.setOutput("exp");
	res.setInitValue("exp", 1);
	return res;
}
	
template<typename T> 
GPAC<T> Sin() {
	GPAC<T> res("Sin", true, true);
	res
		("sin_c-1", -1)
		("sin_P","x","sin","sin_c-1")
		("cos","I","sin_P","t")
		("sin","I","cos","t");
	res.setOutput("sin");
	res.setInitValue("cos", 1);
	res.setInitValue("sin", 0);
	return res;
}

template<typename T>
GPAC<T> Cos() {
	GPAC<T> res("Cos", true, true);
	res
		("cos_c-1", -1)
		("cos_P","x","sin","cos_c-1")
		("cos","I","cos_P","t")
		("sin","I","cos","t");
	res.setOutput("cos");
	res.setInitValue("cos", 1);
	res.setInitValue("sin", 0);
	return res;
}

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

// Circuit computing a polynomial a_0 + a_1 t + ... using Horner's method
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
