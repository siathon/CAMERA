#include "mbed.h"
#include "ov7670.h"
#include "SerialHandler.h"
#include "SIM800.h"
#include "ResetReason.h"
#include "events/EventQueue.h"
#include <string>

std::string reset_reason_to_string(const reset_reason_t reason){
    switch (reason) {
        case RESET_REASON_POWER_ON:
            return "Power On";
        case RESET_REASON_PIN_RESET:
            return "Hardware Pin";
        case RESET_REASON_SOFTWARE:
            return "Software Reset";
        case RESET_REASON_WATCHDOG:
            return "Watchdog";
        default:
            return "Other Reason";
    }
}

Watchdog &watchdog = Watchdog::get_instance();
static EventQueue ev_queue(100 * EVENTS_EVENT_SIZE);

#define DEVICE_CODE 4
#define IMAGE_SIZE 76800    // 320x240 pixels YUV (Only Y)
#define FTP_SIZE 1200
#define CAMERA_INIT_RETRIES 3
#define SIM800_INIT_RETRIES 5
#define CAMERA_CAPTURE_RETRIES 10
#define SMS_CHECK_INTERVAL 300 //sec

RawSerial pc(PC_10, PC_11);
RawSerial serial(PB_6, PB_7);
SerialHandler ser(0);
SIM800 sim800(PB_12, PB_3);

DigitalIn  focus(PA_10);
DigitalOut led(PB_4, 0);
DigitalOut cam_led_1(PB_15, 0);
DigitalOut cam_led_2(PC_13, 0);

OV7670 camera
(
    PB_11, PB_10,     // SDA(SB4) ,SCL(SB3) (I2C / SCCB)
    PC_4, NC, PC_2,   // VSYNC(u10), HREF, WEN(u8)(FIFO)
    PortA, 0xFF,     // PortIn data u99, u3, u4, u5, u2, u12, SB8, u31
    PC_0, PC_1, PC_3 // RRST(u7), OE(u11), RCLK(u1)
);

char secSt[3], minSt[3], hrSt[3], daySt[3], monSt[3], yearSt[5], Tm[40];
int next_h, next_m;
int DEBUG = 1;
int last_sms_sec = 0;

bool snap_flag = false;
bool time_set = false;
bool send_failed = false;
bool next_time_set = false;
bool next_event_set = false;
bool sim_reboot = false;

int stringToInt(string str){
    int indx = str.find('.');
    if (indx != -1) {
        str = str.substr(0, indx);
    }
    double t = 0;
    int l = str.length();
    for(int i = l-1; i >= 0; i--)
        t += (str[i] - '0') * pow(10.0, l - i - 1);
    return (int)t;
}

void intToChar(int t, int indx, char* ch){
    int temp;
    for (int i = 2; i >= 0; i--) {
        temp = t % 10;
        ch[indx + i] = temp + '0';
        t /= 10;
    }
    ch[indx + 3] = ',';
}

int init_camera(){
    int retries = CAMERA_INIT_RETRIES;
    while(1){
        if(DEBUG){
            pc.printf("Initializing Camera...");
        }
        if(camera.Init((char*)"BAW", IMAGE_SIZE) == 1){
            if(DEBUG){
                pc.printf("Done\n");
            }
            return 0;
        }
        else{
            if(DEBUG){
                pc.printf("Failed\n");
            }
            retries--;
            wait_us(1000000);
        }
        if (retries <= 0) {
            if(DEBUG){
                pc.printf("Failed to initialize camera!\n");
            }
            return -1;
        }
    }
}

void focus_camera(){
    pc.baud(460800);
    init_camera();
    led = 1;
    while (1) {
        while (pc.getc() != 's');
        cam_led_1 = 1;
        cam_led_2 = 1;
        camera.CaptureNext();
        while(camera.CaptureDone() == false);
        camera.ReadStart();
        cam_led_1 = 0;
        cam_led_2 = 0;
        wait_us(1000000);
        uint8_t buffer[300];
        for (size_t i = 0; i < IMAGE_SIZE / 300; i++) {
            for (size_t j = 0; j < 300; j++) {
                camera.ReadOnebyte();
                buffer[j] = camera.ReadOnebyte();
            }
            pc.write(buffer, 300, 0);
            wait_us(50000);
        }
        pc.putc('E');
        camera.ReadStop();
        camera.CaptureNext();
        while(camera.CaptureDone() == false);
        wait_us(1000000);
    }
}

