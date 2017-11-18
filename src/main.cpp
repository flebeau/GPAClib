#include <string>
#include <memory>
#include <map>
#include <iostream>

#include "gate.hpp"
#include "circuit.hpp"

int main() {	
	//circlib::AnalogCircuit<double> circuit("C");
	// circuit
	// 	("I1","I","I1","t")
	// 	("P1","x","I1","I1")
	// 	("A1","+","P1","t")
	// 	("c1",1.)
	// 	("c2",2.)
	// 	("c3",2.)
	// 	("P2","x","A1","c3")
	// 	("A2","+","P2","c1");
	// circuit
	// 	("I1","I","I1","t")
	//  	("P1","x","I1","I1")
	//  	("A1","+","P1","t");
	
	// circuit.setOutput("A1");
	// circuit.validate();
	
	GPAClib::GPAC<double> circuit = GPAClib::Sin<double>();
	std::cout << circuit.finalize().Simulate(0., 1., 0.01) << "\n";
	
	return 0;
}
