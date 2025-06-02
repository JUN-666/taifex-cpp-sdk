#include "order_book/order_book.h"
#include "messages/message_i083.h" // For SpecificMessageParsers::MessageI083
#include "messages/message_i010.h" // For SpecificMessageParsers::MessageI010 (though only decimal_locator used for OB constructor)
#include <iostream>
#include <cassert>
#include <vector>
#include <optional> // For checking derived quotes

// Using namespaces for brevity in test functions
using namespace OrderBookManagement;
using namespace SpecificMessageParsers;

// Forward declarations for test functions defined after main()
void test_apply_update_new();
void test_apply_update_change();
void test_apply_update_delete();
void test_apply_update_overlay_derived();
void test_apply_update_sequential();
void test_apply_update_sequence_number();

// Helper to create a MessageI010 with a specific decimal locator for tests
// Not strictly needed if OrderBook constructor takes decimal_locator directly
// and message structs contain already scaled prices.
/*
MessageI010 create_test_i010(uint8_t decimal_locator) {
    MessageI010 i010;
    i010.decimal_locator = decimal_locator;
    // Other fields can be default
    return i010;
}
*/

void test_constructor_and_reset() {
    std::cout << "Running test_constructor_and_reset..." << std::endl;
    std::string prod_id = "TESTPROD";
    uint8_t dec_loc = 2;
    OrderBook ob(prod_id, dec_loc);

    assert(ob.get_product_id() == prod_id);
    assert(ob.get_last_prod_msg_seq() == 0);
    assert(ob.get_top_bids(5).empty());
    assert(ob.get_top_asks(5).empty());
    assert(!ob.get_derived_bid().has_value()); // C++17
    assert(!ob.get_derived_ask().has_value()); // C++17

    // Populate some data then reset
    MessageI083 snapshot_msg;
    snapshot_msg.prod_id = prod_id;
    snapshot_msg.prod_msg_seq = 1;
    snapshot_msg.calculated_flag = '0';
    snapshot_msg.no_md_entries = 2;
    // Prices are assumed to be already scaled as they would be from the parser
    snapshot_msg.md_entries.push_back({'0', '0', 10000, 10, 1});
    snapshot_msg.md_entries.push_back({'1', '0', 10100, 5, 1});

    ob.apply_snapshot(snapshot_msg);
    assert(ob.get_last_prod_msg_seq() == 1);
    assert(!ob.get_top_bids(1).empty());

    ob.reset();
    assert(ob.get_product_id() == prod_id); // Product ID and locator should persist
    assert(ob.get_last_prod_msg_seq() == 0);
    assert(ob.get_top_bids(5).empty());
    assert(ob.get_top_asks(5).empty());
    assert(!ob.get_derived_bid().has_value());
    assert(!ob.get_derived_ask().has_value());

    std::cout << "test_constructor_and_reset PASSED." << std::endl;
}

void test_apply_snapshot_empty() {
    std::cout << "Running test_apply_snapshot_empty..." << std::endl;
    OrderBook ob("EMPTYPROD", 2);
    MessageI083 snapshot_msg;
    snapshot_msg.prod_id = "EMPTYPROD";
    snapshot_msg.prod_msg_seq = 10;
    snapshot_msg.calculated_flag = '0';
    snapshot_msg.no_md_entries = 0;

    ob.apply_snapshot(snapshot_msg);
    assert(ob.get_last_prod_msg_seq() == 10);
    assert(ob.get_top_bids(5).empty());
    assert(ob.get_top_asks(5).empty());
    assert(!ob.get_derived_bid().has_value());
    assert(!ob.get_derived_ask().has_value());
    std::cout << "test_apply_snapshot_empty PASSED." << std::endl;
}

