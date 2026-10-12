
## Project: Multi-Source Priority Event Dispatcher & Sensor Ingestion Engine

**Starting point:** Basic C++ knowledge, with little or no experience using GPIO or interrupts.  
**Platform:** Wokwi, an ESP32 development board, and the Arduino framework.  
**Approach:** Build one working feature at a time, explain its behavior, test its limits, then expand it.

## 1. Why I am doing this

I want to bridge the gap between writing ordinary C++ programs and writing firmware that responds to hardware events. I want to understand how inputs become events, how interrupts interact with normal execution, and how to process those events safely with limited memory and predictable behavior.

The goal is to learn how to reason about embedded software, not just produce a working demo. By the end, I should be able to explain why the design works, identify its limitations, and discuss its tradeoffs in an interview.

My first useful result will be deliberately small: **one button produces events, one fixed-capacity FIFO stores them, and the main loop processes them.** The final system will extend that same design to multiple sources, priorities, status fields, and callback handlers.

## 2. Initial expectations and boundaries

- I should be comfortable with variables, functions, loops, arrays, references, and basic classes in C++. I can learn bitwise operations, function pointers, and templates as they become necessary.
- I will begin with simulated button inputs and generated payloads. Real sensors are a later extension.
- I will use Serial output to observe behavior, but keep printing out of interrupt handlers and critical sections.
- The queues and event records will use fixed storage. This does not imply that the Arduino framework or the entire ESP32 runtime avoids dynamic allocation.
- Arduino on ESP32 runs on FreeRTOS. This project teaches embedded and low-level concepts, but is not a fully bare-metal implementation.
- Simulation is useful for learning behavior. It does not establish real-board timing, electrical reliability, or a proof of concurrency correctness.
- I will progress when I can explain and verify a milestone, rather than follow a fixed calendar.

## 3. How I will work through each stage

For every stage:

1. Write down what the new behavior should be.
2. Implement the smallest version that demonstrates it.
3. Run normal and boundary cases.
4. Record what happened and why.
5. Save a working checkpoint before adding complexity.

Keep a short learning log containing the current milestone, observations, bugs, fixes, and unanswered questions. When something fails, first identify whether the problem is wiring, input behavior, event capture, queue storage, or event processing.

## 4. Stage-by-stage plan

### Stage 0 — Set up the simulator and execution model

**Goal:** Understand where firmware starts and how it keeps running.

**Build:** An ESP32 Wokwi project that prints a startup message in `setup()` and a periodic message from `loop()`.

**Steps:**

1. Create an ESP32 Arduino project in Wokwi.
2. Start Serial communication.
3. Observe the difference between one-time initialization and repeated work.
4. Save the project and record how to run it again.

**Learn:** Firmware lifecycle, initialization, repeated execution, compilation, and the Serial monitor.

**Done when:** I can explain what `setup()` and `loop()` do and reliably run my project.

### Stage 1 — Basic GPIO: one button and one LED

**Goal:** Understand digital inputs and outputs before introducing interrupts.

**Build:** A button on GPIO 18 that controls an LED on a suitable output GPIO. Use an LED resistor in the diagram and an input pull-up for the button.

**Steps:**

1. Configure the LED as an output and blink it.
2. Connect the button between GPIO 18 and ground.
3. Configure GPIO 18 as `INPUT_PULLUP`.
4. Read the button using polling and update the LED.
5. Print only when the button state changes.

**Learn:** GPIO direction, HIGH/LOW, ground, pull-ups, floating inputs, active-low wiring, and polling.

**Done when:** Pressing and releasing the button produces predictable LED behavior, and I can explain why a pressed button reads LOW.

### Stage 2 — Detect one press as one event

**Goal:** Separate a persistent input state from an input transition.

**Build:** A polling-based press counter that increments once per press, rather than on every loop iteration while the button is held.

**Steps:**

1. Remember the previous button state.
2. Detect the HIGH-to-LOW transition.
3. Increment and display a press count.
4. Start with Wokwi button bouncing disabled using `"bounce": "0"`.
5. Enable bouncing briefly and observe how one physical press can produce multiple transitions. Save debounce implementation for Stage 10.

**Learn:** Edge detection, state tracking, contact bounce, and the difference between a signal and an application event.

**Done when:** Holding the button does not repeatedly increment the count, and I can explain the effect of bounce.

### Stage 3 — One button interrupt, with minimal shared state

**Goal:** Understand asynchronous execution before introducing a queue.

**Build:** A button interrupt that increments a pending-event counter. The main loop safely takes a snapshot of the counter and reports it.

**Steps:**

1. Attach a falling-edge interrupt to the button.
2. Keep the ISR limited to a small shared-state update.
3. Protect the ISR update and the main-loop snapshot/reset using the appropriate ESP32 critical-section APIs and one shared lock.
4. Print the captured count outside the critical section.
5. Compare the behavior with the polling version.

**Learn:** ISRs, interrupted execution, shared state, lost updates, and why `volatile` alone is not synchronization.

