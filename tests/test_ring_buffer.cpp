#include "test_runner.hpp"
#include "core/ring_buffer.hpp"

using namespace lxace::core;

TEST_CASE(ring_buffer_push_and_snapshot) {
    RingBuffer<int> rb(3);
    ASSERT_EQ(rb.size(), 0);

    rb.push(1);
    rb.push(2);
    ASSERT_EQ(rb.size(), 2);

    auto snap = rb.snapshot();
    ASSERT_EQ(snap.size(), 2);
    ASSERT_EQ(snap[0], 1);
    ASSERT_EQ(snap[1], 2);
}

TEST_CASE(ring_buffer_overflow_eviction) {
    RingBuffer<int> rb(3);
    rb.push(1);
    rb.push(2);
    rb.push(3);
    rb.push(4); // evicts 1
    rb.push(5); // evicts 2

    ASSERT_EQ(rb.size(), 3);
    auto snap = rb.snapshot();
    ASSERT_EQ(snap.size(), 3);
    ASSERT_EQ(snap[0], 3);
    ASSERT_EQ(snap[1], 4);
    ASSERT_EQ(snap[2], 5);
}

TEST_CASE(ring_buffer_tail_cropping) {
    RingBuffer<std::string> rb(5);
    rb.push("line 1");
    rb.push("line 2");
    rb.push("line 3");
    rb.push("line 4");

    auto last_two = rb.tail(2);
    ASSERT_EQ(last_two.size(), 2);
    ASSERT_EQ(last_two[0], "line 3");
    ASSERT_EQ(last_two[1], "line 4");

    auto more_than_size = rb.tail(10);
    ASSERT_EQ(more_than_size.size(), 4);
    ASSERT_EQ(more_than_size[0], "line 1");
}

TEST_CASE(ring_buffer_versioning) {
    RingBuffer<int> rb(10);
    ASSERT_EQ(rb.version(), 0);

    rb.push(100);
    ASSERT_EQ(rb.version(), 1);

    rb.push(200);
    ASSERT_EQ(rb.version(), 2);
}
