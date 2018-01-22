#include <string>
#include <memory>
#include <map>
#include <iostream>

#include "gate.hpp"
#include "circuit.hpp"

int main() {
	GPAClib::GPAC<double> circuit = GPAClib::Exp<double>()(GPAClib::Sin<double>()) + GPAClib::Identity<double>();
	circuit.rename("exp(sin(t))+t");
	std::cout << circuit.normalize().simplify() << "\n";
	circuit.finalize().SimulateGnuplot(0., 10., 0.001, "simulation.pdf");
	
	return 0;
}
