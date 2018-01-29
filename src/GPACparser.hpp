/*!
 * \file GPACparser.hpp
 * \brief File containing the parser for loading circuits from files
 * \author Fabrice L.
 */

#ifndef GPACPARSER_HPP_
#define GPACPARSER_HPP_

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/lex_lexertl.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/lexical_cast.hpp>
#include <string>

#include "utils.hpp"
#include "GPAC.hpp"

namespace GPAClib {
namespace qi = boost::spirit::qi;
namespace lex = boost::spirit::lex;

/// \brief Lexer for loading a circuit according to the specification format
template <typename T, typename Lexer>
struct GPACLexer : lex::lexer<Lexer>
{
    GPACLexer()
        : 
		  circuit ("Circuit")
		, d ("d")
		, lpar ('(')
		, rpar (')')
		, identifier    ("[a-zA-Z_][a-zA-Z0-9_]*")
        , white_space   ("[ \\t\\n]+")
        , value ("[1-9][0-9]*|[-]?[0-9]*\\.?[0-9]+")
		, op_add ("\\+")
		, op_prod ("\\*")
		, op_int ("int")
		, semicol (";")
		, vert ("\\|")
		, col (":")
		, eq ('=')
		, op_comp ('@')
	{
        this->self = circuit | d | lpar | rpar | value | op_add | op_prod | op_int | col | semicol | vert | eq | op_comp | identifier;
        this->self("WS") = white_space;
    }
	lex::token_def<>            circuit;
	lex::token_def<>            d;
	lex::token_def<>            lpar;
    lex::token_def<>            rpar;
	lex::token_def<std::string> identifier;
    lex::token_def<>            white_space;
    lex::token_def<T>      value;
	lex::token_def<>            op_add;
	lex::token_def<>            op_prod;
	lex::token_def<>            op_int;
	lex::token_def<>            semicol;
	lex::token_def<>            vert;
	lex::token_def<>            col;
	lex::token_def<>            eq;
	lex::token_def<>            op_comp;
};

template<typename T>
std::string ToString(T v) { return std::to_string(v);}

/// \brief Parser for loading a circuit according to the specification format
template<typename T, typename Iterator, typename Lexer>
struct GPACParser : qi::grammar<Iterator, qi::in_state_skipper<Lexer> >
{
	typedef std::map<std::string, GPAC<T> > CircuitMap;
	
	void changeCurrentCircuit(const std::string &s) {
		current_circuit = s;
	}
	
	std::string &getCurrentCircuit() {
		return current_circuit;
	}
	
	GPAC<T> &getCircuit() {
		return circuits.at(current_circuit);
	}
	
