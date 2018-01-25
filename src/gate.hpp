#ifndef GATE_HPP_
#define GATE_HPP_

#include <string>
#include <type_traits>
#include <array>
#include <boost/lexical_cast.hpp>

namespace GPAClib {

/*! \brief Abstract class for defining gates
 */
class Gate {
public:
    virtual ~Gate() {}
	
	/// Ensures that a gate implement a method to print it.
	virtual std::string toString() const = 0;
};

/*! \brief Gate representing a constant
 * \tparam T Type of the value (e.g. double)
 */
template<typename T>
class ConstantGate : public Gate {
public:
	/// Constructing a new constant gate with the given value
	ConstantGate(T c) : constant(c) {}
	
	/// Exporting to string by lexical cast
	virtual std::string toString() const {
		return boost::lexical_cast<std::string>(constant);
	}
	
	/// Retrieving the value of the constant
	const T &Constant() const {return constant;}
private:
	T constant; /*!< Value of the constant */
};

/*! \brief Gate representing a binary operator
 * \tparam T Type of the value (e.g. double)
 */
template<typename T>
class BinaryGate : public Gate {
public:
	/// Creating a binary gate by specifying the name of the two inputs
	BinaryGate(std::string x_name_, std::string y_name_) : x_name(x_name_), y_name(y_name_) {}
	
	/*! \brief Binary gate must implement a binary operator
	 * \param x First operand
	 * \param y Second operand
	 * \return A value of type T
	 *
	 * Ensures that all binary gates implement a binary operator in the form of an overloaded
	 * parenthesis operator.
	 */
	virtual T operator()(T x, T y) const = 0;
	
	const std::string &X() const {return x_name;}
	const std::string &Y() const {return y_name;}
	
	/// Retrieve a reference to the first input
	std::string &X() {return x_name;}
	/// Retrieve a reference to the second input
	std::string &Y() {return y_name;}
	
protected:
	std::string x_name; /*!< Name of the first input */
	std::string y_name; /*!< Name of the second input */
};

/*! \brief Addition gate
 * \tparam T Type of the value (e.g. double)
 * 
 * Implements a binary gate representing the addition operation.
 */
template<typename T>
class AddGate : public BinaryGate<T> {
public:
	AddGate(std::string x_name_, std::string y_name_) : BinaryGate<T>(x_name_,y_name_) {}
	
	/// Printing the input names with a + sign in the middle
	virtual std::string toString() const {
		return BinaryGate<T>::x_name + " + " + BinaryGate<T>::y_name;
	}
	
	/// Overloading parenthesis operator to compute the sum of the inputs
    virtual T operator()(T x, T y) const {
	    return x + y;
	}
};
	
/*! \brief Product gate
 * \tparam T Type of the value (e.g. double)
 * 
 * Implements a binary gate representing the product operation.
 */	
template<typename T>
class ProductGate : public BinaryGate<T> {
public:
    ProductGate(std::string x_name, std::string y_name_) : BinaryGate<T>(x_name,y_name_) {}
	
	/// Printing the input names with a * sign in the middle
	virtual std::string toString() const {
		return BinaryGate<T>::x_name + " * " + BinaryGate<T>::y_name;
	}
	
    /// Overloading parenthesis operator to compute the product of the inputs
	virtual T operator()(T x, T y) const {
		return x * y;
	}
};

/*! \brief Integration gate
 * \tparam T Type of the value (e.g. double)
 * 
 * Implements a binary gate representing the integration operation. The first input represents the integrand, the second input is the one with respect to which it is integrated.
 */
template<typename T>
class IntGate : public BinaryGate<T> {
public:
	IntGate(std::string x_name, std::string y_name_) : BinaryGate<T>(x_name,y_name_) {}
	
	/// Printing of the form: `int <integrand> d( <second_input> )`
	virtual std::string toString() const {
		return "int " + BinaryGate<T>::x_name + " d( " + BinaryGate<T>::y_name + " )";
	}
	
	/// Meaningless parenthesis operator, but required as any class inheriting from BinaryGate
	virtual T operator()(T x, T y) const {
		return x;
	}
};

}
#endif