void test_apply_snapshot_typical() {
    std::cout << "Running test_apply_snapshot_typical..." << std::endl;
    OrderBook ob("TYPICAL", 2);

    MessageI083 msg;
    msg.prod_id = "TYPICAL";
    msg.prod_msg_seq = 100;
    msg.calculated_flag = '0';
    msg.no_md_entries = 4;
    // Prices are scaled: 10025 means 100.25 if book's decimal_locator is 2 (for interpretation)
    msg.md_entries.push_back({'0', '0', 10025, 10, 1}); // Bid L1
    msg.md_entries.push_back({'0', '0', 10000, 5,  2}); // Bid L2
    msg.md_entries.push_back({'1', '0', 10050, 12, 1}); // Ask L1
    msg.md_entries.push_back({'1', '0', 10075, 8,  2}); // Ask L2

    ob.apply_snapshot(msg);

    assert(ob.get_last_prod_msg_seq() == 100);

    auto bids = ob.get_top_bids(2);
    assert(bids.size() == 2);
    assert(bids[0].price == 10025 && bids[0].quantity == 10); // Best bid
    assert(bids[1].price == 10000 && bids[1].quantity == 5);

    auto asks = ob.get_top_asks(2);
    assert(asks.size() == 2);
    assert(asks[0].price == 10050 && asks[0].quantity == 12); // Best ask
    assert(asks[1].price == 10075 && asks[1].quantity == 8);

    assert(!ob.get_derived_bid().has_value());
    assert(!ob.get_derived_ask().has_value());
    std::cout << "test_apply_snapshot_typical PASSED." << std::endl;
}

void test_apply_snapshot_with_derived() {
    std::cout << "Running test_apply_snapshot_with_derived..." << std::endl;
    OrderBook ob("DERIVEDPROD", 2);

    MessageI083 msg;
    msg.prod_id = "DERIVEDPROD";
    msg.prod_msg_seq = 200;
    msg.calculated_flag = '0'; // Derived quotes present if calculated_flag = '0'
    msg.no_md_entries = 4;
    msg.md_entries.push_back({'0', '0', 9900, 20, 1});  // Bid
    msg.md_entries.push_back({'1', '0', 9950, 15, 1});  // Ask
    msg.md_entries.push_back({'E', '0', 9890, 5, 1});   // Derived Bid
    msg.md_entries.push_back({'F', '0', 9960, 8, 1});   // Derived Ask

    ob.apply_snapshot(msg);
    assert(ob.get_last_prod_msg_seq() == 200);
    assert(ob.get_top_bids(1)[0].price == 9900);
    assert(ob.get_top_asks(1)[0].price == 9950);

    auto derived_bid = ob.get_derived_bid(); // C++17
    assert(derived_bid.has_value());        // C++17
    assert(derived_bid->price == 9890 && derived_bid->quantity == 5); // C++17

    auto derived_ask = ob.get_derived_ask(); // C++17
    assert(derived_ask.has_value());        // C++17
    assert(derived_ask->price == 9960 && derived_ask->quantity == 8); // C++17
    std::cout << "test_apply_snapshot_with_derived PASSED." << std::endl;
}

void test_apply_snapshot_calculated_flag() {
    std::cout << "Running test_apply_snapshot_calculated_flag..." << std::endl;
    OrderBook ob("CALCPROD", 0);

    MessageI083 msg;
    msg.prod_id = "CALCPROD";
    msg.prod_msg_seq = 300;
    msg.calculated_flag = '1'; // Call auction result, no derived quotes expected
    msg.no_md_entries = 2;
    // Market Buy (special price)
    msg.md_entries.push_back({'0', '0', 999999999LL, 10, 1});
    // Market Sell (special price, parser provides magnitude, sign indicates negative)
    msg.md_entries.push_back({'1', '-', 999999999LL, 5, 1});

    ob.apply_snapshot(msg);
    assert(ob.get_last_prod_msg_seq() == 300);

    auto bids = ob.get_top_bids(1);
    assert(bids.size() == 1);
    assert(bids[0].price == 999999999LL && bids[0].quantity == 10);

    auto asks = ob.get_top_asks(1);
    assert(asks.size() == 1);
    assert(asks[0].price == -999999999LL && asks[0].quantity == 5); // Price is negative due to SIGN='-'

    // No derived quotes if calculated_flag is '1'
    assert(!ob.get_derived_bid().has_value());
    assert(!ob.get_derived_ask().has_value());
    std::cout << "test_apply_snapshot_calculated_flag PASSED." << std::endl;
}


int main() {
    test_constructor_and_reset();
    test_apply_snapshot_empty();
    test_apply_snapshot_typical();
    test_apply_snapshot_with_derived();
    test_apply_snapshot_calculated_flag();

    std::cout << "Initial OrderBook tests completed." << std::endl;
    // Tests for apply_update (I081) will be added next.

    test_apply_update_new();
    test_apply_update_change();
    test_apply_update_delete();
    test_apply_update_overlay_derived();
    test_apply_update_sequential();
    test_apply_update_sequence_number();

    std::cout << "All OrderBook tests completed." << std::endl;
    return 0;
}

// --- New test functions for apply_update (I081) ---
#include "messages/message_i081.h" // For SpecificMessageParsers::MessageI081

