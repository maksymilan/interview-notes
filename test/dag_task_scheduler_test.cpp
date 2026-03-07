#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "questions/01_dag_task_scheduler/dag_task_scheduler.cpp"

namespace {

void Connect(Task& from, Task& to) {
    from.downstream.push_back(&to);
    to.upstream.push_back(&from);
}

void ExpectSchedulerFinished(Graph& graph, Cluster& cluster) {
    for (const auto& task : graph.tasks) {
        EXPECT_EQ(task.indegree, 0);
    }
    EXPECT_EQ(cluster.GetAvailableWorkers(), cluster.GetAllWorkers());
}

}  // namespace

TEST(ClusterTest, PollReturnsFinishedTask) {
    Cluster cluster(1);
    Task task;

    EXPECT_EQ(cluster.GetAllWorkers(), 1);
    EXPECT_EQ(cluster.GetAvailableWorkers(), 1);

    EXPECT_TRUE(cluster.Schedule(task));
    EXPECT_EQ(cluster.GetAvailableWorkers(), 0);

    Task* finished = nullptr;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (finished == nullptr && std::chrono::steady_clock::now() < deadline) {
        finished = cluster.Poll();
        if (finished == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    ASSERT_NE(finished, nullptr);
    EXPECT_EQ(finished, &task);
    EXPECT_EQ(cluster.GetAvailableWorkers(), 1);
}

TEST(SchedulerTest, RunHandlesEmptyGraph) {
    Graph graph;
    Cluster cluster(2);
    Scheduler scheduler(graph, cluster);

    scheduler.Run();

    EXPECT_EQ(cluster.GetAvailableWorkers(), cluster.GetAllWorkers());
}

TEST(SchedulerTest, RunCompletesDiamondDag) {
    Graph graph;
    graph.tasks.resize(4);

    Task& t0 = graph.tasks[0];
    Task& t1 = graph.tasks[1];
    Task& t2 = graph.tasks[2];
    Task& t3 = graph.tasks[3];

    Connect(t0, t1);
    Connect(t0, t2);
    Connect(t1, t3);
    Connect(t2, t3);

    Cluster cluster(2);
    Scheduler scheduler(graph, cluster);
    scheduler.Run();

    ExpectSchedulerFinished(graph, cluster);
}

TEST(SchedulerTest, RunCompletesMultiLayerDag) {
    Graph graph;
    graph.tasks.resize(8);

    Task& t0 = graph.tasks[0];
    Task& t1 = graph.tasks[1];
    Task& t2 = graph.tasks[2];
    Task& t3 = graph.tasks[3];
    Task& t4 = graph.tasks[4];
    Task& t5 = graph.tasks[5];
    Task& t6 = graph.tasks[6];
    Task& t7 = graph.tasks[7];

    Connect(t0, t3);
    Connect(t0, t4);
    Connect(t1, t4);
    Connect(t1, t5);
    Connect(t2, t5);
    Connect(t3, t6);
    Connect(t4, t6);
    Connect(t4, t7);
    Connect(t5, t7);

    Cluster cluster(3);
    Scheduler scheduler(graph, cluster);
    scheduler.Run();

    ExpectSchedulerFinished(graph, cluster);
}

TEST(SchedulerTest, RunCompletesDisconnectedComponents) {
    Graph graph;
    graph.tasks.resize(7);

    Task& a0 = graph.tasks[0];
    Task& a1 = graph.tasks[1];
    Task& a2 = graph.tasks[2];
    Task& b0 = graph.tasks[3];
    Task& b1 = graph.tasks[4];
    Task& isolated0 = graph.tasks[5];
    Task& isolated1 = graph.tasks[6];

    Connect(a0, a1);
    Connect(a1, a2);
    Connect(b0, b1);

    // Keep two independent single-node components.
    (void)isolated0;
    (void)isolated1;

    Cluster cluster(2);
    Scheduler scheduler(graph, cluster);
    scheduler.Run();

    ExpectSchedulerFinished(graph, cluster);
}

TEST(SchedulerTest, RunCanBeInvokedTwiceOnSameGraph) {
    Graph graph;
    graph.tasks.resize(5);

    Task& t0 = graph.tasks[0];
    Task& t1 = graph.tasks[1];
    Task& t2 = graph.tasks[2];
    Task& t3 = graph.tasks[3];
    Task& t4 = graph.tasks[4];

    Connect(t0, t2);
    Connect(t1, t2);
    Connect(t2, t3);
    Connect(t2, t4);

    Cluster cluster(2);
    Scheduler scheduler(graph, cluster);

    scheduler.Run();
    ExpectSchedulerFinished(graph, cluster);

    scheduler.Run();
    ExpectSchedulerFinished(graph, cluster);
}
