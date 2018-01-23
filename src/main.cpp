#include <string>
#include <memory>
#include <map>
#include <iostream>

#include "gate.hpp"
#include "circuit.hpp"
#include "parser.hpp"

int main(int argc, char *argv[]) {
	// GPAClib::GPAC<double> circuit = GPAClib::Exp<double>()(GPAClib::Sin<double>()) + GPAClib::Identity<double>();
	// circuit.rename("exp(sin(t))+t");
	// std::cout << circuit.normalize().simplify() << "\n";
	// circuit.finalize().SimulateGnuplot(0., 20., 0.001, "simulation.pdf");
	
	GPAClib::GPAC<double> circuit = GPAClib::LoadFromFile<double>(argv[1]);
	std::cout << circuit.normalize().simplify() << "\n";
	circuit.finalize().SimulateGnuplot(0., 10., 0.001, "simulation.pdf");
	
	return 0;
}
