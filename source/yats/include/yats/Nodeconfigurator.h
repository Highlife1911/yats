#pragma once

#include <yats/Nodecontainer.h>
#include <yats/Output.h>

#include <iostream>
#include <memory>
#include <vector>


class AbstractNodeconfigurator
{
public:
	AbstractNodeconfigurator() = default;
	virtual ~AbstractNodeconfigurator() = default;

	virtual std::unique_ptr<AbstractNodecontainer> make() const = 0;

	virtual Input input() = 0;

	virtual Output output() = 0;

private:

};


template <typename Node>
class Nodeconfigurator : public AbstractNodeconfigurator
{
public:
	
	using Helper = decltype(MakeHelper(&Node::run));

	Nodeconfigurator() = default;

	std::unique_ptr<AbstractNodecontainer> make() const override
	{
		return std::make_unique<Nodecontainer<Node>>();
	}

	Input input() override
	{
		return i[0];
	}

	Output output() override
	{
		return o[0];
	}

private:

	std::vector<Input> i;
	std::vector<Output> o;
};