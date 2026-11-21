#include "test_runner.hpp"
#include "agent/orchestrator.hpp"
#include "agent/sub_agent.hpp"
#include "core/event_bus.hpp"
#include <atomic>
#include <chrono>
#include <thread>

using namespace lxace::agent;

TEST_CASE(sub_agent_reports_failed_without_registry) {
    // A task with no executable work must land in Failed, never Succeeded. The original
    // implementation always assigned Succeeded, so TaskState::Failed was unreachable.
    SubAgent agent("Sub-agent A", nullptr, nullptr, "");
    Task task;
    task.id = "task-1";
    task.prompt = "find files";

    std::atomic<bool> token{false};
    agent.execute_task(task, token);

    ASSERT_EQ(task.state, TaskState::Failed);
    ASSERT_FALSE(task.result_summary.empty());
}

TEST_CASE(sub_agent_respects_a_preset_cancel_token) {
    SubAgent agent("Sub-agent A", nullptr, nullptr, "");
    Task task;
    task.id = "task-2";
    task.prompt = "find files";

    std::atomic<bool> token{true};
    agent.execute_task(task, token);

    ASSERT_EQ(task.state, TaskState::Cancelled);
}

TEST_CASE(orchestrator_stop_all_cancels_pending_tasks) {
    auto bus = std::make_shared<lxace::core::EventBus>();
    Orchestrator orchestrator(bus, nullptr, "");

    const std::string id = orchestrator.submit_prompt("find files");
    ASSERT_FALSE(id.empty());

    orchestrator.stop_all();

    bool found = false;
    for (const auto& t : orchestrator.get_tasks()) {
        if (t.id == id) {
            found = true;
            ASSERT_EQ(t.state, TaskState::Cancelled);
        }
    }
    ASSERT_TRUE(found);
}

TEST_CASE(orchestrator_new_prompt_after_stop_is_not_cancelled) {
    // The old shared cancel flag was reset on every submit_prompt, which resurrected
    // tasks that stop_all had already cancelled. Fresh tasks must use a fresh token.
    auto bus = std::make_shared<lxace::core::EventBus>();
    Orchestrator orchestrator(bus, nullptr, "");

    orchestrator.submit_prompt("first task");
    orchestrator.stop_all();

    const std::string second = orchestrator.submit_prompt("second task");

    bool found = false;
    for (const auto& t : orchestrator.get_tasks()) {
        if (t.id == second) {
            found = true;
            ASSERT_TRUE(t.state != TaskState::Cancelled);
        }
    }
    ASSERT_TRUE(found);

    orchestrator.stop_all();
}

TEST_CASE(orchestrator_history_trim_keeps_worker_task_alive) {
    // Tasks are held by shared_ptr identity, so trimming history must not destroy a task
    // that a worker still owns. This exercises the trim boundary without crashing.
    auto bus = std::make_shared<lxace::core::EventBus>();
    Orchestrator orchestrator(bus, nullptr, "");

    for (int i = 0; i < 110; ++i) {
        orchestrator.submit_prompt("task " + std::to_string(i));
    }

    const auto tasks = orchestrator.get_tasks();
    ASSERT_TRUE(tasks.size() <= 100);
    ASSERT_TRUE(tasks.size() >= 1);

    orchestrator.stop_all();
}