**Done when:** I can describe a possible race between reading and resetting a counter, explain how the protection prevents it, and demonstrate that printing happens in the main loop.

### Stage 4 — Build and test a fixed-capacity FIFO separately

**Goal:** Understand the data structure before connecting it to interrupts.

**Build:** A small ring buffer, such as capacity 8, with `push()` and `pop()` operations.

**Steps:**

1. Use fixed storage such as `std::array`.
2. Track a read index, write index, and count.
3. Return failure when pushing into a full queue or popping an empty queue.
4. Require a nonzero power-of-two capacity and use bit masking to wrap indices.
5. Exercise the queue with deterministic input sequences in ordinary code.

**Required checks:**

- Pop from an empty queue.
- Fill exactly to capacity, then reject one additional item.
- Confirm FIFO order.
- Drain the queue and confirm that it is empty.
- Alternate pushes and pops through several wraparounds.

**Learn:** Queue invariants, full/empty states, boundary checks, fixed memory usage, and index wrapping.

**Done when:** All checks pass and I can explain what each index and the count represent. These checks validate queue logic; they do not yet establish interrupt safety.

### Stage 5 — First core milestone: one button, one FIFO, one consumer

**Goal:** Connect asynchronous event capture to deferred processing.

**Build:** The button ISR creates a small event and enqueues it. The main loop pops events and prints their details or updates the LED.

**Steps:**

1. Define a fixed-size event containing a source ID, sequence number, and timestamp from an ISR-safe platform facility.
2. Add short, context-appropriate critical sections around queue mutations using one shared lock.
3. Enqueue from the ISR and dequeue from the main loop.
4. Handle the event after releasing the lock.
5. Add a drop-newest policy and a safely shared dropped-event counter.
6. Temporarily limit consumer processing to make queue overflow observable.

**Learn:** Producer/consumer architecture, top-half/bottom-half separation, event ownership, and overload handling.

**Done when:** Events retain FIFO order, full queues reject new events without corruption, drops are visible, and the ISR performs no printing, blocking, or heap allocation.

**Checkpoint:** This is the first complete project. Pause here and explain the whole path from button edge to background processing before expanding it.

### Stage 6 — Encapsulate synchronization with RAII

**Goal:** Make correct critical-section handling easier to maintain.

**Build:** Small non-copyable, non-movable guard types that pair entry and exit for task and ISR contexts.

**Steps:**

1. Wrap the supported ESP32 critical-section APIs in constructors and destructors.
2. Preserve the shared lock and make the calling context explicit.
3. Keep critical sections short and avoid logging or callback execution inside them.
4. Check that an early return still exits the critical section correctly.

**Learn:** RAII, scope-based cleanup, platform-specific synchronization, and protection of invariants.

**ESP32 rule:** Local interrupt masking alone does not protect shared memory from the other core. Use the supported spinlock-backed critical sections. Study raw interrupt save/restore as a separate architecture-specific concept.

**Done when:** I can explain why the guard is useful and why unconditional interrupt re-enabling is not a correct general restoration strategy.

### Stage 7 — Add a second producer

**Goal:** Extend the existing FIFO to multiple event sources.

**Build:** A second button on GPIO 19. Both ISRs enqueue events into the same FIFO; one main-loop consumer processes them.

**Steps:**

1. Give each source a distinct ID.
2. Make both producers use the same synchronization scheme.
3. Track accepted and dropped events per source.
4. Try interleaved inputs and scripted bursts where practical.
5. Check event counts after stopping producers and draining the queue.

**Learn:** Multi-producer/single-consumer design, serialized access, and the difference between arrival order and successful enqueue order.

**Done when:** Each attempted event is accounted for as accepted or dropped, accepted events are processed once, and I can explain the synchronization argument. Manual button presses are useful demonstrations, but not a proof of race freedom.

### Stage 8 — Add priority queues

**Goal:** Prefer urgent pending work while preserving FIFO order within each priority.

**Build:** Fixed-capacity queues for Critical, High, Medium, and Low events. Initially map the two buttons to Critical and Low.

**Steps:**

1. Add a priority field to the event.
2. Enqueue directly into the matching queue.
3. Protect the complete highest-priority scan and removal with one shared critical section.
4. Execute the selected callback after releasing the lock.
5. Test priority order using a known batch of queued events before starting consumption.
6. Generate sustained critical traffic and observe low-priority starvation.

**Learn:** Priority scheduling, FIFO within a priority, starvation, and bounded queue-selection work.

**Limits:** The scan is O(P) for P priority levels, with a fixed four-level bound in this project. It selects the highest pending priority at selection time. It cannot preempt a callback already running, and lock contention also affects latency.

**Done when:** Known batches are selected in the expected order and I can explain both starvation and the lack of callback preemption.

### Stage 9 — Add register emulation and callback dispatch

**Goal:** Make events resemble information received from a peripheral.

**Build:** Emulated status values containing data-ready, error, overflow, channel-ID, and error-code fields, plus a static channel-to-handler table.

**Steps:**

