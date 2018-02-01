/*!
 * \file GPACsim.cpp
 * \brief Simple program loading a circuit from a file and simulating it using GPAClib
 * \author Fabrice L.
 */

#include <string>
#include <memory>
#include <map>
#include <iostream>
#include <boost/program_options.hpp>

#include "GPAC.hpp"
#include "GPACparser.hpp"

int main(int argc, char *argv[]) {
	std::string filename;
	bool simulate = true, simplification = true;
	double b = 5.;
	double step = 0.001;
	std::string output;
	
	/* Handle program options */
	namespace po = boost::program_options;
	try {
		po::options_description opt_descr("Options description");
		opt_descr.add_options()
			("help,h", "Display this help message")
			("circuit-file,i", po::value<std::string>(&filename)->required(), "Input file defining the circuit to simulate")
			("no-simulation", "Validate the circuit without simulating it")
			("no-simplification", "Disable simplification of loaded circuit")
			("sup,b", po::value<double>(&b), "Sup of the interval on which the circuit is to be simulated")
			("step,s", po::value<double>(&step), "Step for the simulation")
			("output,o", po::value<std::string>(&output), "Output (pdf) file")
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
		
		po::notify(vm);
	}
	catch (std::exception &e) {
		std::cerr << "Error: " << e.what() << "\n";
		return EXIT_FAILURE;
	}
	
	GPAClib::GPAC<double> circuit = GPAClib::LoadFromFile<double>(filename);
	
	if (circuit.Output() == "") {
		std::cerr << "Quitting after error...\n" << std::endl;
		return EXIT_FAILURE;
	}
	std::cout << circuit.finalize(simplification) << "\n";
	
	if (simulate)
		circuit.SimulateGnuplot(0., b, step, output);
	
	return 0;
}
