#include "main.h"

int main() {
    focus.mode(PullDown);
    if(focus){
        DEBUG = 0;
        focus_camera();
        return 0;
    }
    pc.baud(9600);
    serial.baud(9600);
    serial.attach(callback(&ser, &SerialHandler::rx));
    const reset_reason_t reason = ResetReason::get();
    if (DEBUG) {
        pc.printf("Last system reset reason: %s\r\n", reset_reason_to_string(reason).c_str());
    }
    watchdog.start(32000);
    ev_queue.call(blink);
    ev_queue.call(run);
    Watchdog::get_instance().kick();
    ev_queue.call_every(20000, check_time);
    ev_queue.dispatch_forever();
    return 0;
}