void test_apply_update_new() {
    std::cout << "Running test_apply_update_new..." << std::endl;
    OrderBook ob("NEWPROD", 2);

    MessageI081 msg;
    msg.prod_id = "NEWPROD";
    msg.prod_msg_seq = 1;
    msg.no_md_entries = 2;
    // Prices are scaled: 10025 means 100.25 if locator is 2
    msg.md_entries.push_back({'0', '0', '0', 10025, 10, 1}); // New Bid: P=10025, Q=10
    msg.md_entries.push_back({'0', '1', '0', 10050, 5,  1}); // New Ask: P=10050, Q=5

    ob.apply_update(msg);

    assert(ob.get_last_prod_msg_seq() == 1);
    auto bids = ob.get_top_bids(1);
    assert(bids.size() == 1 && bids[0].price == 10025 && bids[0].quantity == 10);
    auto asks = ob.get_top_asks(1);
    assert(asks.size() == 1 && asks[0].price == 10050 && asks[0].quantity == 5);
    std::cout << "test_apply_update_new PASSED." << std::endl;
}

void test_apply_update_change() {
    std::cout << "Running test_apply_update_change..." << std::endl;
    OrderBook ob("CHANGEPROD", 2);

    // Initial state: Bid 10000 Q10, Ask 10100 Q20
    MessageI083 initial_snapshot;
    initial_snapshot.prod_id = "CHANGEPROD";
    initial_snapshot.prod_msg_seq = 1;
    initial_snapshot.calculated_flag = '0';
    initial_snapshot.no_md_entries = 2;
    initial_snapshot.md_entries.push_back({'0', '0', 10000, 10, 1});
    initial_snapshot.md_entries.push_back({'1', '0', 10100, 20, 1});
    ob.apply_snapshot(initial_snapshot);

    MessageI081 msg;
    msg.prod_id = "CHANGEPROD";
    msg.prod_msg_seq = 2;
    msg.no_md_entries = 3;
    // 1. Change Bid 10000 Q10 -> Q15
    msg.md_entries.push_back({'1', '0', '0', 10000, 15, 1});
    // 2. Change Ask 10100 Q20 -> Q0 (delete)
    msg.md_entries.push_back({'1', '1', '0', 10100, 0,  1});
    // 3. Change non-existent Bid 9900 Q5 (should add it)
    msg.md_entries.push_back({'1', '0', '0', 9900, 5, 2});

    ob.apply_update(msg);

    assert(ob.get_last_prod_msg_seq() == 2);
    auto bids = ob.get_top_bids(2);
    assert(bids.size() == 2); // 10000 Q15, 9900 Q5
    assert(bids[0].price == 10000 && bids[0].quantity == 15);
    assert(bids[1].price == 9900 && bids[1].quantity == 5);

    auto asks = ob.get_top_asks(1);
    assert(asks.empty()); // 10100 Q0 was deleted

    std::cout << "test_apply_update_change PASSED." << std::endl;
}

void test_apply_update_delete() {
    std::cout << "Running test_apply_update_delete..." << std::endl;
    OrderBook ob("DELETEPROD", 2);

    MessageI083 initial_snapshot;
    initial_snapshot.prod_id = "DELETEPROD";
    initial_snapshot.prod_msg_seq = 1;
    initial_snapshot.calculated_flag = '0';
    initial_snapshot.no_md_entries = 2;
    initial_snapshot.md_entries.push_back({'0', '0', 10000, 10, 1}); // Bid
    initial_snapshot.md_entries.push_back({'1', '0', 10100, 20, 1}); // Ask
    ob.apply_snapshot(initial_snapshot);

    MessageI081 msg;
    msg.prod_id = "DELETEPROD";
    msg.prod_msg_seq = 2;
    msg.no_md_entries = 2;
    // 1. Delete Bid 10000
    msg.md_entries.push_back({'2', '0', '0', 10000, 0, 1}); // Qty for delete is ignored by current logic, price matters
    // 2. Delete non-existent Ask 10200
    msg.md_entries.push_back({'2', '1', '0', 10200, 0, 2});

    ob.apply_update(msg);

    assert(ob.get_last_prod_msg_seq() == 2);
    assert(ob.get_top_bids(1).empty()); // Bid 10000 deleted
    assert(ob.get_top_asks(1).size() == 1 && ob.get_top_asks(1)[0].price == 10100); // Ask 10100 still there
    std::cout << "test_apply_update_delete PASSED." << std::endl;
}

