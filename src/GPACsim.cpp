/*!
 * \file GPACsim.cpp
 * \brief Simple program loading a circuit from a file and simulating it using GPAClib
 * \author Fabrice L.
 */

#include <string>
#include <memory>
#include <map>
#include <iostream>

#include "GPAC.hpp"
#include "GPACparser.hpp"

int main(int argc, char *argv[]) {
	GPAClib::GPAC<double> circuit = GPAClib::LoadFromFile<double>(argv[1]);
	
	if (circuit.Output() == "") {
		std::cerr << "Quitting after error...\n" << std::endl;
		return EXIT_FAILURE;
	}
	std::cout << circuit.normalize().simplify() << "\n";
	circuit.finalize().SimulateGnuplot(0., 5, 0.0001, "simulation.pdf");
	
	return 0;
}