void blink() {
    if (led) {
        led = 0;
        ev_queue.call_in(900, blink);
    }
    else{
        led = 1;
        ev_queue.call_in(100, blink);
    }
}

void enable_send(){
    snap_flag = true;
}

void turn_off_devices(){
    camera.sleepMode(true);
    // sim800.simSleep();
    snap_flag = false;
}

void run(){
    Watchdog::get_instance().kick();
    if (init_camera() != 0) {
        send_failed = true;
        turn_off_devices();
        return;
    }
    led = 1;
    if(!time_set){
        for (size_t i = 0; i < SIM800_INIT_RETRIES; i++) {
            Watchdog::get_instance().kick();
            // sim800.simWake();
            sim800.checkSim();
            if (!sim800.simRegistered) {
                sim800.start();
            }
            if(sim800.disableEcho() != 0){continue;}
            if(sim800.setGPRSSettings() != 0){continue;}
            if(sim800.startGPRS() != 0){sim800.stopGPRS(); continue;}
            Watchdog::get_instance().kick();
            if(sim800.initHTTP() != 0){sim800.termHTTP(); sim800.stopGPRS(); continue;}
            string s = sim800.readTimeStamp();
            pc.printf("%s\n", s.c_str());
            if(s.compare("-1") == 0){sim800.stopGPRS(); continue;}
            sim800.termHTTP();
            sim800.stopGPRS();
            set_time(stringToInt(s));
            time_set = true;
            break;
        }
    }
    Watchdog::get_instance().kick();
    if(!time_set){
        pc.printf("Faile to set time, Low charge\n");
        send_failed = true;
        turn_off_devices();
        return;
    }
    Watchdog::get_instance().kick();
    sim800.setFormat();
    sim800.initFTP();
    time_t seconds = time(NULL);
    strftime(secSt, 32, "%S", localtime(&seconds));
    strftime(minSt, 32, "%M", localtime(&seconds));
    strftime(hrSt , 32, "%H", localtime(&seconds));
    strftime(daySt, 32, "%d", localtime(&seconds));
    strftime(monSt, 32, "%m", localtime(&seconds));
    strftime(yearSt,32, "%Y", localtime(&seconds));
    sprintf(Tm, "C%04d_%s-%s-%s_%s:%s:%s", DEVICE_CODE, yearSt, monSt, daySt, hrSt, minSt, secSt);
    sim800.setFTPFileName(Tm);
    int retries = SIM800_INIT_RETRIES;
    while (true) {
        Watchdog::get_instance().kick();
        sim800.setGPRSSettings();
        sim800.startGPRS();
        if (sim800.startFTP() == 0) {
            break;
        }
        sim800.stopFTP();
        sim800.stopGPRS();
        retries--;
        if (retries == 0) {
            pc.printf("Failed to start FTP, Low charge!\n");
            turn_off_devices();
            send_failed = true;
            return;
        }
    }
    cam_led_1 = 1;
    cam_led_2 = 1;
    wait_us(100000);
    camera.CaptureNext();
    int capture_retries = CAMERA_CAPTURE_RETRIES;
    while(camera.CaptureDone() == false){
        Watchdog::get_instance().kick();
        wait_us(100000);
        capture_retries--;
        if (capture_retries <= 0) {
            pc.printf("Failed to capture image\n");
            turn_off_devices();
            send_failed = true;
            cam_led_1 = 0;
            cam_led_2 = 0;
            return;
        }
    }
    wait_us(100000);
    cam_led_1 = 0;
    cam_led_2 = 0;
    camera.ReadStart();
    Watchdog::get_instance().kick();
    for (size_t i = 0; i < IMAGE_SIZE / (FTP_SIZE / 4); i++) {
        Watchdog::get_instance().kick();
        char ch[FTP_SIZE + 1];
        for (size_t j = 0; j < FTP_SIZE / 4; j++) {
            int t = camera.ReadOnebyte();
            t = camera.ReadOnebyte();
            intToChar(t, 4 * j, ch);
        }
        pc.printf("Read %d bytes\n", FTP_SIZE / 4);
        ch[FTP_SIZE] = '\r';
        if (sim800.FTPsend(ch, FTP_SIZE) != 0) {
            pc.printf("Failed to send picture completely, Low charge\n");
            turn_off_devices();
            send_failed = true;
            return;
        }
    }
    sim800.stopFTP();
    sim800.stopGPRS();
    camera.ReadStop();
    camera.CaptureNext();
    while(camera.CaptureDone() == false);
    turn_off_devices();
    Watchdog::get_instance().kick();
}

