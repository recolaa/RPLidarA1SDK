/*
 * This file is part of a Lidar Receiver/Sender application.
 * 
 * It makes use of software under the following license:
 * Copyright (c) 2009 - 2014 RoboPeak Team (http://www.robopeak.com)
 * Copyright (c) 2014 - 2018 Shanghai Slamtec Co., Ltd.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <memory>
#include <atomic>

// Include our UDP sender header
#include "UdpSender.h"

#ifdef _WIN32
// Windows Sleep
#include <Windows.h>
#define delay(x) ::Sleep(x)
#else
// Unix-like
#include <unistd.h>
static inline void delay(size_t ms){
    usleep(ms * 1000);
}
#endif

#include "sl_lidar.h"
#include "sl_lidar_driver.h"

// For older code that doesn't define _countof
#ifndef _countof
#define _countof(_Array) (int)(sizeof(_Array) / sizeof(_Array[0]))
#endif

using namespace sl;

// ------------------------------------------------
// Forward declaration for usage printing
static void print_usage(const char* progName);

// ------------------------------------------------
// Check LiDAR health
bool checkSLAMTECLIDARHealth(ILidarDriver * drv)
{
    sl_result op_result;
    sl_lidar_response_device_health_t healthinfo;

    op_result = drv->getHealth(healthinfo);
    if (SL_IS_OK(op_result)) {
        printf("SLAMTEC Lidar health status : %d\n", healthinfo.status);
        if (healthinfo.status == SL_LIDAR_STATUS_ERROR) {
            fprintf(stderr, "Error, slamtec lidar internal error detected.\n");
            fprintf(stderr, "Please reboot the device to retry.\n");
            return false;
        }
        return true;
    } else {
        fprintf(stderr, "Error, cannot retrieve the lidar health code: %x\n", op_result);
        return false;
    }
}

static std::atomic_bool ctrl_c_pressed(false);
void ctrlc(int) { ctrl_c_pressed.store(true); }

// ------------------------------------------------
int main(int argc, const char * argv[])
{
    // Parse arguments
    const char * opt_is_channel = NULL; 
    const char * opt_channel = NULL;
    const char * opt_channel_param_first = NULL;
    sl_u32       opt_channel_param_second = 0;
    sl_u32       baudrateArray[2] = {115200, 256000};
    sl_result    op_result;
    int          opt_channel_type = CHANNEL_TYPE_SERIALPORT;
    bool         useArgcBaudrate = false;

    printf("Ultra simple LIDAR data grabber for SLAMTEC LIDAR.\n"
           "UDP JSON Sender Example\n"
           "SDK Version: %s\n", SL_LIDAR_SDK_VERSION);

    // Very basic argument parsing
    if (argc > 1) { 
        opt_is_channel = argv[1];
    } else {
        print_usage(argv[0]);
        return -1;
    }

    if (strcmp(opt_is_channel, "--channel") == 0) {
        opt_channel = argv[2];
        if(strcmp(opt_channel, "-s")==0 || strcmp(opt_channel, "--serial")==0) {
            opt_channel_param_first = argv[3];
            if (argc > 4) {
                opt_channel_param_second = strtoul(argv[4], NULL, 10);
            }
            useArgcBaudrate = true;
        }
        else if(strcmp(opt_channel, "-u")==0 || strcmp(opt_channel, "--udp")==0) {
            opt_channel_param_first = argv[3];
            if (argc > 4) {
                opt_channel_param_second = strtoul(argv[4], NULL, 10);
            }
            opt_channel_type = CHANNEL_TYPE_UDP;
        }
        else {
            print_usage(argv[0]);
            return -1;
        }
    } else {
        print_usage(argv[0]);
        return -1;
    }

    // Default COM port if none provided
    if (opt_channel_type == CHANNEL_TYPE_SERIALPORT) {
#ifdef _WIN32
        if (!opt_channel_param_first) opt_channel_param_first = "\\\\.\\com3";
#else
        if (!opt_channel_param_first) opt_channel_param_first = "/dev/ttyUSB0";
#endif
    }

    // Create driver instance
    ILidarDriver * drv = *createLidarDriver();
    if (!drv) {
        fprintf(stderr, "Insufficient memory, exit.\n");
        return -2;
    }

    // Attempt to connect
    sl_lidar_response_device_info_t devinfo;
    bool connectSuccess = false;
    IChannel* _channel = nullptr;

    // Serial or UDP channel connection
    if (opt_channel_type == CHANNEL_TYPE_SERIALPORT) {
        // If user specified a baud rate, try that only
        if (useArgcBaudrate) {
            _channel = (*createSerialPortChannel(opt_channel_param_first, opt_channel_param_second));
            if (SL_IS_OK(drv->connect(_channel))) {
                op_result = drv->getDeviceInfo(devinfo);
                if (SL_IS_OK(op_result)) {
                    connectSuccess = true;
                }
            }
        }
        // Otherwise try typical baud rates
        else {
            size_t baudRateArraySize = sizeof(baudrateArray) / sizeof(baudrateArray[0]);
            for (size_t i = 0; i < baudRateArraySize; ++i) {
                _channel = (*createSerialPortChannel(opt_channel_param_first, baudrateArray[i]));
                if (SL_IS_OK(drv->connect(_channel))) {
                    op_result = drv->getDeviceInfo(devinfo);
                    if (SL_IS_OK(op_result)) {
                        connectSuccess = true;
                        break;
                    } else {
                        // If failed, try next baudrate
                        // But must release any partial driver
                        // (In practice, you'd re-create or re-connect, etc.)
                    }
                }
            }
        }
    }
    else if (opt_channel_type == CHANNEL_TYPE_UDP) {
        _channel = *createUdpChannel(opt_channel_param_first, opt_channel_param_second);
        if (SL_IS_OK(drv->connect(_channel))) {
            op_result = drv->getDeviceInfo(devinfo);
            if (SL_IS_OK(op_result)) {
                connectSuccess = true;
            }
        }
    }

    // If connection failed, clean up and return early (no goto).
    if (!connectSuccess) {
        fprintf(stderr, "Error, cannot connect to the LiDAR on %s.\n", opt_channel_param_first);
        if (drv) {
            delete drv;
            drv = NULL;
        }
        return -3;
    }

    // Print device info
    printf("SLAMTEC LIDAR S/N: ");
    for (int pos = 0; pos < 16; ++pos) {
        printf("%02X", devinfo.serialnum[pos]);
    }
    printf("\n"
           "Firmware Ver: %d.%02d\n"
           "Hardware Rev: %d\n",
           devinfo.firmware_version >> 8,
           devinfo.firmware_version & 0xFF,
           (int)devinfo.hardware_version);

    // Check health
    if (!checkSLAMTECLIDARHealth(drv)) {
        // If health check fails, clean up and return
        if (drv) {
            delete drv;
            drv = NULL;
        }
        return -4;
    }

    // Set up Ctrl+C handling
    signal(SIGINT, ctrlc);

    // For A1 or similar, start motor if using serial
    if (opt_channel_type == CHANNEL_TYPE_SERIALPORT) {
        drv->setMotorSpeed();
    }

    //-------------------------------------------------------
    // 1) Create a UDP sender (change IP/port as needed!)
    //-------------------------------------------------------
    // e.g., if you have a receiver listening on 127.0.0.1:9002
    UdpSender udpSender("10.35.194.214", 7000);
    if (!udpSender.init()) {
        std::cerr << "Failed to initialize UDP sender.\n";
        // We can jump to the cleanup label here
        goto on_finished;
    }

    // 2) Start scanning
    drv->startScan(0, 1);

    // 3) Grab scan data in a loop, build JSON, send via UDP
    while (!ctrl_c_pressed.load()) {
        sl_lidar_response_measurement_node_hq_t nodes[8192];
        size_t count = _countof(nodes);

        op_result = drv->grabScanDataHq(nodes, count);
        if (SL_IS_OK(op_result)) {
            drv->ascendScanData(nodes, count);

            for (size_t pos = 0; pos < count; ++pos) {
                float distance = nodes[pos].dist_mm_q2 / 4.0f;
                float angle    = (nodes[pos].angle_z_q14 * 90.f) / 16384.f;
                int   quality  = (nodes[pos].quality >> SL_LIDAR_RESP_MEASUREMENT_QUALITY_SHIFT);

                // Construct the JSON string
                // Now includes "type":"LIDAR"
                std::ostringstream oss;
                oss << "{"
                    << "\"type\":\"LIDAR\","
                    << "\"distance\":" << distance << ","
                    << "\"angle\":" << angle << ","
                    << "\"quality\":" << quality
                    << "}";

                std::string jsonStr = oss.str();

                // Send JSON string via UDP
                udpSender.send(jsonStr);

            }
        }

    }

    // 4) Stop scanning, stop motor (if applicable)
    drv->stop();
    delay(200);
    if (opt_channel_type == CHANNEL_TYPE_SERIALPORT) {
        drv->setMotorSpeed(0);
    }

on_finished:
    if (drv) {
        delete drv;
        drv = NULL;
    }
    return 0;
}

// ------------------------------------------------
// Print usage
static void print_usage(const char* progName) {
    printf("Usage:\n"
           " For serial channel:\n"
           "   %s --channel --serial <com port> [baudrate]\n"
           " For UDP channel:\n"
           "   %s --channel --udp <ipaddr> [port NO.]\n",
           progName, progName);
}
