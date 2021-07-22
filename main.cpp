#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <exception>

enum Event {
    INCOMING_CALL,
    CALL_DECLINED,
    CALL_ANSWERED,
    CALL_ENDED
};

enum StateDescriptor {
    IDLE,
    PHONE_RING,
    IN_CALL
};

/**
 * States: Idle, PhoneRinging, InCall
 * 
 * Transition Table:
 * Current State |    Event          | Operation      |  Next State
 * -----------------------------------------------------------------
 * Idle          |    INCOMING_CALL  | Start ringing  |  PhoneRinging
 * PhoneRinging  |    CALL_ANSWERED  | Start call     |  InCall
 * PhoneRinging  |    CALL_DECLINED  | Stop ringing   |  Idle
 * InCall        |    CALL_ENDED     | End call       |  Idle
 * 
 */


using OpHandler = void (*) ();

class State {
    public:

    virtual void do_op(Event e) = 0;
    virtual StateDescriptor get_descriptor() const = 0;
};


class Idle : public State {
    private:

    static void incoming_call_handler() {
        std::cout << "Phone started ringing\n";
    }

    std::map<Event, OpHandler> handler_map;


    public:

    Idle() {
        handler_map.insert({INCOMING_CALL, &incoming_call_handler});
    }

    virtual void do_op(Event e) {
        handler_map[e]();
    }
    
    virtual StateDescriptor get_descriptor() const {
        return IDLE;
    }
};

class PhoneRinging : public State {
    private:

    static void call_answered_handler() {
        std::cout << "Call answered. Starting conversation\n";
    }
    
    static void call_declined_handler() {
        std::cout << "Call declined\n";
    }

    std::map<Event, OpHandler> handler_map;


    public:

    PhoneRinging() {
        handler_map.insert({CALL_ANSWERED, &call_answered_handler});
        handler_map.insert({CALL_DECLINED, &call_declined_handler});
    }

    virtual void do_op(Event e) {
        handler_map[e]();
    }
    
    virtual StateDescriptor get_descriptor() const {
        return PHONE_RING;
    }
};

class InCall : public State {
    private:

    static void call_ended_handler() {
        std::cout << "Call ended\n";
    }

    std::map<Event, OpHandler> handler_map;


    public:

    InCall() {
        handler_map.insert({CALL_ENDED, &call_ended_handler});
    }

    virtual void do_op(Event e) {
        handler_map[e]();
    }
    
    virtual StateDescriptor get_descriptor() const {
        return IN_CALL;
    }
};


class TransitionManager {
    private:

    static const int NUM_STATES = 3;
    static const int NUM_EVENTS = 4;

    std::shared_ptr<State> mapping[NUM_STATES][NUM_EVENTS];

    std::shared_ptr<State> idle_state {new Idle()};
    std::shared_ptr<State> phone_ringing_state {new PhoneRinging()};
    std::shared_ptr<State> in_call_state {new InCall()};

    
    public:

    TransitionManager() {
        mapping[IDLE][INCOMING_CALL] = phone_ringing_state;
        mapping[IDLE][CALL_ANSWERED] = nullptr;
        mapping[IDLE][CALL_DECLINED] = nullptr;
        mapping[IDLE][CALL_ENDED] = nullptr;

        mapping[PHONE_RING][INCOMING_CALL] = nullptr;
        mapping[PHONE_RING][CALL_ANSWERED] = in_call_state;
        mapping[PHONE_RING][CALL_DECLINED] = idle_state; 
        mapping[PHONE_RING][CALL_ENDED] = nullptr; 

        mapping[IN_CALL][INCOMING_CALL] = nullptr;
        mapping[IN_CALL][CALL_ANSWERED] = nullptr;
        mapping[IN_CALL][CALL_DECLINED] = nullptr; 
        mapping[IN_CALL][CALL_ENDED] = idle_state; 
    }

    std::shared_ptr<State> get_next_state(StateDescriptor sd, Event event) const {
        return mapping[sd][event];
    }
};


static Event get_event() {

    static int call = 0;

    switch (call) {
        case 0:
            ++call;
            return INCOMING_CALL;
        
        case 1:
            ++call;
            return CALL_ANSWERED;
        
        case 2:
            call = 0;
            return CALL_ENDED;
    }


    throw std::runtime_error("Fatal error: case must not be reached");
    return CALL_ENDED;
}

class FSM {

    std::shared_ptr<State> current_state;

    public:

    FSM() :
    current_state(new Idle())
    {}

    void run(const TransitionManager& transition_manager,
             std::condition_variable& event_ready,
             std::mutex& m) {
    loop:
        std::unique_lock<std::mutex> lock(m); 
        
        event_ready.wait(lock);

        const Event e = get_event();

        current_state->do_op(e);
        current_state = transition_manager.get_next_state(current_state->get_descriptor(), e);
        goto loop;
    }
};

static void thread_main (std::condition_variable& event_ready) {
    
    loop:
    std::this_thread::sleep_for(std::chrono::seconds(3));
    event_ready.notify_one();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    event_ready.notify_one();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    event_ready.notify_one();
    goto loop;
}


int main() {

    FSM fsm;
    TransitionManager manager;

    std::condition_variable event_ready;
    std::mutex m;
    std::thread t(thread_main, std::ref(event_ready));


    fsm.run(manager, event_ready, m);

    return 0;
}