    template< typename TokenDef >
    GPACParser(const TokenDef& tok) : GPACParser::base_type(spec)
    {
		using boost::spirit::_val;
		namespace spi = boost::spirit;
		namespace phx = boost::phoenix;
		
		/* Initializing circuits with builtin circuits */
		circuits["Exp"] = GPAClib::Exp<T>();
		circuits["Sin"] = GPAClib::Sin<T>();
		circuits["Cos"] = GPAClib::Cos<T>();
		circuits["Arctan"] = GPAClib::Arctan<T>();
		circuits["Id"] = GPAClib::Identity<T>();
		circuits["t"] = GPAClib::Identity<T>();
		
		/* Definition of the grammar */
		
		spec = +((tok.circuit >> tok.identifier [phx::ref(current_circuit) = spi::_1, phx::ref(circuits)[spi::_1] = GPAC<T>()]
												 //,std::cout << val("Detecting circuit ") << _1 << std::endl] 
				  >> (circuit_gates | circuit_expr) >> tok.semicol) 
				 [phx::bind(&GPAC<T>::rename, phx::ref(circuits)[phx::ref(current_circuit)], phx::ref(current_circuit))]);
				  //,std::cout << phx::ref(circuits)[phx::ref(current_circuit)] << std::endl ]);
		
		/* First way of defining circuits: by a list of gates */
		
		circuit_gates =
			(tok.col
			 >> +(gate) [ phx::bind(&GPAC<T>::setOutput, phx::ref(circuits)[phx::ref(current_circuit)], phx::ref(current_gate))])
			 ;
		
		gate = 
			(tok.identifier [ phx::ref(current_gate) = spi::_1] >> tok.col >> (add_gate | prod_gate | int_gate | constant_gate))
		;
		
		add_gate = 
			(tok.identifier >> tok.op_add >> tok.identifier)
			[ //std::cout << spi::_1 << val(" + ") << spi::_3 << std::endl,
			  phx::bind(&GPAC<T>::addAddGate, phx::ref(circuits)[phx::ref(current_circuit)], phx::ref(current_gate), spi::_1, spi::_3, false)]
		;
		
		prod_gate =
			(tok.identifier >> tok.op_prod >> tok.identifier)
			[ //std::cout << spi::_1 << val(" x ") << spi::_3 << std::endl,
			  phx::bind(&GPAC<T>::addProductGate, phx::ref(circuits)[phx::ref(current_circuit)], phx::ref(current_gate), spi::_1, spi::_3, false) ]
		;
		
		int_gate =
			(tok.op_int >> tok.identifier >> tok.d >> tok.lpar >> tok.identifier >> tok.rpar >> tok.vert >> tok.value)
			[ //std::cout << val("int ") << spi::_2 << val(" d( ") << spi::_5 << val(" ) | ") << spi::_8 << std::endl,
			  phx::bind(&GPAC<T>::addIntGate, phx::ref(circuits)[phx::ref(current_circuit)], phx::ref(current_gate), spi::_2, spi::_5, false),
			  phx::bind(&GPAC<T>::setInitValue, phx::ref(circuits)[phx::ref(current_circuit)], phx::ref(current_gate), spi::_8)]
		;
		
		constant_gate =
			(tok.value)
			[ //std::cout << spi::_1 << std::endl,
			  phx::bind(&GPAC<T>::addConstantGate, phx::ref(circuits)[phx::ref(current_circuit)], phx::ref(current_gate), spi::_1, false)]
		;
		
		/* Second way of defining circuits: arithmetic operations on user-defined and predefined circuits */
		circuit_expr =
			(tok.eq >> expression [ phx::ref(circuits)[phx::ref(current_circuit)] = phx::ref(circuits)[spi::_1]])
		;
		
		expression = 
			(tok.lpar >> op >> tok.rpar) [qi::_val = spi::_2]
			| (tok.identifier) [qi::_val = spi::_1]
			| (tok.value) [qi::_val = phx::bind(&ToString<T>, spi::_1),
				           phx::ref(circuits)[qi::_val] = phx::bind(&GPAClib::Constant<T>, spi::_1),
						   phx::bind(&GPAC<T>::rename, phx::ref(circuits)[qi::_val], qi::_val)]
		;
		
		op = ((expression >> tok.op_add >> expression) 
			  [qi::_val = "_" + spi::_1 + "_p_" + spi::_3,
			   phx::ref(circuits)[qi::_val] = phx::ref(circuits)[spi::_1] + phx::ref(circuits)[spi::_3]]
			  |(expression >> tok.op_prod >> expression) 
			  [qi::_val = "_" + spi::_1 + "_t_" + spi::_3,
			   phx::ref(circuits)[qi::_val] = phx::ref(circuits)[spi::_1] * phx::ref(circuits)[spi::_3]]
			  |(expression >> tok.op_comp >> expression) 
			  [qi::_val = "_" + spi::_1 + "_c_" + spi::_3,
			   phx::ref(circuits)[qi::_val] = phx::bind(&GPAC<T>::operator(), phx::ref(circuits)[spi::_1], phx::ref(circuits)[spi::_3])]
			);
    }
	
	qi::rule<Iterator, qi::in_state_skipper<Lexer> > spec; 
	qi::rule<Iterator, qi::in_state_skipper<Lexer> > circuit_gates, gate, add_gate, prod_gate, int_gate, constant_gate;
	qi::rule<Iterator, qi::in_state_skipper<Lexer> > circuit_expr;
	qi::rule<Iterator, std::string(), qi::in_state_skipper<Lexer> > expression, op;
		
	std::string current_circuit;
	std::string current_gate;
	CircuitMap circuits;
};

/*! \brief Loading a circuit written in the specification format from a file
 * \param filename
 * \returns The circuit corresponding to the last circuit specified in the file.
 */
template<typename T>
GPAC<T> LoadFromFile(std::string filename)
{
	std::ifstream circuit_spec;
	circuit_spec.open(filename);
	
	typedef std::string::iterator base_iterator_type;
    typedef lex::lexertl::token<base_iterator_type> token_type;
	typedef lex::lexertl::lexer<token_type> lexer_type;
	typedef GPACLexer<T, lexer_type> Lexer;
	typedef typename Lexer::iterator_type iterator_type;
	typedef GPACParser<T, iterator_type, typename Lexer::lexer_def> Parser;
	
    Lexer lexer;
    Parser parser(lexer);

	/* Retrieve the content of file in a string */
	std::stringstream s;
	s << circuit_spec.rdbuf();
	std::string str = s.str(); 
	
	/* Initialize lexer */
    std::string::iterator it = str.begin();
    iterator_type iter = lexer.begin(it, str.end());
    iterator_type end = lexer.end();
      
	/* Parse the content of the file according to the GPAC parser */
	bool success = qi::phrase_parse(iter, end, parser, qi::in_state("WS")[lexer.self]);
	
    if (!success || iter != end)
    {
		ErrorMessage() << "Parsing of file " << filename << " failed!";
		return GPAC<T>();
    }
	
	std::cerr << "======================\n"
			  << "Parsing of file " << filename << " successful!\n"
			  << "Loaded circuit " << parser.getCircuit().Name() << std::endl
			  << "======================" << std::endl;
    
	return parser.getCircuit();
}
}
	
#endif