void check_sms(){
    led = 1;
    pc.printf("Checing for SMS\n");
    for (size_t i = 0; i < 15; i++) {
        Watchdog::get_instance().kick();
        string output = sim800.readSMS(i+1);
        if (output.find("-1") != -1) {
            continue;
        }
        for (size_t j = 0; j < 3; j++) {
            int u = output.find("\"");
            output = output.substr(u+1);
        }
        int u = output.find("\"");
        string phoneNumber = output.substr(0, u);
        pc.printf("Senders number = %s\n", phoneNumber.c_str());
        for (size_t j = 0; j < 5; j++) {
            int u = output.find("\"");
            output = output.substr(u+1);
        }
        u = output.find("OK");
        output = output.substr(0, u);
        pc.printf("sms = %s\n", output.c_str());
        if (output.find("#qu") != -1) {
            pc.printf("Quality SMS Found\n");
            string csq = sim800.getSignalQuality();
            char text[10];
            sprintf(text, "CSQ = %s\n", csq.c_str());
            pc.printf("%s\n", text);
            sim800.sendSMS(text, (char*)phoneNumber.c_str());
        }
        else if(output.find("#GP") != -1){
            pc.printf("Capture SMS Found\n");
            ev_queue.call(run);
        }
        else{
            pc.printf("Irrelevant SMS.\n");
        }
        sim800.deleteSMS(i+1);
    }
    led = 0;
}

void check_time(){
    Watchdog::get_instance().kick();
    pc.printf("Check time\n");
    time_t seconds = time(NULL);
    strftime(hrSt , 32, "%H", localtime(&seconds));
    strftime(minSt, 32, "%M", localtime(&seconds));
    int now_h = stringToInt(hrSt);
    int now_m = stringToInt(minSt);
    pc.printf("time = %s:%s\n", hrSt,minSt);
    pc.printf("next image time = %02d:%02d\n", next_h, next_m);
    Watchdog::get_instance().kick();
    if (seconds > last_sms_sec + SMS_CHECK_INTERVAL) {
        sim800.checkSim();
        if (!sim800.simRegistered) {
            sim800.start();
        }
        sim800.setFormat();
        check_sms();
        last_sms_sec = seconds;
    }
    if (sim_reboot && stringToInt(hrSt) >= 23) {
        sim_reboot = false;
    }
    if (!sim_reboot && stringToInt(hrSt) == 22) {
        sim_reboot = true;
        sim800.powerOFF();
        for (size_t i = 0; i < 60; i++) {
            Watchdog::get_instance().kick();
            wait_us(1000000);
            led = !led;
        }
        sim800.powerON();
        sim800.start();
    }
    if (send_failed) {
        send_failed = false;
        next_h = now_h + 1;
        next_m = DEVICE_CODE % 60;
        if (next_h >= 24) {
            next_h -= 24;
        }
        pc.printf("Set next image time to %d:%d\n", next_h, next_m);
        next_time_set = true;
    }
    if (!next_time_set) {
        int dm = DEVICE_CODE % 60;
        if (now_h % 2 == 0) {
            if(dm < now_m){
                next_h = now_h + 2;
            }
            else{
                next_h = now_h;
            }

        }
        else{
            next_h = now_h + 1;
        }
        if (next_h >= 24) {
            next_h -= 24;
        }
        next_m = dm;
        pc.printf("Set next image time to %d:%d\n", next_h, next_m);
        next_time_set = true;
    }
    if (now_h == next_h && now_m >= next_m) {
        pc.printf("Next image time reached\n");
        enable_send();
    }
    if (snap_flag) {
        snap_flag = false;
        next_time_set = false;
        next_event_set = false;
        ev_queue.call(run);
    }
    Watchdog::get_instance().kick();
}