void test_apply_update_overlay_derived() {
    std::cout << "Running test_apply_update_overlay_derived..." << std::endl;
    OrderBook ob("OVERLAYPROD", 2);

    // Initial derived quotes
    MessageI083 initial_snapshot;
    initial_snapshot.prod_id = "OVERLAYPROD";
    initial_snapshot.prod_msg_seq = 1;
    initial_snapshot.calculated_flag = '0';
    initial_snapshot.no_md_entries = 2;
    initial_snapshot.md_entries.push_back({'E', '0', 9900, 5, 1}); // Derived Bid
    initial_snapshot.md_entries.push_back({'F', '0', 9950, 8, 1}); // Derived Ask
    ob.apply_snapshot(initial_snapshot);
    assert(ob.get_derived_bid().has_value() && ob.get_derived_bid()->price == 9900);

    MessageI081 msg;
    msg.prod_id = "OVERLAYPROD";
    msg.prod_msg_seq = 2;
    msg.no_md_entries = 2;
    // 1. Overlay Derived Bid with new values
    msg.md_entries.push_back({'5', 'E', '0', 9910, 10, 1});
    // 2. Overlay Derived Ask with P0 Q0 (clear)
    msg.md_entries.push_back({'5', 'F', '0', 0, 0, 1});

    ob.apply_update(msg);

    assert(ob.get_last_prod_msg_seq() == 2);
    auto derived_bid = ob.get_derived_bid();
    assert(derived_bid.has_value());
    assert(derived_bid->price == 9910 && derived_bid->quantity == 10);
    assert(!ob.get_derived_ask().has_value()); // Should be cleared
    std::cout << "test_apply_update_overlay_derived PASSED." << std::endl;
}

void test_apply_update_sequential() {
    std::cout << "Running test_apply_update_sequential..." << std::endl;
    OrderBook ob("SEQPROD", 2);

    MessageI081 msg;
    msg.prod_id = "SEQPROD";
    msg.prod_msg_seq = 1;
    msg.no_md_entries = 3;
    // 1. New Bid P=100, Q=10
    msg.md_entries.push_back({'0', '0', '0', 10000, 10, 1});
    // 2. New Ask P=101, Q=5
    msg.md_entries.push_back({'0', '1', '0', 10100, 5, 1});
    // 3. Change Bid P=100, Q=10 -> Q=12
    msg.md_entries.push_back({'1', '0', '0', 10000, 12, 1});

    ob.apply_update(msg);
    assert(ob.get_last_prod_msg_seq() == 1);
    auto bids = ob.get_top_bids(1);
    assert(bids.size() == 1 && bids[0].price == 10000 && bids[0].quantity == 12);
    auto asks = ob.get_top_asks(1);
    assert(asks.size() == 1 && asks[0].price == 10100 && asks[0].quantity == 5);
    std::cout << "test_apply_update_sequential PASSED." << std::endl;
}

void test_apply_update_sequence_number() {
    std::cout << "Running test_apply_update_sequence_number..." << std::endl;
    OrderBook ob("SEQNUMPROD", 2);

    MessageI081 msg1, msg2, msg_old;
    msg1.prod_id = msg2.prod_id = msg_old.prod_id = "SEQNUMPROD";

    msg1.prod_msg_seq = 10;
    msg1.no_md_entries = 1;
    msg1.md_entries.push_back({'0', '0', '0', 10000, 10, 1}); // New Bid
    ob.apply_update(msg1);
    assert(ob.get_last_prod_msg_seq() == 10);

    msg_old.prod_msg_seq = 9; // Older sequence
    msg_old.no_md_entries = 1;
    msg_old.md_entries.push_back({'0', '1', '0', 10100, 5, 1}); // New Ask
    ob.apply_update(msg_old); // Should be ignored by current logic
    assert(ob.get_last_prod_msg_seq() == 10); // Sequence should not change
    assert(ob.get_top_asks(1).empty()); // Ask from old message should not be applied

    msg2.prod_msg_seq = 11;
    msg2.no_md_entries = 1;
    msg2.md_entries.push_back({'0', '1', '0', 10200, 8, 1}); // New Ask
    ob.apply_update(msg2);
    assert(ob.get_last_prod_msg_seq() == 11);
    assert(ob.get_top_asks(1).size() == 1 && ob.get_top_asks(1)[0].price == 10200);

    std::cout << "test_apply_update_sequence_number PASSED." << std::endl;
}
