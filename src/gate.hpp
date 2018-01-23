#ifndef GATE_HPP_
#define GATE_HPP_

#include <string>
#include <type_traits>
#include <array>
#include <boost/lexical_cast.hpp>

class Gate {
public:
    virtual ~Gate() {}
	
	virtual std::string toString() const = 0;
};

template<typename T>
class ConstantGate : public Gate {
public:
	ConstantGate(T c) : constant(c) {}
	
	virtual std::string toString() const {
		return boost::lexical_cast<std::string>(constant);
	}
	
	const T &Constant() const {return constant;}
private:
	T constant;
};

/*template<unsigned N>
class CircuitGate : public Gate {
public:
	using InputsType = std::array<std::string, N>;
	
	template<class... Types, typename std::enable_if<sizeof...(Types) == N, int>::type = 0>
	CircuitGate(Types... args) {
		inputs = {args...};
	}
	
	const InputsType &Inputs() const { return inputs; }
	
protected:
	InputsType inputs;
};*/

template<typename T>
class BinaryGate : public Gate {
public:
	BinaryGate(std::string x_name_, std::string y_name_) : x_name(x_name_), y_name(y_name_) {}
	
	virtual T operator()(T x, T y) const = 0;
	
	const std::string &X() const {return x_name;}
	const std::string &Y() const {return y_name;}
	
	std::string &X() {return x_name;}
	std::string &Y() {return y_name;}
	
protected:
	std::string x_name;
	std::string y_name;
};

template<typename T>
class AddGate : public BinaryGate<T> {
public:
	AddGate(std::string x_name_, std::string y_name_) : BinaryGate<T>(x_name_,y_name_) {}
	
	virtual std::string toString() const {
		return BinaryGate<T>::x_name + " + " + BinaryGate<T>::y_name;
	}
	
    virtual T operator()(T x, T y) const {
	    return x + y;
	}
};
template<typename T>
class ProductGate : public BinaryGate<T> {
public:
    ProductGate(std::string x_name, std::string y_name_) : BinaryGate<T>(x_name,y_name_) {}
	
	virtual std::string toString() const {
		return BinaryGate<T>::x_name + " x " + BinaryGate<T>::y_name;
	}
	
	virtual T operator()(T x, T y) const {
		return x * y;
	}
};
template<typename T>
class IntGate : public BinaryGate<T> {
public:
	IntGate(std::string x_name, std::string y_name_) : BinaryGate<T>(x_name,y_name_) {}
	
	virtual std::string toString() const {
		return "int " + BinaryGate<T>::x_name + " d( " + BinaryGate<T>::y_name + " )";
	}
	
	virtual T operator()(T x, T y) const {
		return x;
	}
};

#endif
