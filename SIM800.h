#ifndef SIM_800_H
#define SIM_800_H
#include "mbed.h"
#include "SerialHandler.h"

extern RawSerial pc;
extern SerialHandler ser;

class SIM800{
public:
    SIM800(PinName pwr, PinName st);
    void powerON();
    void powerOFF();
    void enable();
    int start();
    int disableEcho();
    int checkSim();
    int setGPRSSettings();
    int startGPRS();
    int stopGPRS();
    int initHTTP();
    int termHTTP();
    int setFormat();
    int initFTP();
    int setFTPFileName(char* filename);
    int startFTP();
    int FTPsend(char* data, int ftp_size);
    int stopFTP();
    int sendSMS(char* text, char* phoneNumber);
    int deleteSMS(int indx);
    void simSleep();
    void simWake();
    string getSignalQuality();
    string readSMS(int indx);
    string readTimeStamp();

    bool simRegistered;


    DigitalOut power;
    DigitalOut strt;
};

#endif