1. Document the imaginary register layout and define masks and shifts.
2. Separate pending-flag checks, format validation, and channel bounds checks.
3. Extract fields and construct a self-contained event.
4. Emulate acknowledgment without changing unrelated flags.
5. Check the callback-table index and null entries before dispatch.
6. Handle readings and errors in separate callbacks outside the lock.

**Learn:** Register decoding, bitfields, validation, write-one-to-clear semantics, and function-pointer dispatch.

**Hardware distinction:** Clearing bits in an ordinary variable with `status &= ~flags` is an emulation operation. Real W1C hardware is acknowledged by writing the handled-bit mask to the documented register. The Arduino GPIO layer manages its own GPIO interrupt acknowledgment.

**Done when:** Valid readings and error-only events reach the correct handler, invalid inputs are handled predictably, and unrelated status bits remain unchanged by the emulator.

### Stage 10 — Add debounce, measurements, and overload experiments

**Goal:** Understand behavior under imperfect inputs and excessive load.

**Build:** A non-blocking debounce policy, queue statistics, and event-age measurements.

**Steps:**

1. Enable simulated button bounce and define where duplicate edges will be filtered.
2. Use timing or deferred state checks instead of waiting inside an ISR.
3. Track queue high-water marks and drops per priority.
4. Measure the time from event capture to dispatch using wraparound-safe timestamp arithmetic.
5. Deliberately slow a callback and observe its effect on other events.
6. Use a bounded processing budget per loop iteration so other background work gets an opportunity to run.

**Learn:** Debouncing, event latency, backpressure, consumer budgets, timestamp rollover, and the relationship between arrival rate and processing rate.

**Done when:** I can demonstrate and explain bounce, overflow, starvation, and delayed processing using measurements. I will label simulator timing as simulator observations rather than real-hardware guarantees.

### Stage 11 — Consolidate and explain the finished design

**Goal:** Turn the implementation into evidence of understanding.

**Deliver:**

- A saved Wokwi project and source files.
- A README describing wiring, event flow, synchronization, and overflow policy.
- Results for FIFO boundaries, priority order, invalid channels, and overload behavior.
- A short explanation of limitations and possible next steps.

**Done when:** I can walk through the design without relying on memorized code and explain what happens if an interrupt arrives during a queue operation or callback.

## 5. Expected final behavior

The finished system will capture events from two simulated sources, validate and decode their status, and store accepted events in fixed-capacity priority queues. A single background consumer will select pending events by priority and dispatch them through a static callback table.

It will preserve FIFO order within each priority, expose dropped events, handle invalid channel IDs safely, and keep slower processing outside ISRs and critical sections. Its documentation will state the synchronization assumptions and scheduling limits explicitly.

## 6. Final learning outcomes

After completing the roadmap, I should be able to:

1. Configure digital inputs and outputs and explain pull-ups and active-low signals.
2. Distinguish input state, edges, interrupts, and application events.
3. Explain a shared-state race and why `volatile` does not prevent it.
4. Build and reason about a fixed-capacity ring buffer.
5. Use context-appropriate ESP32 synchronization and RAII cleanup.
6. Explain a lock-protected MPSC queue and its assumptions.
7. Separate minimal interrupt capture from deferred processing.
8. Decode bitfields and distinguish ordinary bit clearing from W1C acknowledgment.
9. Explain event priority, starvation, callback duration, and overflow policy.
10. Use counters and timestamps to investigate missing or delayed events.
11. Discuss the tradeoffs using concrete observations from my own project.

## 7. Interview questions I should be able to answer

- Why should Serial printing stay out of this ISR?
- What could go wrong if the consumer resets shared state without synchronization?
- Why does disabling interrupts on one ESP32 core not protect against the other core?
- What happens when the ring buffer is full?
- Why does bit-mask wrapping require a power-of-two capacity?
- What makes this queue MPSC, and is it lock-free?
- When can a low-priority event starve?
- Can a critical event interrupt a running callback in this design?
- Why is a W1C register different from an ordinary variable?
- How would I choose queue capacity for a known burst size and consumer rate?

## 8. What to explore after this project

After finishing, move to a physical board and a real sensor over I²C or SPI. Then explore hardware timers, ADC sampling, FreeRTOS queues and task notifications, watchdog behavior, and hardware debugging. Comparing the custom queue with a platform-provided queue will help explain when each approach is appropriate.

For a deeper bare-metal understanding, later study startup code, memory-mapped peripherals, vector tables, and architecture-specific interrupt save/restore on a simpler single-core microcontroller.

## References

- [Wokwi ESP32 simulation support](https://docs.wokwi.com/guides/esp32)
- [Wokwi pushbutton wiring and bounce behavior](https://docs.wokwi.com/parts/wokwi-pushbutton)
- [Arduino ESP32 GPIO API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/gpio.html)
- [ESP-IDF FreeRTOS critical sections](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html#critical-sections)
- [ESP32 GPIO register definitions](https://github.com/espressif/esp-idf/blob/master/components/soc/esp32/register/soc/gpio_reg.h)
