#pragma once

#include <memory>
#include <map>

#include <yats/Identifier.h>
#include <yats/InputConnector.h>
#include <yats/TaskContainer.h>
#include <yats/OutputConnector.h>

namespace yats
{

class AbstractConnectionHelper
{
};

template <typename Task>
class ConnectionHelper : public AbstractConnectionHelper
{
public:

	using Helper = decltype(MakeHelper(&Task::run));

	ConnectionHelper(/*const std::map<uint64_t, InputConnector> &, const std::map<uint64_t, OutputConnector> &*/)
		: m_input(std::make_unique<typename Helper::InputQueueBase>())
		, m_callbacks(generateCallbacks(m_input, std::make_index_sequence<Helper::ParameterCount>()))
	{
	}

private:

	template <size_t... index>
	static typename Helper::InputCallbacks generateCallbacks(typename Helper::InputQueue &queue, std::integer_sequence<size_t, index...>)
	{
		// Prevent a warning about unused parameter when handling a run function with no parameters.
		(void) queue;
		return std::make_tuple(generateCallback<index>(queue)...);
	}

	template <size_t index>
	static typename std::tuple_element_t<index, typename Helper::InputCallbacks> generateCallback(typename Helper::InputQueue &queue)
	{
		using ParameterType = typename std::tuple_element_t<index, typename Helper::InputQueueBase>::value_type;
		return [&current = std::get<index>(*queue)](ParameterType input) mutable
		{
			current.push(input);
		};
	}

	typename Helper::InputQueue m_input;
	typename Helper::ReturnCallbacks m_output;
	typename Helper::InputCallbacks m_callbacks;
};

/**/
class AbstractTaskConfigurator
{
public:
	AbstractTaskConfigurator() = default;
	virtual ~AbstractTaskConfigurator() = default;

	virtual std::unique_ptr<AbstractTaskContainer> make() const = 0;
	virtual std::unique_ptr<AbstractConnectionHelper> make2() const = 0;

	virtual AbstractInputConnector& input(const std::string& name) = 0;
	virtual AbstractInputConnector& input(uint64_t id) = 0;

	virtual AbstractOutputConnector& output(const std::string& name) = 0;
	virtual AbstractOutputConnector& output(uint64_t id) = 0;
};


template <typename Task>
class TaskConfigurator : public AbstractTaskConfigurator
{
public:

	using Helper = decltype(MakeHelper(&Task::run));

	TaskConfigurator() = default;

	AbstractInputConnector& input(const std::string& name) override
	{
		return find<Helper::ParameterCount, AbstractInputConnector>(m_inputs, id(name.c_str()));
	}

	AbstractInputConnector& input(uint64_t id) override
	{
		return find<Helper::ParameterCount, AbstractInputConnector>(m_inputs, id);
	}

	AbstractOutputConnector& output(const std::string& name) override
	{
		return find<Helper::OutputParameterCount, AbstractOutputConnector>(m_outputs, id(name.c_str()));
	}

	AbstractOutputConnector& output(uint64_t id) override
	{
		return find<Helper::OutputParameterCount, AbstractOutputConnector>(m_outputs, id);
	}

	static void build(std::map<std::string, std::unique_ptr<AbstractTaskConfigurator>> &configurators)
	{
		std::vector<AbstractTaskConfigurator*> confs;
		for (auto &c : configurators)
		{
			confs.push_back(c.second.get());
		}

		std::vector<std::unique_ptr<AbstractConnectionHelper>> helpers;
		for (auto c : confs)
		{
			helpers.emplace_back(c->make2());
		}

		/*
		std::map<OutputConnector*, size_t> outputOwner;
		for (size_t i = 0; i < confs.size(); ++i)
		{
			auto outputs = confs[i]->outputs();
			for (auto output : outputs)
			{
				outputOwner.emplace(output, i);
			}
		}

		for (auto c : confs)
		{
			auto inputs = c->inputs();
			for (auto input : inputs)
			{
				auto id = outputOwner[input->source()];
			}
		}
		*/

		// Throw if a InputConnector has a nullptr OutputConnector
		// Add the function to the correct ReturnCallback list that is assosiated with the OutputConnector

		// Construct all TaskContainer
	}

	std::unique_ptr<AbstractTaskContainer> make() const override
	{
		return nullptr;
		//return std::make_unique<TaskContainer<Task>>();
	}

	std::unique_ptr<AbstractConnectionHelper> make2() const override
	{
		return std::make_unique<ConnectionHelper<Task>>(/*m_inputs, m_outputs*/);
	}

protected:

	template <size_t ParameterCount, typename Return, typename Parameter>
	Return& find(Parameter &tuple, uint64_t id)
	{
		auto connector = get<ParameterCount, Return>(tuple, id);
		if (connector)
		{
			return *connector;
		}
		throw std::runtime_error("Id not found.");
	}

	template <size_t ParameterCount, typename Return, size_t Index = 0, typename Parameter = int>
	std::enable_if_t<Index < ParameterCount, Return*> get(Parameter &tuple, uint64_t id)
	{
		auto elem = &std::get<Index>(tuple);
		if (id == std::tuple_element_t<Index, typename Helper::WrappedInput>::ID)
		{
			return elem;
		}
		return get<ParameterCount, Return, Index + 1>(tuple, id);
	}

	template <size_t ParameterCount, typename Return, size_t Index = 0, typename Parameter = int>
	std::enable_if_t<Index == ParameterCount, Return*> get(Parameter &, uint64_t)
	{
		return nullptr;
	}

	typename Helper::InputConfiguration m_inputs;
	typename Helper::OutputConfiguration m_outputs;
};

}  // namespace yats
