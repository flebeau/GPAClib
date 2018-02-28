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

GPAClib::GPAC<double> GracaImplementation();

int main(int argc, char *argv[]) {
	std::string filename;
	bool simulate = true, simplification = true, to_dot = false, to_code = false, to_latex = false;
	bool finalization = true;
	double b = 5.;
	double step = 0.001;
	std::string output, dot_file, latex_file;
	
	/* Handle program options */
	namespace po = boost::program_options;
	try {
		std::string step_descr = "Step for the simulation (default: " + std::to_string(step) + ")";
		std::string b_descr = "Sup of the interval on which the circuit is to be simulated (default: " + std::to_string(b) + ")";
		po::options_description opt_descr("Options description");
		opt_descr.add_options()
			("help,h", "Display this help message")
			("circuit-file,i", po::value<std::string>(&filename)->required(), "Input file defining the circuit to simulate")
			("output,o", po::value<std::string>(&output), "Output (pdf) file of the simulation")
			("sup,b", po::value<double>(&b), b_descr.c_str())
			("step,s", po::value<double>(&step), step_descr.c_str())
			("to-dot,d", po::value<std::string>(&dot_file)->implicit_value(""), "Generate a dot representation and export it in the specified file")
			("to-latex,d", po::value<std::string>(&latex_file)->implicit_value(""), "Generate a latex code representing the circuit and export it in the specified file")
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
		if (vm.count("to-latex"))
			to_latex = true;
		
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
	
	//GPAClib::GPAC<double> circuit = GracaImplementation();
	GPAClib::GPAC<double> circuit = GPAClib::LoadFromFile<double>(filename);
	
    //GPAClib::GPAC<double> circuit = GPAClib::L2<double>(20. * GPAClib::Exp<double>());
	//GPAClib::GPAC<double> circuit = 
	//	GPAClib::Switching<double>(1. + GPAClib::Identity<double>(), 1.+2.*GPAClib::Identity<double>(), (0.5 * (1. + GPAClib::Sin<double>()(2. * boost::math::constants::pi<double>() * GPAClib::Identity<double>()))), 0.5);
	//GPAClib::GPAC<double> circuit = GPAClib::Max<double>(GPAClib::Cos<double>(), GPAClib::Sin<double>(), 0.05);
	
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
	
	if (to_latex) {
		if (latex_file != "") {
			circuit.toLaTeX(latex_file);
		}
		else
			std::cout << circuit.toLaTeX() << "\n";
	}
	
	if (to_code)
		std::cout << circuit.toCode() << "\n";
	else
		std::cout << circuit << "\n";
	
	if (simulate)
		circuit.SimulateGnuplot(0., b, step, output);
	//circuit.SimulateDump(0., b, step);
	
	return 0;
}

GPAClib::GPAC<double> GracaImplementation() {
	using namespace GPAClib;
	GPAC<double> sin = Sin<double>()(2 * boost::math::constants::pi<double>() * Identity<double>());
	GPAC<double> s = 0.5 * (sin * sin + sin);
	GPAC<double> z1 = Constant<double>(42), z2 = Constant<double>(43);
	z1.rename("z1"); z1.renameGate("c", "z1");
	z2.rename("z2"); z2.renameGate("c", "z2");
	double lambda1 = 10;
	double lambda2 = 10;
	double gamma = 0.5;
	
	GPAC<double> temp = z1 - Exp2<double>()(Round<double>()(z2));
	GPAC<double> y = (1./gamma) * lambda1 * temp * temp * temp * temp + lambda1 / gamma + 10;
	GPAC<double> Z1 = (lambda1 * temp * temp * temp * L2<double>(y)(s)).Integrate(Identity<double>(), 0); 
	
	temp = z2 - Round<double>()(z1);
	y = (1./gamma) * lambda2 * temp * temp * temp * temp + lambda2 / gamma + 10;
	GPAC<double> Z2 = (lambda2 * temp * temp * temp * L2<double>(y)(s(-Identity<double>()))).Integrate(Identity<double>(), 0);
	
	GPAC<double> circuit = Z1;
	Z2.ensureUniqueNames(circuit);
	circuit.copyInto(Z2, false);
	circuit.simplify(true);
	
	circuit.renameInputs("z1", Z1.Output());
	circuit.renameInputs("z2", Z2.Output());
	
	return circuit;
}
