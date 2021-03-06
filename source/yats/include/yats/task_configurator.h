#pragma once

#include <memory>
#include <set>

#include <yats/constraint.h>
#include <yats/identifier.h>
#include <yats/lambda_task.h>
#include <yats/task_container.h>
#include <yats/util.h>

namespace yats
{

class abstract_task_configurator
{
public:
    abstract_task_configurator(const thread_group& thread_constraint)
        : m_thread_constraint(thread_constraint)
    {
    }

    virtual ~abstract_task_configurator() = default;

    abstract_task_configurator(const abstract_task_configurator& other) = delete;
    abstract_task_configurator(abstract_task_configurator&& other) = delete;

    abstract_task_configurator& operator=(const abstract_task_configurator& other) = delete;
    abstract_task_configurator& operator=(abstract_task_configurator&& other) = delete;

    virtual std::unique_ptr<abstract_task_container> construct_task_container(std::unique_ptr<abstract_connection_helper> helper, const external_function& external_callback) = 0;
    virtual std::unique_ptr<abstract_connection_helper> construct_connection_helper() const = 0;

    /**
     * Checks whether {@code connector} is marked as external.
     * @param connector The input object to check
     * @return Returns false if the given input is not marked as external or if the given input object does not belong to this task_configurator
     * @return Retruns true if the given input is marked as external
     */
    bool is_external(const abstract_input_connector* connector) const
    {
        return m_externals.find(connector) != m_externals.cend();
    }

    /**
     * Adds {@code group} to the already specified thread constraints.
     * @param group The constraints to add
     */
    void add_thread_constraint(const thread_group& group)
    {
        m_thread_constraint |= group;
    }

    /**
     * Returns the currently specified thread constraints.
     * @return thread_group containing the constraints.
     */
    const thread_group& thread_constraints() const
    {
        return m_thread_constraint;
    }

protected:
    bool mark_as_external(const abstract_input_connector* connector)
    {
        const auto inserted = m_externals.insert(connector);
        return inserted.second;
    }

    std::set<const abstract_input_connector*> m_externals;
    thread_group m_thread_constraint;
};

/**
 * Class which can be used to configure tasks which were added to the pipeline.
 * The task_configurator is typed with the task itself and the parameters it takes for construction.
 */
template <typename Task, typename... Parameters>
class task_configurator : public abstract_task_configurator
{
    static_assert(has_run_v<Task>, "Cannot create task_configurator because its task has no run function.");

    using helper = decltype(make_helper(&Task::run));
    using input_connectors = typename helper::input_connectors;
    using input_tuple = typename helper::input_tuple;
    using input_writers = typename helper::input_writers;
    using input_writers_ptr = typename helper::input_writers_ptr;
    using output_callbacks = typename helper::output_callbacks;
    using output_connectors = typename helper::output_connectors;
    using output_tuple = typename helper::output_tuple;

public:
    task_configurator(Parameters&&... parameters)
        : abstract_task_configurator(default_thread_constraints())
        , m_writers(std::make_unique<input_writers>())
        , m_options(std::make_unique<option_storage<Task>>(construct_options_map()))
        , m_construction_parameters(std::forward<Parameters>(parameters)...)
    {
    }

    /**
     * Get input with {@code Id}.
     */
    template <uint64_t Id>
    auto& input()
    {
        constexpr auto index = get_index_by_id_v<Id, input_tuple>;
        return std::get<index>(m_inputs);
    }

    /**
     * Gets output with {@code Id}.
     */
    template <uint64_t Id>
    auto& output()
    {
        constexpr auto index = get_index_by_id_v<Id, output_tuple>;
        return std::get<index>(m_outputs);
    }

    /**
     * Marks the input with {@code Id} as external and returns a function object used for writing into this input
     * External inputs get ignored when the connections of the pipeline are constructed.
     * They can now only be written using the returned function object.
     * The returned function objects takes two parameters: The value to write an whether it was the last element.
     * @return Returns a function objects used for writing into this input
     */
    template <uint64_t Id>
    const auto& mark_as_external()
    {
        constexpr auto index = get_index_by_id_v<Id, input_tuple>;
        if (abstract_task_configurator::mark_as_external(&input<Id>()))
        {
            std::get<index>(*m_writers).initialize_external_function();
        }
        return std::get<index>(*m_writers).external_function;
    }

    /**
     * Adds a callable as listener to an output with the given {@code Id}.
     * The given {@code callable} will be called every time the output is given a value.
     * This is syntactic sugar for adding a task without outputs and one input connected to the output with {@code Id}.
     * @param callable Object with a operator() method.
     */
    template <uint64_t Id, typename Callable>
    void add_listener(Callable callable)
    {
        using type = decltype(make_lambda_task(&Callable::operator()));
        constexpr auto index = get_index_by_id_v<Id, output_tuple>;
        std::get<index>(m_listeners).push_back(typename type::function_type(std::move(callable)));
    }

    /**
     * Constructs the container which holds the instantiated task.
     * This should not be called by the user. The task_configurator object is unusable afterwards!
     * @param helper Helper object containing all concrete connections of this task
     * @param external_callback Function object used to signalise the scheduler that a value has been written to an input marked as external
     * @return Returns the constructed task_container
     */
    std::unique_ptr<abstract_task_container> construct_task_container(std::unique_ptr<abstract_connection_helper> helper, const external_function& external_callback) override
    {
        return std::make_unique<task_container<Task, std::remove_reference_t<Parameters>...>>(static_cast<connection_helper<Task>*>(helper.get()), std::move(m_options), std::move(m_writers), external_callback, std::move(m_construction_parameters));
    }

    /**
     * Constructs a helper object containing all concrete connections of this task.
     * This should not be called by the user!
     * @return Returns the constructed helper object
     */
    std::unique_ptr<abstract_connection_helper> construct_connection_helper() const override
    {
        return std::make_unique<connection_helper<Task>>(m_inputs, m_outputs, m_listeners);
    }

    /**
     * Gets an object which can be used to update the options of the task.
     */
    typename options_ptr<Task>::pointer options()
    {
        return m_options.get();
    }

protected:
    template <typename LocalTask = Task>
    static std::enable_if_t<has_static_thread_constraints_v<LocalTask>, thread_group> default_thread_constraints()
    {
        return LocalTask::thread_constraints();
    }

    template <typename LocalTask = Task>
    static std::enable_if_t<!has_static_thread_constraints_v<LocalTask>, thread_group> default_thread_constraints()
    {
        static_assert(!has_thread_constraints_v<LocalTask>, "Task has to either define a static thread_constraints method which returns a thread_group or no thread_constraints method at all.");
        return thread_group();
    }

    template <typename T = Task>
    static std::enable_if_t<has_static_options_v<T>, options_map<T>> construct_options_map()
    {
        return Task::options();
    }

    template <typename T = Task>
    static std::enable_if_t<!has_static_options_v<T>, options_map<T>> construct_options_map()
    {
        static_assert(!has_options_v<T>, "Task has to either define a static options method which returns an options_map or no options method at all.");
        return options_map<Task>();
    }

    input_connectors m_inputs;
    output_connectors m_outputs;
    input_writers_ptr m_writers;
    output_callbacks m_listeners;
    options_ptr<Task> m_options;
    std::tuple<std::remove_reference_t<Parameters>...> m_construction_parameters;
};
}
