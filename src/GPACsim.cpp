/*!
 * \file GPACsim.cpp
 * \brief Simple program loading a circuit from a file and simulating it using GPAClib
 * \author Fabrice L.
 */

#include <string>
#include <memory>
#include <map>
#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/lu.hpp>
#include <boost/numeric/ublas/io.hpp>

#include "GPAC.hpp"
#include "GPACparser.hpp"

int main(int argc, char *argv[]) {
	std::string filename;
	bool simulate = true, simplification = true, to_dot = false, to_code = false;
	bool finalization = true;
	double b = 5.;
	double step = 0.001;
	std::string output, dot_file;
	
	/* Handle program options */
	namespace po = boost::program_options;
	try {
		po::options_description opt_descr("Options description");
		opt_descr.add_options()
			("help,h", "Display this help message")
			("circuit-file,i", po::value<std::string>(&filename)->required(), "Input file defining the circuit to simulate")
			("output,o", po::value<std::string>(&output), "Output (pdf) file of the simulation")
			("sup,b", po::value<double>(&b), "Sup of the interval on which the circuit is to be simulated")
			("step,s", po::value<double>(&step), "Step for the simulation")
			("to-dot,d", po::value<std::string>(&dot_file), "Generate a dot representation and export it in the specified file")
			("to-code", "Prints the C++ representation of the circuit")
			("no-simulation", "Validate the circuit without simulating it")
			("no-simplification", "Disable simplification of the circuit")
			("no-finalization", "Disable finalization of the circuit, also disable simulation")
		;
	    po::positional_options_description p;
        p.add("circuit-file", -1);
		
	    po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(opt_descr).positional(p).run(), vm);

		if (vm.count("help")) {
			std::cerr << opt_descr << "\n";
			return EXIT_SUCCESS;
		}
		if (vm.count("no-simulation"))
			simulate = false;
		if (vm.count("no-simplification"))
			simplification = false;
		if (vm.count("no-finalization"))
			finalization = false;
		if (vm.count("to-dot"))
			to_dot = true;
		if (vm.count("to-code"))
			to_code = true;
		
		po::notify(vm);
	}
	catch (std::exception &e) {
		std::cerr << "Error: " << e.what() << "\n";
		return EXIT_FAILURE;
	}
	
	if (!finalization && simulate) {
		WarningMessage() << "cannot simulate a circuit that is not finalized -> simulation disabled.";
		simulate = false;
	}
	
	GPAClib::GPAC<double> circuit = GPAClib::LoadFromFile<double>(filename);
	
	if (circuit.Output() == "") {
		circuit.CircuitErrorMessage() << "no output defined!";
		return EXIT_FAILURE;
	}
	
	if (finalization)
		circuit.finalize(simplification);
	
	if (to_dot) {
		if (dot_file != "") {
			circuit.toDot(dot_file);
		}
		else
			std::cout << circuit.toDot() << "\n";
	}
	else if (to_code)
		std::cout << circuit.toCode() << "\n";
	else
		std::cout << circuit << "\n";
	
	if (simulate)
		circuit.SimulateGnuplot(0., b, step, output);
	
	return 0;
}
