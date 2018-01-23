#ifndef PARSER_HPP_
#define PARSER_HPP_

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/lex_lexertl.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>

#include "circuit.hpp"

namespace GPAClib {
using namespace boost::spirit;
using boost::phoenix::val;

//enum LexerIDs { T, CIRCUIT, IDENTIFIER, WHITESPACE, VALUE, OP_ADD, OP_PROD, OP_INT, SEMICOL };

template <typename T, typename Lexer>
struct custom_lexer : lex::lexer<Lexer>
{
    custom_lexer()
        : 
		  circuit ("Circuit")
		, d ("d")
		, lpar ('(')
		, rpar (')')
		, identifier    ("[a-zA-Z_][a-zA-Z0-9_]*")
        , white_space   ("[ \\t\\n]+")
        , value ("[1-9][0-9]*|[-+]?[0-9]*\\.?[0-9]+")
		, op_add ("\\+")
		, op_prod ("[x\\*]")
		, op_int ("int")
		, semicol (";")
		, vert ("\\|")
		, col (":")
    {
        this->self = circuit | d | lpar | rpar | value | op_add | op_prod | op_int | col | semicol | vert | identifier;
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
};
	template<typename T>
	GPAC<T> newGPAC(std::string s) { return GPAC<T>(s); }
	
template<typename T, typename Iterator, typename Lexer>
struct custom_grammar : qi::grammar<Iterator, qi::in_state_skipper<Lexer> >
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
    custom_grammar(const TokenDef& tok, CircuitMap &c) : custom_grammar::base_type(spec), circuits(c)
    {
		using boost::spirit::_val;
		namespace spi = boost::spirit;
		namespace phx = boost::phoenix;
		
		spec = +(circuit_gates);
		
		circuit_gates =
			(tok.circuit
			 >> tok.identifier [ phx::ref(current_circuit) = _1, phx::ref(circuits)[_1] = GPAC<T>(), phx::bind(&GPAC<T>::rename, phx::ref(circuits)[_1], _1)] >> tok.col
			 >> +(gate) >> tok.semicol [ phx::bind(&GPAC<T>::setOutput, phx::ref(circuits)[phx::ref(current_circuit)], phx::ref(current_gate)), std::cout << phx::ref(circuits)[phx::ref(current_circuit)] << std::endl ])
			 ;
		
		gate = 
			(tok.identifier [ phx::ref(current_gate) = _1] >> tok.col >> (add_gate | prod_gate | int_gate | constant_gate))
		;
		
		add_gate = 
			(tok.identifier >> tok.op_add >> tok.identifier)
			[ std::cout << spi::_1 << val(" + ") << spi::_3 << std::endl,
			  phx::bind(&GPAC<T>::addAddGate, phx::ref(circuits)[phx::ref(current_circuit)], phx::ref(current_gate), spi::_1, spi::_3, false)]
		;
		
		prod_gate =
			(tok.identifier >> tok.op_prod >> tok.identifier)
			[ std::cout << spi::_1 << val(" x ") << spi::_3 << std::endl,
			  phx::bind(&GPAC<T>::addProductGate, phx::ref(circuits)[phx::ref(current_circuit)], phx::ref(current_gate), spi::_1, spi::_3, false) ]
		;
		
		int_gate =
			(tok.op_int >> tok.identifier >> tok.d >> tok.lpar >> tok.identifier >> tok.rpar >> tok.vert >> tok.value)
			[ std::cout << val("int ") << spi::_2 << val(" d( ") << spi::_5 << val(" ) | ") << spi::_8 << std::endl,
			  phx::bind(&GPAC<T>::addIntGate, phx::ref(circuits)[phx::ref(current_circuit)], phx::ref(current_gate), spi::_2, spi::_5, false),
			  phx::bind(&GPAC<T>::setInitValue, phx::ref(circuits)[phx::ref(current_circuit)], phx::ref(current_gate), spi::_8)]
		;
		
		constant_gate =
			(tok.value)
			[ std::cout << spi::_1 << std::endl,
			  phx::bind(&GPAC<T>::addConstantGate, phx::ref(circuits)[phx::ref(current_circuit)], phx::ref(current_gate), spi::_1, false)]
		;
    }
		
	qi::rule<Iterator, qi::in_state_skipper<Lexer> > spec, circuit_expr; 
	qi::rule<Iterator, qi::in_state_skipper<Lexer>> circuit_gates, gate, add_gate, prod_gate, int_gate, constant_gate;
	std::string current_circuit;
	std::string current_gate;
	CircuitMap &circuits;
};

template<typename T>
GPAC<T> LoadFromFile(std::string filename)
{
	std::ifstream circuit_spec;
	circuit_spec.open(filename);
	
	//std::string str("Circuit Bli: test : int a d(b) bla : a x b toto : D + o foo : int toto d(bla);");
	
	// iterator type used to expose the underlying input stream
    typedef std::string::iterator base_iterator_type;

    typedef lex::lexertl::token<
        base_iterator_type> token_type;
	
    // Here we use the lexertl based lexer engine.
    typedef lex::lexertl::lexer<token_type> lexer_type;

    // This is the token definition type (derived from the given lexer type).
    typedef custom_lexer<T, lexer_type> tokens;

    // this is the iterator type exposed by the lexer 
    typedef typename tokens::iterator_type iterator_type;

    // this is the type of the grammar to parse
    typedef custom_grammar<T, iterator_type, typename tokens::lexer_def> parser;
	
	std::map<std::string, GPAC<T> > circuits;
	
    // now we use the types defined above to create the lexer and grammar
    // object instances needed to invoke the parsing process
    tokens lexer;                         // Our lexer
    parser calc(lexer, circuits);                  // Our parser

    // At this point we generate the iterator pair used to expose the
    // tokenized input stream.
	
	std::stringstream s;
	s << circuit_spec.rdbuf();
	std::string str = s.str();
	
    std::string::iterator it = str.begin();
    iterator_type iter = lexer.begin(it, str.end());
    iterator_type end = lexer.end();
      
	// Parsing is done based on the the token stream, not the character 
    // stream read from the input.
    // Note how we use the lexer defined above as the skip parser. It must
    // be explicitly wrapped inside a state directive, switching the lexer 
    // state for the duration of skipping whitespace.
    bool r = qi::phrase_parse(iter, end, calc, qi::in_state("WS")[lexer.self]);

    if (r && iter == end)
    {
        std::cout << "-------------------------\n";
        std::cout << "Parsing succeeded\n";
        std::cout << "-------------------------\n";
    }
    else
    {
        std::cout << "-------------------------\n";
        std::cout << "Parsing failed\n";
        std::cout << "-------------------------\n";
    }
	
	return calc.getCircuit();
}
}
	
#endif
