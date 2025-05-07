/*
 * @file hrp.h
 * @brief Heart rate profile header file
 * 
 * It includes all the necessary header file, declarations and structures.
 *
 */

#ifndef HRP_H
#define HRP_H

#define _GNU_SOURCE
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/signalfd.h>

#include <glib.h>
#include <dbus/dbus.h>

#include "gdbus/gdbus.h"

#define GATT_MGR_IFACE          "org.bluez.GattManager1" 
#define GATT_SERVICE_IFACE      "org.bluez.GattService1"                        
#define GATT_CHR_IFACE          "org.bluez.GattCharacteristic1"                 
#define GATT_DESCRIPTOR_IFACE   "org.bluez.GattDescriptor1"                     
                                                                                 
/* Heart Rate Service UUID */                                                   
#define HRP_UUID            "0000180d-0000-1000-8000-00805f9b34fb"              
                                                                                 
/*Characteristic UUIDs*/                                                        
#define HR_MSRMT_CHR_UUID       "00002a37-0000-1000-8000-00805f9b34fb"          
#define BODY_SENSOR_LOC_CHR_UUID        "00002a38-0000-1000-8000-00805f9b34fb"  
#define HR_CTRL_PT_CHR_UUID     "00002a39-0000-1000-8000-00805f9b34fb"          
                                                                              
/* Descriptor UUID */                                                           
#define CLIENT_CHR_CONFIG_DESCRIPTOR_UUID   "82602902-1a54-426b-9e36-e84c238bc669"
                                                                             
/*                                                                              
 * Heart Rate measurement support notification only. Supported                  
 * properties are defined at doc/gatt-api.txt. See "Flags"                      
 * property of the GattCharacteristic1.                                         
 */                                                                             
static const char *hrs_hr_msrmt_props[] = { "notify", NULL };                   
static const char *hrs_body_sensor_loc_props[] = { "read", NULL };              
static const char *hrs_hr_ctrl_pt_props[] = { "write", NULL };                 
static const char *ccc_desc_props[] = { "read", "write", NULL };   

static GSList *services;

/**
 * @struct characteristic 
 * Represents the characteristic of HRP
 */
struct characteristic {
	char *service;
	char *uuid;
	char *path;
	uint8_t *value;
	int vlen;
	const char **props;
};

/**
 * @struct descriptor
 * Represents the desctiptor of HRP
 */
struct descriptor {
	struct characteristic *chr;
	char *uuid;
	char *path;
	uint8_t *value;
	int vlen;
	const char **props;
};

/**
 * @brief create the service for HRP
 *
 * This function will create's the service by registering the service and their
 * characteristic
 *
 * @param connection A pointer to DBusConnection representing the DBus connection
 */
void create_services_one(DBusConnection *connection);

/**
 * @brief registers HRP application
 * @param proxy A pointer to GDBusproxy representing HRP application
 */
void register_app(GDBusProxy *proxy);

/**
 * @brief callback function for handling data
 * 
 * This function prints the data and the size of the data which is write to the
 * characteristic and the descriptor
 *
 * @param data a pointer to const char representing the data to be processed
 * @param size size of the data
 */
void callback(const char *data, int size);      

/**                                                                             
 * @brief This function allocates a block of memory of the spesified size.      
 *                                                                              
 * @param size for number of bytes to allocate.                                 
 */                                                                             
void *util_malloc(size_t size);

/**                                                                             
 * @brief This function will creates a copy of memory block.                    
 *                                                                              
 * @param src   A pointer to source memory block.                               
 * @param size  The size of the memory block.                                   
 */                                                                             
void *util_memdup(const void *src, size_t size);    
#endif


